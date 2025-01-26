#include "main.h"

int main(int arg, const char *argv[])
{
    struct sockaddr_in host_addr;         // Server's address structure
    unsigned int       host_addrlen;      // Length of the server address
    fd_set             readfds;           // Set of file descriptors for select
    size_t             max_clients;       // Maximum number of clients that can connect
    int               *client_sockets;    // Array of active client sockets
    int                sd;                // Temp variable for socket descriptor

    // Create client address
    struct sockaddr_in client_addr;
    int                client_addrlen = sizeof(client_addr);

    // Create a TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);    // NOLINT(android-cloexec-socket)
    if(sockfd == -1)
    {
        perror("webserver (socket)");
        return 1;
    }
    printf("Socket created successfully\n");

    // (Debugging) Print program arguments
    printf("%d\n", arg);
    printf("%s\n", argv[0]);

    // Set up Signal Handler
    setup_signal_handler();

    // Initialize client socket, address and number as zero or null
    client_sockets = NULL;
    max_clients    = 0;
#if defined(__linux__)
    memset(&client_addr, 0, sizeof(client_addr));
#endif

    // Create the address to bind the socket to
    // Initialize the server address structure
    host_addrlen = sizeof(host_addr);

    // Use IPv4 to set the server port and bind to available network interface
    host_addr.sin_family      = AF_INET;
    host_addr.sin_port        = htons(PORT);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket to the server address
    if(bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0)
    {
        perror("webserver (bind)");
        close(sockfd);
        return 1;
    }
    printf("Socket successfully bound to address\n");

    // Listen for incoming connections
    if(listen(sockfd, SOMAXCONN) != 0)
    {
        perror("webserver (listen)");
        close(sockfd);
        return 1;
    }
    printf("Server listening for connections\n\n");

    // Infinite loop to handle client connections
    while(!exit_flag)
    {
        int     max_fd;                            // Maximum file descriptor for select
        int     activity;                          // Number of ready file descriptors
        int     newsockfd;                         // New socket for incoming connection
        int     sockn;                             // Temporary socket descriptor
        ssize_t valread;                           // For read operations
        ssize_t valwrite;                          // For write operations
        char    req_header[REQ_HEADER_LEN + 1];    // Request the header buffer
        char    req_path[PATH_LEN];                // Path of the requested file
        char    buffer[BUFFER_SIZE] = {0};         // Buffer for storing incoming data

        // Flags for HEAD, GET and valid HTTP requests
        int is_head = 0;
        int is_get  = 0;
        int is_img  = 0;
        int is_http = 0;

        // Clear the socket set
#ifndef __clang_analyzer__
        FD_ZERO(&readfds);
#endif

#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
        // Add the server socket to the set
        FD_SET(sockfd, &readfds);
#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif

        // Start with the server socket
        max_fd = sockfd;

        // Add the client sockets to the set
        for(size_t i = 0; i < max_clients; i++)
        {
            sd = client_sockets[i];
            if(sd > 0)
            {
#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
                FD_SET(sd, &readfds);
#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
            }
            if(sd > max_fd)
            {
                max_fd = sd;
            }
        }

        // Wait for activity on one of the monitored sockets
        activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if(activity < 0)
        {
            perror("Select error");
            continue;
        }

        // New Connection
        if(FD_ISSET(sockfd, &readfds))
#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
        {
            int *temp;
            // Accept incoming connections
            newsockfd = accept(sockfd, (struct sockaddr *)&host_addr, (socklen_t *)&host_addrlen);
            if(newsockfd < 0)
            {
                perror("webserver (accept)");
                continue;
            }
            printf("connection accepted\n");

            // Increase the size of the client_sockets array
            max_clients++;
            temp = (int *)realloc(client_sockets, sizeof(int) * max_clients);

            if(temp == NULL)
            {
                perror("realloc");
                free(client_sockets);
                exit(EXIT_FAILURE);
            }
            else
            {
                client_sockets                  = temp;
                client_sockets[max_clients - 1] = newsockfd;
            }

            // Get client address
            sockn = getsockname(newsockfd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addrlen);
            if(sockn < 0)
            {
                perror("webserver (getsockname)");
                continue;
            }
        }

        // Handle incoming data from existing clients
        for(size_t i = 0; i < max_clients; i++)
        {
            sd = client_sockets[i];

#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
            if(FD_ISSET(sd, &readfds))
            {
#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
                // Read from the socket: this is the request
                valread = read(sd, buffer, BUFFER_SIZE);
                if(valread < 0)
                {
                    perror("webserver (read)");
                    close(sd);
#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
                    FD_CLR(sd, &readfds);    // Remove the closed socket from the set
#if defined(__FreeBSD__) && defined(__GNUC__)
    #pragma GCC diagnostic pop
#endif
                    client_sockets[i] = 0;
                }
                else
                {
                    printf("[%s:%u]\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    printf("buffer: %s\n", buffer);

                    // check that it is a GET or HEAD request
                    // read until the space from the buffer
                    set_request_method(req_header, buffer);
                    printf("req_header (method): %s\n", req_header);

                    is_head = is_head_request(req_header);
                    printf("is_head: %d\n", is_head);

                    is_get = is_get_request(req_header);
                    printf("is_get: %d\n", is_get);

                    is_img = is_img_request(buffer);
                    printf("is_img: %d\n", is_img);

                    is_http = is_http_request(req_header, buffer);
                    printf("is_http: %d\n", is_http);

                    // if it's not a valid head or get request but it IS a different VALID http request
                    if(is_get < 0 && is_head < 0 && is_http == 0)
                    {
                        printf("METHOD NOT ALLOWED: %s\n", req_header);
                        strncpy(req_path, "/405.txt", LEN_405);
                        req_path[TEN] = '\0';
                    }
                    // if it's not a valid http request we'll serve back 400 error
                    else if(is_http < 0)
                    {
                        printf("gets 400 file path and isn't proper http request\n");
                        strncpy(req_path, "/400.txt", LEN_405);
                        req_path[TEN] = '\0';
                    }
                    else
                    {
                        // gets the substring from the / to the white space from the buffer and put it in req_path
                        // this is the path of the file the request wants to access
                        set_request_path(req_path, buffer);
                    }
                    printf("req_path: %s\n", req_path);

                    // Mark as a HEAD request
                    if(is_head_request(req_header) == 0)
                    {
                        is_head = 1;
                    }

                    // Handle the client request
                    valwrite          = handle_client(sd, req_path, is_head, is_img);
                    client_sockets[i] = 0;    // Remove from client list
                    if(valwrite < 0)
                    {
                        close(sd);
                        FD_CLR(sd, &readfds);
                        continue;
                    }
                }
            }
        }
    }

    // Cleanup and close all client sockets
    for(size_t i = 0; i < max_clients; i++)
    {
        sd = client_sockets[i];

        if(sd > 0)
        {
            socket_close(sd);
        }
    }

    free(client_sockets);

    // Close the server socket
    close(sockfd);

    printf("closing connection\n");

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

/*
    Sets up the signal handler
 */
static void setup_signal_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
    sa.sa_handler = sigint_handler;
#if defined(__clang__)
    #pragma clang diagnostic pop
#endif
    sigaction(SIGINT, &sa, NULL);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/*
    Updates the signal flag upon receiving the exit signal

    @param signum: The signal number received
 */
static void sigint_handler(int signum)
{
    exit_flag = 1;
}

#pragma GCC diagnostic pop

/*
    Extracts the HTTP method from the request header

    @param
    req_header: Where the HTTP request method will be stored
    buffer: The full HTTP request header
 */
void set_request_method(char *req_header, const char *buffer)
{
    char c;
    int  i = 0;
    int  j = 0;

    // Read the buffer
    c = buffer[i];

    // Copy chars from buffer to req_header until first space
    while(c != ' ' && j < REQ_HEADER_LEN)
    {
        req_header[j++] = c;
        c               = buffer[++i];
    }

    // Null-terminate req_header
    req_header[j] = '\0';

    // Debug: print the extracted request
    printf("request path: %s\n", req_header);
    printf("request path length: %d\n", (int)strlen(req_header));
}

/*
    Checks if the header starts with HEAD

    @param
    req_header: The HTTP request method

    @return
    0: The header starts with HEAD
    -1: The header does not start with HEAD
 */
int is_head_request(const char *req_header)
{
    if(strcmp(req_header, "HEAD") == 0)
    {
        return 0;
    }
    return -1;
}

/*
    Checks if the header starts with GET

    @param
    req_header: The HTTP request method

    @return
    0: The header starts with GET
    -1: The header does not start with GET
 */
int is_get_request(const char *req_header)
{
    if(strcmp(req_header, "GET") == 0)
    {
        return 0;
    }
    return -1;
}

/*
    Checks if the HTTP request is for an image

    @param
    buffer: The full HTTP request header

    @return
    0: The request target is an image
    -1: The request target is not an image
 */
int is_img_request(const char *buffer)
{
    int  i = 0;
    char c = buffer[i];

    // Traverse until the first '.'
    while(c != '.' && c != '\0' && i < BUFFER_SIZE)
    {
        c = buffer[++i];
    }

    // If no '.' was found, it's not an image request
    if(c != '.')
    {
        return -1;
    }

    // Check the next few characters to identify the file extension
    if(strncmp(&buffer[i], ".jpg", FILE_EXT_LEN - 1) == 0 || strncmp(&buffer[i], ".jpeg", FILE_EXT_LEN) == 0 || strncmp(&buffer[i], ".png", FILE_EXT_LEN - 1) == 0 || strncmp(&buffer[i], ".gif", FILE_EXT_LEN - 1) == 0)
    {
        return 0;
    }

    return -1;
}

/*
    Checks if the header contains a valid HTTP request method

    @param
    req_header: The HTTP request method
    buffer: The full HTTP request header

    @return
    0: The buffer contains a valid HTTP request
    -1: The buffer does not contain a valid HTTP request
 */
int is_http_request(const char *req_header, const char *buffer)
{
    int valid_firstline = 0;
    int valid_headers   = 0;

    // Check if the method in req_header is a valid HTTP method
    if(strcmp(req_header, "GET") != 0 && strcmp(req_header, "HEAD") != 0 && strcmp(req_header, "POST") != 0 && strcmp(req_header, "PUT") != 0 && strcmp(req_header, "DELETE") != 0 && strcmp(req_header, "CONNECT") != 0 && strcmp(req_header, "OPTIONS") != 0 &&
       strcmp(req_header, "TRACE") != 0 && strcmp(req_header, "PATCH") != 0)
    {
        return -1;
    }

    // Check if the first line is valid
    valid_firstline = has_valid_first_line(buffer);

    // Check if the headers are valid
    valid_headers = has_valid_headers(buffer);
    if(valid_firstline == -1 || valid_headers == -1)
    {
        return -1;
    }
    return 0;
}

/*
    Extracts the request path from the HTTP request header

    @param
    req_path: The path of the requested file
    buffer: The full HTTP request header
 */
void set_request_path(char *req_path, const char *buffer)
{
    char c;
    int  i = 0;
    int  j = 0;

    // Start reading the buffer
    c = buffer[i];

    // Skip characters until the first space (end of HTTP method)
    // This is because header was already confirmed at this point
    while(c != ' ')
    {
        c = buffer[++i];
    }

    // Move past the space to start of request path
    c = buffer[++i];

    // Copy chars from buffer to req_path until next space
    while(c != ' ' && j < BUFFER_SIZE)
    {
        req_path[j++] = c;
        c             = buffer[++i];
    }

    // Null-terminate req_path
    req_path[j] = '\0';

    // Debug: print the extracted request
    printf("request path: %s\n", req_path);
}

/*
    Processes an incoming HTTP request from a client, constructing an HTTP response
    and sending it back to the client

    @param
    newsockfd: socket fd for the client
    request_path: file path requested by the client
    is_head: flag indicating whether the HTTP request is a HEAD request
    is_img: flag indicating that the HTTP request is for an image

    @return
    0: The HTTP response was successfully sent to the client
    -1: An error occurred while generating the HTTP response body
    -2: The requested file was not found
    -3: Memory allocatio for the response failed
 */
int handle_client(int newsockfd, const char *request_path, int is_head, int is_img)
{
    char  *response_string;                     // The Full HTTP response
    char  *content_string = {0};                // HTTP response body
    char **content_ptr    = &content_string;    // Pointer to dynamically allocated resources
    // TODO: malloc content_type_line
    char          content_type_line[BUFFER_SIZE] = {0};    // Content-type header
    int           valread;                                 // Result of file read operation
    unsigned long length          = 0;                     // Length of response body
    unsigned long response_length = 0;                     // Total length of HTTP response
    int           result;

    // we malloc the content_string in this function
    // length also gets set to the length of the body in this function
    valread = write_to_content_string(content_ptr, &length, request_path);

    if(valread == -1)
    {
        perror("webserver (http response body)");
        return -1;
    }

    // set content type
    if(valread == -2)
    {
        set_content_type_from_file_extension(".html", content_type_line);
    }
    else
    {
        set_content_type_from_file_extension(request_path, content_type_line);
    }
    printf("content_type_line: %s\n", content_type_line);

    if(strcmp(request_path, "/405.txt") == 0)
    {
        // The method is unsupported
        response_length = strlen(HTTP_METHOD_NOT_ALLOWED) + strlen(content_type_line) + CONTENT_LEN_BUF + length;
        response_string = (char *)malloc(sizeof(char) * (response_length + 1));
        if(response_string == NULL)
        {
            perror("webserver (malloc)");
            free(content_string);
            return -3;
        }
        append_msg_to_response_string(response_string, HTTP_METHOD_NOT_ALLOWED);
        strncat(response_string, content_type_line, strlen(content_type_line) + 1);
        append_content_length_msg(response_string, length);

        printf("writing 405 content to response: %s\n", response_string);
        write_to_client(newsockfd, response_string);    // Send 405 response
        close(newsockfd);                               // Close the socket
        free(content_string);
        free(response_string);
        return 0;
    }
    // length of response_string = (HTTP HEADER LEN) + content length string length + body length
    if(valread == -2)
    {
        // Serve 404 response
        response_length = strlen(HTTP_NOT_FOUND) + strlen(content_type_line) + CONTENT_LEN_BUF + length;
        response_string = (char *)malloc(sizeof(char) * (response_length + 1));
        if(response_string == NULL)
        {
            perror("webserver (malloc)");
            if(is_img == -1)
            {
                free(content_string);
            }
            return -3;
        }
        append_msg_to_response_string(response_string, HTTP_NOT_FOUND);
        strncat(response_string, content_type_line, strlen(content_type_line) + 1);
        append_content_length_msg(response_string, length);
        write_to_client(newsockfd, response_string);    // Send 404 response
        close(newsockfd);                               // Close the socket
        free(content_string);
        free(response_string);
        return -2;
    }

    if(strcmp(request_path, "/400.txt") == 0)
    {
        // The request is bad
        response_length = strlen(HTTP_BAD_REQUEST) + strlen(content_type_line) + CONTENT_LEN_BUF + length;
        response_string = (char *)malloc(sizeof(char) * (response_length + 1));
        if(response_string == NULL)
        {
            perror("webserver (malloc)");
            free(content_string);
            return -3;
        }
        append_msg_to_response_string(response_string, HTTP_BAD_REQUEST);
        strncat(response_string, content_type_line, strlen(content_type_line) + 1);
        append_content_length_msg(response_string, length);
        write_to_client(newsockfd, response_string);    // Send 400 response
        close(newsockfd);                               // Close the socket
        free(content_string);
        free(response_string);
        return -1;
    }
    // if it's an image we write directly to the socket
    if(is_img == 0)
    {
        int retval = 0;
        printf("it's an image!!!\n");
        response_length = strlen(HTTP_OK) + strlen(content_type_line) + CONTENT_LEN_BUF + length;
        response_string = (char *)malloc(sizeof(char) * (response_length + 1));
        if(response_string == NULL)
        {
            perror("webserver (malloc)");
            free(content_string);
            return -3;
        }

        append_msg_to_response_string(response_string, HTTP_OK);
        strncat(response_string, content_type_line, strlen(content_type_line) + 1);
        append_content_length_msg(response_string, length);

        if(write_to_client(newsockfd, response_string) < 0)
        {
            perror("Error writing to client");
            retval = -1;
            goto cleanup;
        }

        if(write_to_content_binary(newsockfd, request_path) < 0)
        {
            perror("Error writing content to client");
            retval = -1;
            goto cleanup;
        }

    cleanup:
        if(response_string)
        {
            free(response_string);
        }
        if(content_string)
        {
            free(content_string);
        }
        return retval;
    }
    // Request was successful
    printf("request was successful");
    response_length = strlen(HTTP_OK) + strlen(content_type_line) + CONTENT_LEN_BUF + length;
    response_string = (char *)malloc(sizeof(char) * (response_length + 1));
    if(response_string == NULL)
    {
        perror("webserver (malloc)");
        free(content_string);
        return -3;
    }
    append_msg_to_response_string(response_string, HTTP_OK);

    // Append the content-type header
    strncat(response_string, content_type_line, strlen(content_type_line) + 1);

    // append content length section (can only do this once we have the body)
    // but must be appended before the body
    append_content_length_msg(response_string, length);

    // append body section, only if not a HEAD request
    if(is_head == -1)
    {
        append_body(response_string, *content_ptr, length);
    }

    // free allocated memory for the body
    free(content_string);
    result = write_to_client(newsockfd, response_string);
    free(response_string);
    close(newsockfd);

    // write to client
    return result;
}

/*
    Closes a socket

    @param
    sockfd: The file descriptor of the socket to close
 */
static void socket_close(int sockfd)
{
    if(close(sockfd) == -1)
    {
        perror("Error closing socket");
        exit(EXIT_FAILURE);
    }
}

/*
    Checks if the request has a valid first line like METHOD URI HTTP/x{x}\r\n
    This function assumes the HTTP method has already been validated

    @param
    buffer: The buffer containing the HTTP request

    @return
    0: The request has a valid first line
    -1: The request is invalid
 */
int has_valid_first_line(const char *buffer)
{
    int  i = 0;
    char c = buffer[i];

    // Traverse until the first space
    while(c != ' ' && i < BUFFER_SIZE)
    {
        c = buffer[++i];
    }

    // Check that there is no URI before HTTP/
    if(i < BUFFER_SIZE - 4)
    {
        if(buffer[i + 1] == 'H' && buffer[i + 2] == 'T' && buffer[i + 3] == 'T' && buffer[i + 4] == 'P' && buffer[i + FILE_EXT_LEN] == '/')
        {
            return -1;
        }
    }

    // Traverse until the next space, end of the uRI
    while(c != ' ' && i < BUFFER_SIZE)
    {
        c = buffer[++i];
    }

    // Check that the URI does not end with a '/'
    if(i < BUFFER_SIZE - 1 && buffer[i + 1] != '/')
    {
        return -1;
    }

    // Move past the URI
    c = buffer[++i];

    // Traverse until the next space
    while(c != ' ' && i < BUFFER_SIZE)
    {
        c = buffer[++i];
    }
    i++;

    // will return -1 if there is no HTTP/x{x}
    if(buffer[i] != 'H' || buffer[i + 1] != 'T' || buffer[i + 2] != 'T' || buffer[i + 3] != 'P' || buffer[i + 4] != '/')
    {
        return -1;
    }

    // Traverse until '\r'
    while(c != '\r')
    {
        c = buffer[++i];
    }

    // Check if the line ends with \r\n
    if(buffer[i + 1] != '\n')
    {
        return -1;
    }
    return 0;
}

/*
    Checks to make sure headers have colons and end in \r\n\r\n, and each ends with \r\n
    This assumes the request line has already been validate

    @param
    buffer: Holds the entire HTTP request

    @return
    0: The headers are valid
    -1: The headers are invalid
 */
int has_valid_headers(const char *buffer)
{
    int  i              = 0;
    char c              = buffer[i];
    int  final_rn_found = -1;

    // go to the first \r\n (which is right before the headers)
    while(i < BUFFER_SIZE - 1 && c != '\r')
    {
        c = buffer[++i];
    }
    c = buffer[++i];    // buffer is now \n

    // Process the headers
    while(i < BUFFER_SIZE && final_rn_found == -1)
    {
        // find a colon
        while(i < BUFFER_SIZE - 1 && c != ':')
        {
            c = buffer[++i];
        }
        if(i > BUFFER_SIZE - 1)
        {
            return -1;
        }
        // make sure there is at least 1 char after the colon
        c = buffer[i];

        // find the \r\n
        while(i < BUFFER_SIZE - 3 && c != '\r')
        {
            c = buffer[++i];
        }
        if(i > BUFFER_SIZE - 2)
        {
            return -1;
        }
        if(buffer[++i] != '\n')
        {
            return -1;
        }
        // we have already incremeted i one past the \r\n

        // if we don't hit another \r we havent hit the end of the headers
        if(buffer[++i] != '\r')
        {
            continue;
        }

        // check if the next characters are \r\n, marking the end of the header
        if(buffer[i] == '\r' && buffer[i + 1] == '\n')
        {
            final_rn_found = 0;
        }
        break;
    }
    return final_rn_found;
}

/*
    Reads the content of a file at the specified path and writes it to a string

    @param
    content_string: Where the text content of a file will be stored
    length: Length of the content
    file_path: The path to the file being read

    @return
    0: File was read successfully and stored in content_string
    -1: An error occurred while reading the file
    -2: The requested file was not found, 404 page loaded instead
    -3: Memory allocation failed while creating content_string
 */
int write_to_content_string(char **content_string, unsigned long *length, const char *file_path)
{
    char         c;                                     // Temp character for read functions
    struct stat  file_stat;                             // Holds file metadata
    struct stat *fileStat = &file_stat;                 // Pointer to file metadata
    int          file_fd;                               // File descriptor for the file
    char        *path;                                  // String that will store the file path
    const char  *MSG_404 = "<p>404 NOT FOUND</p>\0";    // 404 error message
    int          retval  = 0;                           // Return value

    // Check if the requested file path is "/"
    if(strcmp(file_path, "/") == 0)
    {
        // Allocate memory for the index path
        path = (char *)malloc(sizeof(char) * (FILE_PATH_LEN + 1));
        if(path == NULL)
        {
            perror("malloc");
            return -3;
        }

        // Copy the index path
        for(size_t i = 0; i < strlen(INDEX_FILE_PATH); i++)
        {
            path[i] = INDEX_FILE_PATH[i];
        }
        path[strlen(INDEX_FILE_PATH)] = '\0';
    }
    else
    {
        // Allocate memory for the requested file path
        path = (char *)malloc(sizeof(char) * (strlen(file_path) + 1));
        if(path == NULL)
        {
            perror("malloc");
            return -3;
        }

        // Copy the file path
        for(size_t i = 0; i < strlen(file_path); i++)
        {
            path[i] = file_path[i];
        }
        path[strlen(file_path)] = '\0';
    }

    // Open the file at the specified path
    open_file_at_path(path, &file_fd, fileStat);

    // Free the allocated path memory
    free(path);

    // If file could not be opened, served the 404 error page
    if(file_fd == -1)
    {
        // If the 404 file is missing, return an error message
        perror("webserver (open: 404 html msg file has been moved or deleted)");
        *content_string = (char *)malloc((sizeof(char) * SIZE_404_MSG) + 1);
        if(*content_string == NULL)
        {
            perror("webserver (malloc)");
            close(file_fd);
            return -3;
        }
        for(int i = 0; i <= SIZE_404_MSG; i++)
        {
            (*content_string)[i] = MSG_404[i];
        }
        close(file_fd);
        return -2;
    }

#if(defined(__APPLE__) && defined(__MACH__))
    printf("filestat st_size: %lld\n", fileStat->st_size);
#endif

#if defined(__linux__)
    printf("filestat st_size: %ld\n", fileStat->st_size);
#endif

    // Allocate memory for the content string
    *content_string = (char *)malloc(sizeof(char) * ((size_t)fileStat->st_size + 1));
    if(*content_string == NULL)
    {
        perror("webserver (malloc)");
        close(file_fd);
        return -3;
    }

    // Read the contents of the file into content_string
    for(int i = 0; i < fileStat->st_size; i++)
    {
        ssize_t valread = read(file_fd, &c, sizeof(char));
        if(valread < 0)
        {
            perror("webserver (read content string)");
            close(file_fd);
            free(*content_string);
            return -1;
        }
        (*content_string)[i] = c;
        (*length)++;
    }
    (*content_string)[(*length)] = '\0';
    printf("content_string: %s\n", *content_string);

    // Close the file descriptor
    close(file_fd);

    // we don't want to free the content_string here because we need it to stay allocated
    // in order to put it in the response_string in handle_client

    return retval;
}

int write_to_content_binary(int fd, const char *file_path)
{
    struct stat  file_stat;                // Holds file metadata
    struct stat *fileStat = &file_stat;    // Pointer to file metadata
    int          file_fd;                  // File descriptor for the file
    char        *path;                     // String to store the file path
    int          retval = 0;               // Return value
    ssize_t      bytes_read;
    ssize_t      bytes_written;
    char        *buffer;

    // Check if the requested file path is "/"
    if(strcmp(file_path, "/") == 0)
    {
        // Allocate memory for the index path
        path = (char *)malloc(sizeof(char) * (FILE_PATH_LEN + 1));
        if(path == NULL)
        {
            perror("malloc");
            return -3;
        }

        // Copy the index path
        strncpy(path, INDEX_FILE_PATH, FILE_PATH_LEN);
        path[FILE_PATH_LEN] = '\0';
    }
    else
    {
        // Allocate memory for the requested file path
        path = (char *)malloc(sizeof(char) * (strlen(file_path) + 1));
        if(path == NULL)
        {
            perror("malloc");
            return -3;
        }

        // Copy the file path
        strncpy(path, file_path, strlen(file_path));
        path[strlen(file_path)] = '\0';
    }

    // Open the file at the specified path
    open_file_at_path(path, &file_fd, fileStat);

    // Free the allocated path memory
    free(path);

    // If file could not be opened, serve the 404 error page (returning -2 signals this)
    if(file_fd == -1)
    {
        return -2;
    }

#if(defined(__APPLE__) && defined(__MACH__))
    printf("File size: %lld bytes\n", fileStat->st_size);
#endif

#if defined(__linux__)
    printf("File size: %ld bytes\n", fileStat->st_size);
#endif

    // Allocate memory for the binary content
    buffer = (char *)malloc((size_t)fileStat->st_size);

    if(buffer == NULL)
    {
        perror("webserver (malloc)");
        close(file_fd);
        return -3;
    }

    // Read the entire file into buffer
    bytes_read = read(file_fd, buffer, (size_t)fileStat->st_size);
    if(bytes_read < 0)
    {
        perror("webserver (read binary file)");
        free(buffer);
        close(file_fd);
        return -1;
    }

    bytes_written = write(fd, buffer, (size_t)fileStat->st_size);
    if(bytes_written < 0)
    {
        perror("Error writing to destination socket");
        free(buffer);
        close(fd);
        close(file_fd);
        return -1;
    }

    // Close the file descriptors and free dynamic memory
    close(file_fd);
    close(fd);
    free(buffer);
    return retval;    // Success
}

/*
    Sends the HTTP response to the client

    @param
    newsockfd: The client's socket file descriptor
    response_string: The HTTP response string to be sent

    @return
    0: Response successfully sent
    -1: An error occurred while writing to the socket
 */
int write_to_client(int newsockfd, const char *response_string)
{
    ssize_t valwrite;
    valwrite = write(newsockfd, response_string, strlen(response_string));
    if(valwrite < 0)
    {
        perror("webserver (write)");
        return -1;
    }
    printf("successfully wrote to client\n");
    return 0;
}

/*
    Sets the content-type header based on the file extension in the requested path

    @param
    request_path: The path of the requested file
    content_type_string: A string where the appropriate content-type will be stored
 */
void set_content_type_from_file_extension(const char *request_path, char *content_type_string)
{
    size_t req_path_i                   = strlen(request_path) - 1;
    int    file_ext_i                   = 0;
    char   file_extension[FILE_EXT_LEN] = {0};

    // grab the last chars up to '.' of the request_path
    printf("request path index: %zu\n", req_path_i);
    while(request_path[req_path_i] != '.' && req_path_i > 0 && file_ext_i < FILE_EXT_LEN)
    {
        file_extension[file_ext_i++] = request_path[req_path_i--];
    }
    printf("file_extension: %s\n", file_extension);

    if(strcmp(file_extension, TXT_EXT) == 0)
    {
        strncpy(content_type_string, TEXT_CONTENT_TYPE, strlen(TEXT_CONTENT_TYPE));
        content_type_string[strlen(TEXT_CONTENT_TYPE)] = '\0';
    }
    else if(strcmp(file_extension, JS_EXT) == 0)
    {
        strncpy(content_type_string, JS_CONTENT_TYPE, strlen(JS_CONTENT_TYPE));
        content_type_string[strlen(JS_CONTENT_TYPE)] = '\0';
    }
    else if(strcmp(file_extension, CSS_EXT) == 0)
    {
        strncpy(content_type_string, CSS_CONTENT_TYPE, strlen(CSS_CONTENT_TYPE));
        content_type_string[strlen(CSS_CONTENT_TYPE)] = '\0';
    }
    else if(strcmp(file_extension, JPG_EXT) == 0 || strcmp(file_extension, JPEG_EXT) == 0)
    {
        strncpy(content_type_string, JPEG_CONTENT_TYPE, strlen(JPEG_CONTENT_TYPE));
        content_type_string[strlen(JPEG_CONTENT_TYPE)] = '\0';
    }
    else if(strcmp(file_extension, PNG_EXT) == 0)
    {
        strncpy(content_type_string, PNG_CONTENT_TYPE, strlen(PNG_CONTENT_TYPE));
        content_type_string[strlen(PNG_CONTENT_TYPE)] = '\0';
    }
    else if(strcmp(file_extension, GIF_EXT) == 0)
    {
        strncpy(content_type_string, GIF_CONTENT_TYPE, strlen(GIF_CONTENT_TYPE));
        content_type_string[strlen(GIF_CONTENT_TYPE)] = '\0';
    }
    else
    {
        strncpy(content_type_string, HTML_CONTENT_TYPE, strlen(HTML_CONTENT_TYPE));
        content_type_string[strlen(HTML_CONTENT_TYPE)] = '\0';
    }

    printf("set content type header to: %s\n", content_type_string);
}

/*
    Appends a message to the response string

    @param
    response: Where the message will be appended
    msg: The message to be copied
 */
void append_msg_to_response_string(char *response, const char *msg)
{
    strncpy(response, msg, strlen(msg));
    response[strlen(msg)] = '\0';
}

/*
    Appends a Content-Length header to the HTTP response string
    This one is special because it has the extra \r\n and needs to be constructed with the appropriate length

    @param
    response_string: Where the content-length will be appended
    length: Length of the HTTP body
 */
void append_content_length_msg(char *response_string, unsigned long length)
{
    char content_len_buffer[CONTENT_LEN_BUF];
    char content_length_msg[BUFFER_SIZE] = "Content-Length: ";

    // Convert the length value to a string and store it in content_len_buffer
    int_to_string(content_len_buffer, length);
    printf("content length: %s\n", content_len_buffer);

    // Append the length
    strncat(content_length_msg, content_len_buffer, strlen(content_len_buffer));

    // Append a trailing CRLF sequence
    strncat(content_length_msg, "\r\n\r\n", CONTENT_TERM_LEN);

    printf("content_length_msg: %s\n", content_length_msg);
    printf("length: %lu\n", length);

    // Append the content-length header
    strncat(response_string, content_length_msg, strlen(content_length_msg) + 1);

    printf("response string: %s\n", response_string);
}

/*
    Appends the body and a trailing CRLF sequence to the HTTP response string

    @param
    response_string: Contains the HTTP response
    content_string: The body content to be appended
    length: The length of the string to be appended
*/
void append_body(char *response_string, const char *content_string, unsigned long length)
{
    if(content_string != NULL)
    {
        strncat(response_string, content_string, length);
        strncat(response_string, "\r\n", 2);
    }
}

/*
    Converts an integer into a string

    @param
    string: Where the converted value will be stored
    n: The number to be converted
 */
void int_to_string(char *string, unsigned long n)
{
    char          buffer[TEN] = {0};
    int           digits      = 0;
    unsigned long i           = n;

    // If n is zero
    if(n == 0)
    {
        string[0] = '0';
        string[1] = '\0';
        return;
    }

    // Extract digits and store them in reverse order
    while(i > 0)
    {
        buffer[digits++] = (char)((i % TEN) + '0');
        i                = i / TEN;
        printf("%lu\n", i);
    }
    printf("digits: %d\n", digits);

    // Reverse the order of the digits in the buffer
    for(int j = 0; j < digits; j++)
    {
        string[j] = buffer[digits - j - 1];
    }
    string[digits] = '\0';
}

/*
    Opens a file at the specified path and retrieves its file descriptor and metadata

    @param
    request_path: The path of the file to open
    file_fd: Stores the file descriptor of the file
    file_state: Stores the file's metadata
 */
void open_file_at_path(const char *request_path, int *file_fd, struct stat *file_stat)
{
    // Allocate memory for the file path
    char *path = (char *)malloc(sizeof(char) * (strlen(request_path) + FILE_PATH_LEN + 1));

    // Set the base directory
#if(defined(__APPLE__) && defined(__MACH__))
    strncpy(path, "./resources", FILE_PATH_LEN);
#endif

#if defined(__linux__)
    strncpy(path, "../resources", FILE_PATH_LEN);
#endif

    // Append the requested file path
    strncpy(path + FILE_PATH_LEN, request_path, strlen(request_path) + 1);
    printf("file path: %s\n", path);

    // Open the file and store the file descriptor
    *file_fd = open(path, O_RDONLY | O_CLOEXEC);

    // Retrieve the file metadata
    stat(path, file_stat);

#if(defined(__APPLE__) && defined(__MACH__))
    printf("File size of %s: %lld bytes\n", path, file_stat->st_size);
#endif

#if defined(__linux__)
    printf("File size of %s: %ld bytes\n", path, file_stat->st_size);
#endif

    printf("File descriptor: %d\n", *file_fd);

    // Free the allocated memory
    free(path);
}
