#include "../include/fd.hpp"
#include "../include/response.hpp"
#include "../include/utils.hpp"
#include <array>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <pthread.h>
#include <regex>
#include <string>
#include <sys/socket.h>

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

    // Extract the method and path from headers.
    std::string_view req(request);
    static const std::regex r("^(GET|POST|PUT|DELETE) ([^ ]*) HTTP/1\\.[01]");
    std::cmatch m;
    if (!std::regex_search(req.begin(), req.begin() + header_end, m, r)) {
      respond_404(client);
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
      respond_404(client);
    }

    // printf("Requested path: %s\n", path.c_str());
    // printf("Resolved path: %s\n", resolved.c_str());

    // Check if client wants close...
    bool client_wants_close = false;
    if (std::regex_search(request, std::regex("Connection: close",
                                              std::regex_constants::icase))) {
      client_wants_close = true;
    }

    // Dispatch HTTP request based on method...
    // --- GET ---
    if (method == "GET") {
      // Directory listing
      if (fs::is_directory(resolved)) {
        std::string listing =
            "<!DOCTYPE html><html><head><title>Index of " + path +
            "</title>"
            "<style>body{font-family:monospace;}a{text-decoration:none;color:#"
            "000;}a:hover{text-decoration:underline;}ul{list-style:none;"
            "padding-"
            "left:0;}li{margin:0.25rem 0;}</style>"
            "</head><body class=\"dir-listing\">"
            "<h1>Index of " +
            path + "</h1><ul>";

        if (path != "/") {
          fs::path parent = fs::path(path).parent_path();
          listing += "<li><a href=\"" + parent.string();
          listing +=
              (parent.string() == "/") ? "\">..</a></li>" : "/\">..</a></li>";
        }

        for (auto &entry : fs::directory_iterator(resolved)) {
          std::string name = entry.path().filename().string();
          std::string href = path;
          if (!href.ends_with('/'))
            href += '/';
          href += name;
          listing += "<li><a href=\"" + href + "\">" + name + "</a></li>";
        }

        listing += "</ul></body></html>";
        std::string header =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
            std::to_string(listing.size()) +
            "\r\nConnection: keep-alive\r\n\r\n";
        send_all(client, header);
        send_all(client, listing);
      } else {
        // Send static content
        int fd = ::open(resolved.c_str(), O_RDONLY);
        if (fd < 0) {
          respond_404(client);
        }
        Fd file{fd};
        off_t size = file_size_of(file);
        if (size < 0) {
          respond_404(client);
        }
        std::string mime = mime_from_ext(ext_of(resolved));
        std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + mime +
                             "\r\nContent-Length: " + std::to_string(size) +
                             "\r\nConnection: keep-alive\r\n\r\n";
        if (!send_all(client, header))
          return;
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
      int fd = ::open(resolved.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (fd < 0) {
        respond_404(client);
      }
      Fd file{fd};
      ::write(file, body.data(), body.size());

      send_all(client, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK");

      // Store leftovers...
      size_t body_end = request.size();
      leftover = (body_end > (header_end + content_length))
                     ? request.substr(header_end + content_length)
                     : "";
    }
    // --- DELETE ---
    else if (method == "DELETE") {
      if (::unlink(resolved.c_str()) != 0) {
        respond_404(client);
      } else {
        send_all(client, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK");
      }
    } else
      respond_404(client);

    if (client_wants_close)
      return; // else, continue to next request...
  }
}
