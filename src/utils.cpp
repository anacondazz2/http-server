#include <filesystem>
#include <map>
#include <string>
#include <sys/stat.h>

namespace fs = std::filesystem; // module path alias

// Extract mime from extension 'ext'.
std::string mime_from_ext(std::string_view ext) {
  if (ext.empty())
    return "text/plain";

  static const std::map<std::string, std::string, std::less<>> table{
      {"html", "text/html"}, {"htm", "text/html"},   {"txt", "text/plain"},
      {"jpg", "image/jpeg"}, {"jpeg", "image/jpeg"}, {"png", "image/png"},
      {"css", "text/css"}, {"mov", "video/quicktime"}, {"mp4", "video/mp4"}};
  if (auto it = table.find(std::string(ext)); it != table.end())
    return it->second;
  return "application/octet-stream";
}

// Get extension of filepath 'p'.
std::string ext_of(const fs::path &p) {
  std::string ext = p.extension().string();
  if (!ext.empty() && ext.front() == '.')
    ext.erase(0, 1);
  return ext;
}

// Get file size of file 'fd'.
off_t file_size_of(int fd) {
  struct stat st{};
  if (::fstat(fd, &st) == -1)
    return -1;
  return st.st_size;
}

// Decode url encodings.
std::string url_decode(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size() &&
        std::isxdigit((unsigned char)s[i + 1]) &&
        std::isxdigit((unsigned char)s[i + 2])) {
      // Convert two hex digits
      int hi = std::isdigit(s[i + 1]) ? s[i + 1] - '0'
                                      : std::tolower(s[i + 1]) - 'a' + 10;
      int lo = std::isdigit(s[i + 2]) ? s[i + 2] - '0'
                                      : std::tolower(s[i + 2]) - 'a' + 10;
      out.push_back(char((hi << 4) | lo));
      i += 2; // skip both hex chars
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}
