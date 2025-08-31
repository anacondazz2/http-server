#pragma once
#include <filesystem>

namespace fs = std::filesystem;

void handle_request(int client_fd, const fs::path &webroot);
