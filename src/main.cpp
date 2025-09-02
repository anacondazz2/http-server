#include "../include/fd.hpp"
#include "../include/request.hpp"
#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem; // module path alias
constexpr int PORT = 8081;

int main(int argc, char **argv) {
  std::signal(SIGPIPE, SIG_IGN);
  fs::path webroot = fs::current_path();
  if (argc > 1)
    webroot = fs::path(argv[1]);
  webroot = fs::weakly_canonical(webroot / "public");

  // Create IPv6 TCP socket
  Fd server{::socket(AF_INET6, SOCK_STREAM, 0)};
  if (server < 0) {
    perror("socket() failed");
    return 1;
  }

  // Enable dual-stack (IPv4 + IPv6) on Linux by disabling "IPv6 Only"
  int no = 0;
  if (::setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) < 0) {
    return 1;
  }

  // Make socket "resusable"; allows quickly restarting server on same port
  int yes = 1;
  if (::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    perror("setsockopt SO_REUSEADDR failed");
    return 1;
  }

  // Prepare IPv6 address structure and bind it to the socket
  sockaddr_in6 addr6{};
  addr6.sin6_family = AF_INET6;
  addr6.sin6_addr =
      in6addr_any; // [::1] (loopback), fe08:: (local), 2000:: (public)
  addr6.sin6_port = htons(PORT);

  if (::bind(server, (sockaddr *)&addr6, sizeof(addr6)) < 0) {
    perror("bind() failed");
    return 1;
  }
  if (::listen(server, 64) < 0) {
    perror("listen() failed");
    return 1;
  }

  std::cout << "Serving " << webroot << " on port " << PORT << '\n';

  while (true) {
    sockaddr_in caddr{};
    socklen_t clen = sizeof(caddr);
    int cfd = ::accept(server, (sockaddr *)&caddr, &clen);
    if (cfd < 0) {
      perror("accept() failed");
      continue;
    }
    std::thread([cfd, webroot] { handle_request(cfd, webroot); }).detach();
  }

  return 0;
}
