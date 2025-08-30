#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <string_view>
#include <thread>

using namespace std::literals;
namespace fs = std::filesystem; // module path alias

constexpr int PORT = 80;
constexpr size_t CHUNK = 64 * 1024; // 64 KiB

struct Fd {
  int fd{-1};
  Fd() = default;
  explicit Fd(int f) noexcept : fd(f) {} // fd(f) is initialization of `fd`
  Fd(const Fd &) = delete;
  Fd &operator=(const Fd &) = delete;
  Fd(Fd &&o) noexcept
      : fd(std::__exchange(o.fd, -1)) {} // exhange returns the old value
  Fd &operator=(Fd &&o) noexcept {
    if (this != &o) {
      close();
      fd = std::__exchange(o.fd, -1);
    }
    return *this;
  }
  ~Fd() { close(); } // close() here actually refers to close() below
  void close() {
    if (fd >= 0)
      ::close(fd), fd = -1; // :: go straight to global namespace
  }
  operator int() const { return fd; }
};

// Extract mime from extension 'ext'.
static std::string mime_from_ext(std::string_view ext) {
  static const std::map<std::string, std::string, std::less<>> table{
      {"html", "text/html"}, {"htm", "text/html"},   {"txt", "text/plain"},
      {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"}, {"png", "image/png"}};
  if (auto it = table.find(std::string(ext)); it != table.end())
    return it->second;
  return "application/octet-stream";
}

// Get extension of filepath 'p'.
static std::string ext_of(const fs::path &p) {
  std::string ext = p.extension().string();
  if (!ext.empty() && ext.front() == '.')
    ext.erase(0, 1);
  return ext;
}

// Get file size of file 'fd'.
static off_t file_size_of(int fd) {
  struct stat st{};
  if (::fstat(fd, &st) == -1)
    return -1;
  return st.st_size;
}

// Send string 's' to socket 'sock'.
static bool send_all(int sock, std::string_view s) {
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
static void send_file(int sock, int file_fd) {
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

static void respond_404(int sock) {
  send_all(sock, "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "Content-Length: 13\r\n"
                 "\r\n"
                 "404 Not Found");
}

static std::string url_decode(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size() &&
        std::isxdigit((unsigned char)s[i+1]) &&
        std::isxdigit((unsigned char)s[i+2])) {
      // convert two hex digits
      int hi = std::isdigit(s[i+1]) ? s[i+1] - '0' : std::tolower(s[i+1]) - 'a' + 10;
      int lo = std::isdigit(s[i+2]) ? s[i+2] - '0' : std::tolower(s[i+2]) - 'a' + 10;
      out.push_back(char((hi << 4) | lo));
      i += 2; // skip both hex chars
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

static void handle_request(int client_fd, const fs::path &webroot) {
  // Read from socket into memory.
  Fd client{client_fd};
  std::array<char, 4096> buf{};
  ssize_t got = ::recv(client, buf.data(), buf.size() - 1, 0);
  if (got <= 0)
    return;

  // Get the path from client's request.
  std::string_view req(buf.data(), size_t(got));
  static const std::regex r("^GET ([^ ]*) HTTP/1\\.[01]");
  std::cmatch m;
  if (!std::regex_search(req.begin(), req.end(), m, r)) {
    respond_404(client);
    return;
  }
  std::string path = url_decode(m[1].str());
  if (path.empty() || path == "/")
    path = "/index.html";
  
  // Prevent accidental access to root paths.
  fs::path requested = fs::path(path).is_absolute()
    ? fs::path(path).relative_path()
    : fs::path(path);

  fs::path resolved = fs::weakly_canonical(webroot / requested);
  if (resolved.string().rfind(webroot.string(), 0) != 0) {  // check if weakly_canonical failed
    respond_404(client);
    return;
  }

  // Open requested file and send it.
  int fd = ::open(resolved.c_str(), O_RDONLY);
  if (fd == -1) {
    respond_404(client);
    return;
  }
  Fd file{fd};
  
  auto size = file_size_of(file);
  if (size < 0) {
    respond_404(client);
    return;
  }
  
  auto mime = mime_from_ext(ext_of(resolved));
  std::string header;
  header.reserve(256);
  header += "HTTP/1.1 200 OK\r\n";
  header += "Content-Type: " + mime + "\r\n";
  header += "Content-Length: " + std::to_string(size) + "\r\n";
  header += "Connection: close\r\n\r\n";

  if (!send_all(client, header)) return;
  send_file(client, file);
}

int main(int argc, char **argv) {
  std::signal(SIGPIPE, SIG_IGN);
  fs::path webroot = fs::current_path();
  if (argc > 1)
    webroot = fs::path(argv[1]);
  webroot = fs::weakly_canonical(webroot);

  Fd server{::socket(AF_INET, SOCK_STREAM, 0)};
  if (server < 0) {
    perror("socket() failed");
    return 1;
  }

  int yes = 1;
  ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(PORT);

  if (::bind(server, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind() failed");
    return 1;
  }
  if (::listen(server, 128) < 0) {
    perror("listen() failed");
    return 1;
  }

  std::cout << "Serving " << webroot << " on port " << PORT << '\n';

  while(true) {
    sockaddr_in caddr{};
    socklen_t clen = sizeof(caddr);
    int cfd = ::accept(server, (sockaddr*)&caddr, &clen);
    if (cfd < 0) {
      perror("accept() failed");
      continue;
    }
    std::thread([cfd, webroot] {handle_request(cfd, webroot); }).detach();
  }
}
