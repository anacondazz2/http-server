#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <array>
#include <string_view>

constexpr int CHUNK = 64 * 1024;

// Send string 's' to socket 'sock'.
bool send_all(int sock, std::string_view s) {
  const char *p = s.data();
  size_t n = s.size();
  while (n) {
    ssize_t bytes_sent = ::send(sock, p, n, 0);
    // Note. although we request kernel to send all n bytes, it may send less
    //       so the while loop is needed.
    if (bytes_sent <= 0)
      return false;
    p += bytes_sent;
    n -= size_t(bytes_sent);
  }
  return true;
}

// Send the file 'file_fd' to socket 'sock'.
void send_file(int sock, int file_fd) {
  std::array<char, CHUNK> buf{};
  // Send 'CHUNK' number of bytes of the file at a time.
  while (true) {
    // Wrap the read() in a while(1) in case all buf.size() bytes are not read
    // in one go
    ssize_t bytes_read = ::read(file_fd, buf.data(), buf.size());
    if (bytes_read <= 0)
      break;
    const char *p = buf.data();
    size_t n = size_t(bytes_read);
    while (n) {
      ssize_t bytes_sent = send(sock, p, n, 0);
      if (bytes_sent <= 0)
        break;
      p += bytes_sent;
      n -= size_t(bytes_sent);
    }
  }
}

void respond_404(int sock) {
  send_all(sock, "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 13\r\n"
                 "\r\n"
                 "404 Not Found");
}
