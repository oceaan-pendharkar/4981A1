#ifndef MAIN_H
#define MAIN_H

// ./steps/step007.c
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

#define HTTP_OK "HTTP/1.0 200 OK\r\n"
#define HTTP_NOT_FOUND "HTTP/1.0 404 Not Found\r\n"
#define HTTP_BAD_REQUEST "HTTP/1.0 400 Bad Request\r\n"
#define HTTP_METHOD_NOT_ALLOWED "HTTP/1.0 405 Method Not Allowed\r\nAllow: GET, HEAD\r\n"

#define HTML_CONTENT_TYPE "Content-Type: text/html\r\n"
#define TEXT_CONTENT_TYPE "Content-Type: text/plain\r\n"
#define CSS_CONTENT_TYPE "Content-Type: text/css\r\n"
#define JS_CONTENT_TYPE "Content-Type: text/javascript\r\n"
#define JPEG_CONTENT_TYPE "Content-Type: image/jpeg\r\n"
#define PNG_CONTENT_TYPE "Content-Type: image/png\r\n"
#define GIF_CONTENT_TYPE "Content-Type: image/gif\r\n"

#define REQ_HEADER_LEN 8
#define PATH_LEN 1024
#define CONTENT_LEN_BUF 100
#define CONTENT_TERM_LEN 5
#define TEN 10
#define LEN_405 9
#define FILE_EXT_LEN 5
#define SIZE_404_MSG 20
#define INDEX_FILE_PATH "/index.html"

// don't need html because it's the default
#define JS_EXT "sj"
#define CSS_EXT "ssc"
#define JPG_EXT "gpj"
#define JPEG_EXT "gepj"
#define PNG_EXT "gnp"
#define GIF_EXT "fig"
#define TXT_EXT "txt"

static volatile sig_atomic_t exit_flag = 0;    // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// FILE_PATH_LEN is the length we have to append to include ./resources to the path
// we want to open the file we're serving at
#if(defined(__APPLE__) && defined(__MACH__))
    #define FILE_PATH_LEN 11
#endif

#if defined(__linux__)
    #define FILE_PATH_LEN 12
#endif

// Priorities:
//  Serve different file types
//  Handle errors listed on assignment
//  Implement multiplexing or threads

static void setup_signal_handler(void);
static void sigint_handler(int signum);
void        set_request_method(char *req_header, const char *buffer);
int         is_head_request(const char *req_header);
int         is_get_request(const char *req_header);
int is_img_request(const char *buffer);
int         is_http_request(const char *req_header, const char *buffer);
void        set_request_path(char *req_path, const char *buffer);
int         handle_client(int newsockfd, const char *request_path, int is_head, int is_img);
static void socket_close(int sockfd);
int         has_valid_first_line(const char *buffer);
int         has_valid_headers(const char *buffer);
int         write_to_content_string(char **content_string, unsigned long *length, const char *file_path);
int write_to_content_binary(char **content_string, unsigned long *length, const char *file_path);
int         write_to_client(int newsockfd, char *response_string);
void        set_content_type_from_file_extension(const char *request_path, char *content_type_string);
void        append_msg_to_response_string(char *response, const char *msg);
void        append_content_length_msg(char *response_string, unsigned long length);
void        append_body(char *response_string, const char *content_string, unsigned long length);
void        int_to_string(char *string, unsigned long n);
void        open_file_at_path(const char *request_path, int *file_fd, struct stat *file_stats);

#endif    // MAIN_H
