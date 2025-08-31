#pragma once
#include <unistd.h>
#include <utility>

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
