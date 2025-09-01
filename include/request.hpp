#pragma once
#include <filesystem>

namespace fs = std::filesystem;

void handle_request(int client_fd, const fs::path &webroot);

void handle_dir(int client, std::string path, fs::path resolved);
