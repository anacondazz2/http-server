#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

std::string mime_from_ext(std::string_view ext);

std::string ext_of(const fs::path &p);

off_t file_size_of(int fd);

std::string url_decode(std::string_view s);
