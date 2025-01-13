// ./steps/step007.c
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define HTTP_OK "HTTP/1.0 200 OK\r\n"
#define HTTP_NOT_FOUND "HTTP/1.0 404 Not Found\r\n"

//TODO: create switch statement and generate dynamically
#define HTTP_CONTENT_TYPE "Content-Type: text/html\r\n"
#define REQ_HEADER_LEN 5
#define PATH_LEN 1024
#define CONTENT_LEN_BUF 100
#define TEN 10

// FILE_PATH_LEN is the length we have to append to include ./resources to the path
// we want to open the file we're serving at
#define FILE_PATH_LEN 11

// Priorities:
//  Serve different file types
//  Handle incorrect requests
//  Handle GET and HEAD
//  Implement multiplexing or threads

int  handle_client(int newsockfd, const char *request_path);
int  is_get_request(const char *req_header);
int  is_head_request(const char *req_header);
void set_request_path(char *req_path, const char *buffer);
void int_to_string(char *string, unsigned long n);
void open_file_at_path(const char *request_path, int *file_fd);
void append_msg_to_response_string(char *response, const char *msg);
void append_content_length_msg(char *response_string, unsigned long length);
void append_body(char *response_string, const char *content_string, unsigned long length);
int  write_to_client(int file_fd, int newsockfd, const char *response_string);

int main(int arg, const char *argv[])
{
    char               buffer[BUFFER_SIZE];
    struct sockaddr_in host_addr;
    unsigned int       host_addrlen;

    // Create client address
    struct sockaddr_in client_addr;
    int                client_addrlen = sizeof(client_addr);

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);    // NOLINT(android-cloexec-socket)
    if(sockfd == -1)
    {
        perror("webserver (socket)");
        return 1;
    }
    printf("socket created successfully\n");
    printf("%d\n", arg);
    printf("%s\n", argv[0]);

    // Create the address to bind the socket to

    host_addrlen = sizeof(host_addr);

    host_addr.sin_family      = AF_INET;
    host_addr.sin_port        = htons(PORT);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the address
    if(bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0)
    {
        perror("webserver (bind)");
        close(sockfd);
        return 1;
    }
    printf("socket successfully bound to address\n");

    // Listen for incoming connections
    if(listen(sockfd, SOMAXCONN) != 0)
    {
        perror("webserver (listen)");
        close(sockfd);
        return 1;
    }
    printf("server listening for connections\n");

    while(1)
    {
        int     sockn;
        ssize_t valread;
        ssize_t valwrite;
        char    req_header[REQ_HEADER_LEN + 1];
        char    req_path[PATH_LEN];

        // Accept incoming connections
        int newsockfd = accept(sockfd, (struct sockaddr *)&host_addr, (socklen_t *)&host_addrlen);
        if(newsockfd < 0)
        {
            perror("webserver (accept)");
            continue;
        }
        printf("connection accepted\n");

        // Get client address
        sockn = getsockname(newsockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addrlen);
        if(sockn < 0)
        {
            perror("webserver (getsockname)");
            continue;
        }

        // Read from the socket: this is the request
        valread = read(newsockfd, buffer, BUFFER_SIZE);
        if(valread < 0)
        {
            perror("webserver (read)");
            continue;
        }
        printf("[%s:%u]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        printf("buffer: %s\n", buffer);

        // check that it is a GET or HEAD request
        strncpy(req_header, buffer, REQ_HEADER_LEN - 1);
        req_header[REQ_HEADER_LEN] = '\0';
        printf("req_header: %s\n", req_header);
        if(is_get_request(req_header) < 0 && is_head_request(req_header) < 0)
        {
            perror("webserver (wrong request type)");    // our server only handles GET and HEAD
            continue;
        }

        // gets the substring from the / to the white space from the buffer and put it in req_path
        // this is the path of the file the request wants to access
        set_request_path(req_path, buffer);

        valwrite = handle_client(newsockfd, req_path);
        if(valwrite == -1)
        {
            continue;
        }

        printf("closing connection\n");
        close(newsockfd);
    }

#if defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunreachable-code-return"
#elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunreachable-code"
#endif

    return 0;

#pragma GCC diagnostic pop
}

int handle_client(int newsockfd, const char *request_path)
{
    char          response_string[BUFFER_SIZE];
    char          content_string[BUFFER_SIZE];
    int           file_fd;
    ssize_t       valread;
    char          c;
    unsigned long length = 0;

    // get the content of the file with file_fd
    if(strcmp(request_path, "/") == 0)
    {
        file_fd = open("./resources/index.html", O_RDONLY | O_CLOEXEC);
    }
    else
    {
        open_file_at_path(request_path, &file_fd);
    }
    if(file_fd == -1)
    {
        // This means the file requested was not found/openable
        append_msg_to_response_string(response_string, HTTP_NOT_FOUND);
        file_fd = open("./resources/404.html", O_RDONLY | O_CLOEXEC);
        if(file_fd == -1)
        {
            perror("webserver (open)");
            return -1;
        }
    }
    else
    {
        // This means the file was found so we will return 200
        append_msg_to_response_string(response_string, HTTP_OK);
    }

    printf("reading from file\n");
    while((valread = read(file_fd, &c, 1)) > 0 && c != '\0')
    {
        // write c to response_string
        content_string[length++] = c;
    }
    if(valread < 0)
    {
        perror("webserver (read)");
        close(file_fd);
        return -1;
    }
    content_string[length] = '\0';

    // append content type line
    // only deals with http content type right now
    strncat(response_string, HTTP_CONTENT_TYPE, strlen(HTTP_CONTENT_TYPE));

    // append content length section (can only do this once we have the body)
    // but must be appended before the body
    append_content_length_msg(response_string, length);

    // append body section (TODO: don't do this if it's a HEAD req)
    append_body(response_string, content_string, length);

    // write to client
    return write_to_client(file_fd, newsockfd, response_string);
}

int is_get_request(const char *req_header)
{
    if(strcmp(req_header, "GET ") == 0)
    {
        return 0;
    }
    return -1;
}

int is_head_request(const char *req_header)
{
    if(strcmp(req_header, "HEAD") == 0)
    {
        return 0;
    }
    return -1;
}

void set_request_path(char *req_path, const char *buffer)
{
    char c;
    int  i = 0;
    int  j = 0;
    c      = buffer[i];
    while(c != ' ')
    {
        c = buffer[++i];
    }
    c = buffer[++i];
    while(c != ' ')
    {
        req_path[j++] = c;
        c             = buffer[++i];
    }
    req_path[j] = '\0';
    printf("request path: %s\n", req_path);
}

void int_to_string(char *string, unsigned long n)
{
    char          buffer[TEN] = {0};
    int           digits      = 0;
    unsigned long i           = n;

    if(n == 0)
    {
        string[0] = '0';
        string[1] = '\0';
        return;
    }

    while(i > 0)
    {
        buffer[digits++] = (char)((i % TEN) + '0');
        i                = i / TEN;
        printf("%lu\n", i);
    }

    printf("digits: %d\n", digits);
    for(int j = 0; j < digits; j++)
    {
        string[j] = buffer[digits - j - 1];
    }
    string[digits] = '\0';
}

void open_file_at_path(const char *request_path, int *file_fd)
{
    char *path = (char *)malloc(sizeof(char) * (strlen(request_path) + FILE_PATH_LEN + 1));
    strncpy(path, "./resources", FILE_PATH_LEN);
    strncpy(path + FILE_PATH_LEN, request_path, strlen(request_path) + 1);
    printf("file path: %s\n", path);
    *file_fd = open(path, O_RDONLY | O_CLOEXEC);
    free(path);
}

void append_msg_to_response_string(char *response, const char *msg)
{
    strncpy(response, msg, strlen(msg) - 1);
    response[strlen(msg) - 1] = '\0';
}

// This one is special because it has the extra \r\n and needs to be constructed with the appropriate length
void append_content_length_msg(char *response_string, unsigned long length)
{
    char content_len_buffer[CONTENT_LEN_BUF];
    char content_length_msg[BUFFER_SIZE] = "Content-Length: ";
    int_to_string(content_len_buffer, length);
    printf("content length: %s\n", content_len_buffer);
    strncat(content_length_msg, content_len_buffer, strlen(content_len_buffer));
    strncat(content_length_msg, "\r\n\r\n", 4);
    printf("content_length_msg: %s\n", content_length_msg);
    strncat(response_string, content_length_msg, length + 2);
    printf("response string: %s\n", response_string);
}

void append_body(char *response_string, const char *content_string, unsigned long length)
{
    strncat(response_string, content_string, length);
    strncat(response_string, "\r\n", 2);
}

int write_to_client(int file_fd, int newsockfd, const char *response_string)
{
    ssize_t valwrite;
    valwrite = write(newsockfd, response_string, strlen(response_string));
    if(valwrite < 0)
    {
        perror("webserver (write)");
        close(file_fd);
        return -1;
    }
    close(file_fd);
    return 0;
}
