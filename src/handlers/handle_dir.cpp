#include "../../include/response.hpp"
#include <filesystem>
#include <string>
#include <sys/socket.h>

namespace fs = std::filesystem;

void handle_dir(int client, std::string path, fs::path resolved) {
  std::string listing =
      "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Index "
      "of " +
      path +
      "</title>"
      "<style>"
      "body{font-family:monospace;}"
      "a{text-decoration:none;color:#000;}"
      "a:hover{text-decoration:underline;}"
      "table{border-collapse:collapse;width:100%;}"
      "th,td{padding:0.25rem;}"
      "th{text-align:left;}"
      "td.size{text-align:left;}"
      "</style>"
      "</head><body class=\"dir-listing\">"
      "<h1>Index of " +
      path +
      "</h1>"
      "<table>"
      "<tr><th>Name</th><th>Size</th><th>Modified</th></tr>";

  // Parent directory link
  fs::path parent = fs::path(path).parent_path().parent_path();
  // printf("path: %s\n", path.c_str());
  // printf("parent: %s\n", parent.c_str());
  std::string href = parent.string();
  if (!href.ends_with('/'))
    href += '/';
  listing += "<tr>"
             "<td>📁 <a href=\"" +
             href +
             "\">..</a></td>"
             "<td class=\"size\">-</td>"
             "<td>-</td>"
             "</tr>";

  for (auto &entry : fs::directory_iterator(resolved)) {
    std::string entry_name = entry.path().filename().string();
    std::string href = path;
    if (!href.ends_with('/'))
      href += '/'; // href is currently the previous directory, not the current
                   // entry
    href += entry_name;
    if (fs::is_directory(entry.path()))
      href += '/';

    std::string icon = fs::is_directory(entry.path()) ? "📁" : "📄";

    // File size
    std::string size_str = "-";
    if (fs::is_regular_file(entry.path())) {
      auto size = fs::file_size(entry.path());
      if (size < 1024)
        size_str = std::to_string(size) + " B";
      else if (size < 1024 * 1024)
        size_str = std::to_string(size / 1024) + " KB";
      else
        size_str = std::to_string(size / (1024 * 1024)) + " MB";
    }

    // Last modified time
    auto ftime = fs::last_write_time(entry.path());
    auto sctp =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    std::stringstream time_ss;
    time_ss << std::put_time(std::localtime(&cftime), "%Y-%m-%d %H:%M:%S");
    std::string mod_time = time_ss.str();

    listing += "<tr>"
               "<td>" +
               icon + " <a href=\"" + href + "\">" + entry_name +
               "</a></td>"
               "<td class=\"size\">" +
               size_str +
               "</td>"
               "<td>" +
               mod_time +
               "</td>"
               "</tr>";
  }
  listing += "</table></body></html>";

  // Prepare header to send back
  std::string header =
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
      std::to_string(listing.size()) + "\r\nConnection: keep-alive\r\n\r\n";
  send_all(client, header);
  send_all(client, listing);
}
