// ./steps/step007.c
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int arg, const char *argv[])
{
    char               buffer[BUFFER_SIZE];
    struct sockaddr_in host_addr;
    unsigned int       host_addrlen;
    const char         resp[] = "HTTP/1.0 200 OK\r\n"
                                "Server: webserver-c\r\n"
                                "Content-type: text/html\r\n\r\n"
                                "<html>hello, world</html>\r\n";
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

        // Read from the socket
        valread = read(newsockfd, buffer, BUFFER_SIZE);
        if(valread < 0)
        {
            perror("webserver (read)");
            continue;
        }
        printf("[%s:%u]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Write to the socket
        valwrite = write(newsockfd, resp, strlen(resp));
        if(valwrite < 0)
        {
            perror("webserver (write)");
            continue;
        }
        close(newsockfd);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunreachable-code-return"
    return 0;
#pragma GCC diagnostic pop
}
