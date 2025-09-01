#include "../include/request.hpp"
#include "../include/fd.hpp"
#include "../include/response.hpp"
#include "../include/utils.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <sys/socket.h>

#define LOG_ERR(msg)                                                           \
  std::cerr << "[" << __FILE__ << ": " << __LINE__ << "]" << msg << ": "       \
            << strerror(errno) << '\n';

namespace fs = std::filesystem;

void handle_request(int client_fd, const fs::path &webroot) {
  Fd client{client_fd};
  std::array<char, 4096> buf{};
  std::string leftover;

  // Assume connection: keep-alive unless client says otherwise...
  while (true) {
    std::string request = leftover;
    size_t header_end = std::string::npos;

    // Read until end of headers.
    // Reads in 4KB chunks, technically no limit on request size...
    while (true) {
      ssize_t got = ::recv(client, buf.data(), buf.size(), 0);
      if (got <= 0)
        return; // client closed or error
      request.append(buf.data(), got);

      header_end = request.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        header_end += 4;
        break;
      }
    }

    // Check if client wants close...
    bool client_wants_close = false;
    if (std::regex_search(request, std::regex("Connection: close",
                                              std::regex_constants::icase))) {
      client_wants_close = true;
    }

    // Extract the method and path from headers.
    std::string_view req(request);
    static const std::regex r("^(GET|POST|PUT|DELETE) ([^ ]*) HTTP/1\\.[01]");
    std::cmatch m;
    if (!std::regex_search(req.begin(), req.begin() + header_end, m, r)) {
      respond_404(client);
      if (client_wants_close)
        return;
      else
        continue;
    }
    std::string method = m[1].str();
    std::string path = url_decode(m[2].str());
    if (path.empty() || path == "/")
      path = "/index.html";

    // Sanitize path...
    fs::path requested = fs::path(path).is_absolute()
                             ? fs::path(path).relative_path()
                             : fs::path(path);
    fs::path resolved = fs::weakly_canonical(webroot / requested);
    if (resolved.string().rfind(webroot.string(), 0) != 0) {
      LOG_ERR("Path does not begin with webroot...");
      respond_404(client);
      if (client_wants_close)
        return;
      else
        continue;
    }

    // printf("webroot: %s\n", webroot.c_str());
    // printf("requested: %s\n", requested.c_str());
    // printf("resolved: %s\n\n", resolved.c_str());

    // Dispatch HTTP request based on method...
    // --- GET ---
    if (method == "GET") {
      if (fs::is_directory(resolved)) {
        handle_dir(client, path, resolved);
      } else {
        // Send static content
        int fd = ::open(resolved.c_str(), O_RDONLY);
        if (fd < 0) {
          LOG_ERR("open() failed");
          respond_404(client);
          if (client_wants_close)
            return;
          else
            continue;
        }
        Fd file{fd};
        off_t size = file_size_of(file);
        if (size < 0) {
          LOG_ERR("file_size_of() failed");
          respond_404(client);
          if (client_wants_close)
            return;
          else
            continue;
        }
        std::string mime = mime_from_ext(ext_of(resolved));
        std::string accessToken =
            "placeholder"; // just to desmonstrate use of cookies
                           // and CSRF measures (SameSite=Strict)
        std::string refreshToken = "placeholder";
        std::string header = "HTTP/1.1 200 OK"
                             "\r\nContent-Type: " +
                             mime +
                             "\r\nContent-Length: " + std::to_string(size) +
                             "\r\nConnection: keep-alive"
                             "\r\nSet-Cookie: accessToken=" +
                             accessToken +
                             "; HttpOnly; SameSite=Strict; Path=/"
                             "\r\nSet-Cookie: refreshToken=" +
                             refreshToken +
                             "; HttpOnly; SameSite=Strict; Path=/"
                             "\r\n\r\n";
        if (!send_all(client, header)) {
          LOG_ERR("send_all() failed");
          return;
        }
        send_file(client, file);

        // Store leftovers...
        size_t request_end = request.size();
        leftover = (request_end > header_end) ? request.substr(header_end) : "";
      }
    }
    // --- POST / PUT ---
    else if (method == "POST" || method == "PUT") {
      // Parse content length...
      std::smatch cl_match;
      size_t content_length = 0;
      if (std::regex_search(request, cl_match,
                            std::regex("Content-Length: ([0-9]+)",
                                       std::regex_constants::icase))) {
        content_length = std::stoul(cl_match[1]);
      }

      // Read remaining body if needed...
      size_t body_have = request.size() - header_end;
      while (body_have < content_length) {
        ssize_t got = ::recv(client, buf.data(), buf.size(), 0);
        if (got <= 0)
          return;
        request.append(buf.data(), got);
        body_have = request.size() - header_end;
      }
      std::string body = request.substr(header_end, content_length);

      // Only allow changes to webroot/anacondazz2
      // Note that directory traversal attack (..) would not reach here
      // as sanitization above backs out early.
      resolved = fs::weakly_canonical(webroot / "anacondazz2" / requested);
      // printf("Resolved path: %s\n", resolved.c_str());

      // Create parent directories if neeeded
      fs::path parent = resolved.parent_path();
      std::error_code ec;
      if (!fs::exists(parent) && !fs::create_directories(parent, ec)) {
        LOG_ERR("Failed to create directories: ");
        std::cerr << parent << " (" << ec.message() << ")\n";
        respond_404(client);
        if (client_wants_close)
          return;
        else
          continue;
      }

      int fd = ::open(resolved.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd < 0) {
        LOG_ERR("open() failed");
        respond_404(client);
        if (client_wants_close)
          return;
        else
          continue;
      }
      Fd file{fd};
      ::write(file, body.data(), body.size());

      if (!send_all(client, "HTTP/1.1 200 OK\r\nContent-Length: 2"
                            "\r\nAccess-Control-Allow-Origin: bmac678.ca"
                            "\r\n\r\nOK")) {

        LOG_ERR("send_all() failed"); // likely some network error, should close
                                      // connection immediately...
        return;
      }

      // Store leftovers...
      size_t body_end = request.size();
      leftover = (body_end > (header_end + content_length))
                     ? request.substr(header_end + content_length)
                     : "";
    }
    // --- DELETE ---
    else if (method == "DELETE") {
      // Only allow changes to webroot/anacondazz2
      resolved = fs::weakly_canonical(webroot / "anacondazz2" / requested);
      std::error_code ec;
      if (fs::is_directory(resolved, ec)) {
        fs::remove_all(resolved, ec); // recursively delete directory
      } else {
        fs::remove(resolved, ec); // just delete the file
      }

      if (ec) {
        LOG_ERR("Failed to delete: ");
        std::cerr << resolved << " -> " << ec.message() << '\n';
        respond_404(client);
        if (client_wants_close)
          return;
        else
          continue;
      }

      if (!send_all(client, "HTTP/1.1 200 OK\r\nContent-Length: 2"
                            "\r\nAccess-Control-Allow-Origin: bmac678.ca"
                            "\r\n\r\nOK")) {
        LOG_ERR("send_all() failed");
        return;
      }
    } else
      respond_404(client);

    if (client_wants_close)
      return; // else, continue to
              // next request...
  }
}
