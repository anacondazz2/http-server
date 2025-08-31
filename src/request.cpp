#include "../include/fd.hpp"
#include "../include/response.hpp"
#include "../include/utils.hpp"
#include <array>
#include <fcntl.h>
#include <filesystem>
#include <regex>
#include <string>
#include <sys/socket.h>

namespace fs = std::filesystem;

void handle_request(int client_fd, const fs::path &webroot) {
  Fd client{client_fd};
  std::string request;
  std::array<char, 4096> buf{};

  // Read until end of headers.
  // Reads in 4KB chunks, technically no limit on request size...
  size_t header_end = std::string::npos;
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
    return;
  }
  std::string method = m[1].str();
  std::string path = url_decode(m[2].str());
  if (path.empty() || path == "/")
    path = "/index.html";

  // Sanitize path...
  fs::path requested = fs::path(path).is_absolute()
                           ? fs::path(path).relative_path()
                           : fs::path(path);
  fs::path resolved = fs::weakly_canonical(webroot / "public" / requested);
  if (resolved.string().rfind(webroot.string(), 0) != 0) {
    respond_404(client);
    return;
  }

  // Dispatch HTTP request based on method...
  // --- GET ---
  if (method == "GET") {
    int fd = ::open(resolved.c_str(), O_RDONLY);
    if (fd < 0) {
      respond_404(client);
      return;
    }
    Fd file{fd};
    off_t size = file_size_of(file);
    if (size < 0) {
      respond_404(client);
      return;
    }
    std::string mime = mime_from_ext(ext_of(resolved));
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + mime +
                         "\r\nContent-Length: " + std::to_string(size) +
                         "\r\nConnection: close\r\n\r\n";
    if (!send_all(client, header))
      return;
    send_file(client, file);
    return;
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
      return;
    }
    Fd file{fd};
    ::write(file, body.data(), body.size());

    send_all(client, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK");
    return;
  }
  // --- DELETE ---
  else if (method == "DELETE") {
    if (::unlink(resolved.c_str()) != 0) {
      respond_404(client);
    } else {
      send_all(client, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK");
    }
    return;
  } else
    respond_404(client);
}
