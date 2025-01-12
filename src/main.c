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
#define HTTP_CONTENT_TYPE "Content-Type: text/html\r\n"
#define HTTP_CONTENT_SIZE 22
#define REQ_HEADER_LEN 5
#define PATH_LEN 1024

// Priorities:
//  Serve different file types
//  Construct response correctly (make a struct?)
//  Handle incorrect requests
//  Handle GET and HEAD
//  Implement multiplexing or threads

int  write_to_client(int newsockfd, const char *ok_msg, const char *content_type, const char *content_length, unsigned long content_length_size);
int  is_get_request(const char *req_header);
int  is_head_request(const char *req_header);
void set_request_path(char *req_path, const char *buffer);

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
            perror("webserver (request header)");
            continue;
        }

        // get the / to the white space from the buffer and put it in req_path
        set_request_path(req_path, buffer);

        // TODO: if or switch statements for different kinds of acceptable requests and what they should return
        valwrite = write_to_client(newsockfd, HTTP_OK, HTTP_CONTENT_TYPE, "Content-Length: 25\r\n\r\n", HTTP_CONTENT_SIZE);
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

int write_to_client(int newsockfd, const char *ok_msg, const char *content_type, const char *content_length, unsigned long content_length_size)
{
    char          response_string[BUFFER_SIZE];
    char          content_string[BUFFER_SIZE];
    int           file_fd;
    ssize_t       valread;
    ssize_t       valwrite;
    char          c;
    unsigned long length = 0;

    // if valread is a GET request for html
    // serve the html file
    strncpy(response_string, ok_msg, strlen(ok_msg));
    strncat(response_string, content_type, strlen(content_type));
    // 25 is the length of the hello world html file
    strncat(response_string, content_length, content_length_size);
    // get the content of the file index.html with fd and put it in resp
    file_fd = open("./resources/index.html", O_RDONLY | O_CLOEXEC);
    if(file_fd == -1)
    {
        perror("webserver (open)");
        return -1;
    }
    printf("reading from html file\n");
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
    strncat(response_string, content_string, length);
    strncat(response_string, "\r\n", 2);
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
