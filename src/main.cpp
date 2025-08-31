#include "../include/request.hpp"
#include "../include/fd.hpp"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>
#include <csignal>
#include <iostream>
#include <thread>

namespace fs = std::filesystem; // module path alias
constexpr int PORT = 80;

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

  return 0;
}
