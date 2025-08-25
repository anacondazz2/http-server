#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 80
#define BUFFER_SIZE 104857600

// --- Get file extension ---
const char *get_file_extension(const char *file_name) {
  const char *dot = strrchr(file_name, '.');
  if (!dot || dot == file_name) {
    return "";
  }
  return dot + 1;
}

// --- Get mime type ---
const char *get_mime_type(const char *file_ext) {
  if (strcasecmp(file_ext, "html") == 0 || strcasecmp(file_ext, "htm") == 0) {
    return "text/html";
  } else if (strcasecmp(file_ext, "txt") == 0) {
    return "text/plain";
  } else if (strcasecmp(file_ext, "jpg") == 0 ||
             strcasecmp(file_ext, "jpeg") == 0) {
    return "image/jpeg";
  } else if (strcasecmp(file_ext, "png") == 0) {
    return "image/png";
  } else {
    return "application/octet-stream";
  }
}

// --- Get file size
off_t get_file_size(int file_fd) {
  struct stat file_stat;
  fstat(file_fd, &file_stat);
  return file_stat.st_size;
}

// --- Build http response ---
void build_http_response(const char *file_name, const char *file_ext,
                         char *response, size_t *response_len) {
  const char *mime_type = get_mime_type(file_ext);
  int file_fd = open(file_name, O_RDONLY);
  if (file_fd == -1) {
    snprintf(response, BUFFER_SIZE,
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/plain\r\n"
             "\r\n"
             "404 Not Found");
    *response_len = strlen(response);
    return;
  }
  off_t file_size = get_file_size(file_fd);

  char *header = (char *)malloc(BUFFER_SIZE);
  snprintf(header, BUFFER_SIZE,
           "HTTP/1.1 200 OK\r\n"  // status line
           "Content-Type: %s\r\n" // headers
           "Content-Length: %ld\r\n"
           "\r\n", // separate header and body with \n
           mime_type, file_size);

  // Copy header and file to response buffer
  *response_len = 0;
  memcpy(response, header, strlen(header));
  *response_len += strlen(header);

  ssize_t bytes_read;
  while ((bytes_read = read(file_fd, response + *response_len,
                            BUFFER_SIZE - *response_len)) > 0) {
    *response_len += bytes_read;
  }
  free(header);
  close(file_fd);
}

// --- Handle client ---
void *handle_client(void *arg) {
  int client_fd = *((int *)arg);
  char *buffer = (char *)malloc(BUFFER_SIZE);

  ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
  if (bytes_received > 0) {
    regex_t regex;
    regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
    regmatch_t matches[2];

    if (regexec(&regex, buffer, 2, matches, 0) == 0) {
      buffer[matches[1].rm_eo] = '\0'; // terminate the buffer early
      /*
      Example:
         buffer = "GET /foo.txt HTTP/1.1"
         capture group = "foo.txt"
         rm_so = 5   (index of 'f')
         rm_eo = 15  (index after 't')
      */
      const char *file_name;
      if (matches[1].rm_eo - matches[1].rm_so == 0) {
        // User requested "/"
        file_name = "index.html"; 
      } else {
        buffer[matches[1].rm_eo] = '\0';
        file_name = buffer + matches[1].rm_so;
      }

      //char file_ext[32]; // consider removing, no need for a copy of file_ext
      //strcpy(file_ext, get_file_extension(file_name));
      const char *file_ext = get_file_extension(file_name);
      char *response = (char *)malloc(BUFFER_SIZE * 2);
      size_t response_len;
      build_http_response(file_name, file_ext, response, &response_len);

      send(client_fd, response, response_len, 0);

      free(response);
    }
    regfree(&regex);
  }
  free(buffer);
  free(arg);
  return NULL;
}

// --- Main ---
int main(int argc, char **argv) {
  int server_fd;
  struct sockaddr_in server_addr;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket() failed");
    exit(EXIT_FAILURE);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("bind() failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 10) < 0) {
    perror("listen() failed");
    exit(EXIT_FAILURE);
  }

  printf("Server is listening on port %d\n", PORT);
  while (1) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int *client_fd = malloc(sizeof(int));

    if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                             &client_addr_len)) < 0) {
      perror("accept() failed");
      continue;
    }

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
    pthread_detach(thread_id);
  }

  close(server_fd);
  return 0;
}
