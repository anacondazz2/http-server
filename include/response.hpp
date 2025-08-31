#pragma once
#include <string_view>

bool send_all(int sock, std::string_view s);

void send_file(int sock, int file_fd);

void respond_404(int sock);
