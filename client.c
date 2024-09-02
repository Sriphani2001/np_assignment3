#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <regex.h>
#include <signal.h>
#include <sys/select.h>
#include <termios.h> // For non-echoing input

#define BUFFER_SIZE 256
#define NICKNAME_MAX_LEN 12

int sockfd;  // Socket file descriptor
struct termios oldtio; // Terminal settings for restoring on exit

// Function to handle cleanup on exit
void handle_sigint(int sig) {
    printf("\nInterrupt received, closing connection.\n");
    close(sockfd);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldtio); // Restore terminal settings
    exit(0);
}

// Function to validate nickname using regex
int validate_nickname(const char *nickname) {
    regex_t regex;
    int result;

    // Compile regex to allow [A-Za-z0-9_]
    result = regcomp(&regex, "^[A-Za-z0-9_]{1,12}$", REG_EXTENDED);
    if (result) {
        fprintf(stderr, "Could not compile regex\n");
        exit(1);
    }

    // Execute regex to check nickname
    result = regexec(&regex, nickname, 0, NULL, 0);
    regfree(&regex);

    return result == 0;  // Returns 1 if nickname is valid, 0 otherwise
}

// Function to establish a TCP connection
int connect_to_server(const char *host, const char *port) {
    struct addrinfo hints, *servinfo, *p;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &servinfo) != 0) {
        perror("getaddrinfo");
        exit(1);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue; // Try the next result
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            continue; // Try the next result
        }
        break;
    }

    freeaddrinfo(servinfo);

    if (p == NULL) {
        fprintf(stderr, "Failed to connect to server\n");
        exit(1);
    }

    return sockfd;
}

// Function to configure terminal for non-echoing input
void configure_terminal() {
    struct termios newtio;
    tcgetattr(STDIN_FILENO, &oldtio); // Save current terminal settings
    newtio = oldtio; // Copy the settings
    newtio.c_lflag &= ~(ECHO); // Disable echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newtio); // Apply the new settings
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP:PORT> <nickname>\n", argv[0]);
        exit(1);
    }

    // Split the IP and port
    char *host = strtok(argv[1], ":");
    char *portStr = strtok(NULL, ":");
    if (!host || !portStr) {
        fprintf(stderr, "Error: Invalid IP:PORT format.\n");
        exit(1);
    }

    // Validate nickname
    const char *nickname = argv[2];
    if (strlen(nickname) > NICKNAME_MAX_LEN || !validate_nickname(nickname)) {
        fprintf(stderr, "Error: Invalid nickname format.\n");
        exit(1);
    }

    // Establish a connection to the server
    sockfd = connect_to_server(host, portStr);
    signal(SIGINT, handle_sigint); // Handle SIGINT for graceful exit
    configure_terminal(); // Disable input echo

    // Send the nickname to the server
    char sendBuffer[BUFFER_SIZE];
    snprintf(sendBuffer, sizeof(sendBuffer), "NICK %s\n", nickname);
    send(sockfd, sendBuffer, strlen(sendBuffer), 0);
    fflush(stdout);

    // Check server response
    char recvBuffer[BUFFER_SIZE];
    int bytesReceived = recv(sockfd, recvBuffer, sizeof(recvBuffer) - 1, 0);
    if (bytesReceived <= 0 || strncmp(recvBuffer, "OK", 2) != 0) {
        fprintf(stderr, "Error: Server did not accept nickname or connection failed.\n");
        close(sockfd);
        tcsetattr(STDIN_FILENO, TCSANOW, &oldtio); // Restore terminal settings
        exit(1);
    }

    // Handle non-blocking input and communication
    fd_set read_fds;
    int max_fd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            break;
        }

        if (FD_ISSET(sockfd, &read_fds)) {
            // Handle incoming message from the server
            bytesReceived = recv(sockfd, recvBuffer, sizeof(recvBuffer) - 1, 0);
            if (bytesReceived <= 0) {
                printf("\nDisconnected from server.\n");
                break;
            }
            recvBuffer[bytesReceived] = '\0';
            if (strncmp(recvBuffer, "ERROR", 5) == 0) {
                fprintf(stderr, "Server Error: %s", recvBuffer + 6); // Display server error message
            } else {
                // Print the received message
                printf("%s", recvBuffer);
            }
            fflush(stdout);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            // Handle user input
            if (fgets(sendBuffer, sizeof(sendBuffer), stdin) != NULL) {
                send(sockfd, sendBuffer, strlen(sendBuffer), 0);
                fflush(stdout);
            }
        }
    }

    // Clean up before exit
    close(sockfd);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldtio); // Restore terminal settings
    return 0;
}
