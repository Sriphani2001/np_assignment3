#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <regex.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

#define BUFFER_SIZE 256
#define MAX_CLIENTS 100
#define NICKNAME_MAX_LEN 12

int server_sockfd;
int client_sockets[MAX_CLIENTS];
char client_nicknames[MAX_CLIENTS][NICKNAME_MAX_LEN + 1];
regex_t regex;

// Signal handler for graceful shutdown
void handle_sigint(int sig) {
    printf("Server shutdown signal received. Closing all connections...\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0) {
            close(client_sockets[i]);
            client_sockets[i] = 0;
        }
    }
    close(server_sockfd);
    regfree(&regex);
    printf("Server shutdown complete.\n");
    exit(0);
}

// Function to validate nickname using regex
int validate_nickname(const char *nickname) {
    return regexec(&regex, nickname, 0, NULL, 0) == 0;
}

// Function to broadcast messages to all clients except the sender
void broadcast_message(const char *message, int sender_sockfd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != sender_sockfd) {
            if (send(client_sockets[i], message, strlen(message), 0) == -1) {
                perror("Error sending broadcast message");
            }
        }
    }
}

// Function to get the index of a client socket
int get_client_index(int sockfd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == sockfd) {
            return i;
        }
    }
    return -1;
}

// Main server function
int main(int argc, char *argv[]) {
    struct sockaddr_in server_addr, client_addr;
    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    int max_sd, new_socket, activity, valread;
    socklen_t addrlen = sizeof(client_addr);

    // Initialize client sockets array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = 0;
    }

    // Check for correct number of arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP:PORT>\n", argv[0]);
        exit(1);
    }

    // Parse IP and port
    char *host = strtok(argv[1], ":");
    char *portStr = strtok(NULL, ":");
    if (!host || !portStr) {
        fprintf(stderr, "Error: Invalid IP:PORT format.\n");
        exit(1);
    }
    int port = atoi(portStr);

    // Compile regular expression for nickname validation
    if (regcomp(&regex, "^[A-Za-z0-9_]{1,12}$", REG_EXTENDED) != 0) {
        fprintf(stderr, "Error compiling regex for nickname validation.\n");
        exit(1);
    }

    // Signal handler for SIGINT
    signal(SIGINT, handle_sigint);

    // Create server socket
    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(1);
    }
    printf("Server socket created successfully.\n");

    // Set socket options
    int opt = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("Set socket options failed");
        exit(1);
    }
    printf("Server socket options set.\n");

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(host);
    server_addr.sin_port = htons(port);

    // Bind the socket to the IP and port
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Socket binding failed");
        exit(1);
    }
    printf("Socket bound to %s:%d.\n", host, port);

    // Listen for incoming connections
    if (listen(server_sockfd, 3) < 0) {
        perror("Listen failed");
        exit(1);
    }
    printf("Server is listening on %s:%d.\n", host, port);

    // Main loop for accepting and handling clients
    while (1) {
        // Clear the socket set and add server socket
        FD_ZERO(&read_fds);
        FD_SET(server_sockfd, &read_fds);
        max_sd = server_sockfd;

        // Add child sockets to set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &read_fds);
            if (sd > max_sd) max_sd = sd;
        }

        // Wait for activity on one of the sockets
        activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0 && errno != EINTR) {
            perror("Select error");
        }

        // Incoming connection on server socket
        if (FD_ISSET(server_sockfd, &read_fds)) {
            if ((new_socket = accept(server_sockfd, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
                perror("Accept error");
                exit(1);
            }
            printf("New connection from %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            // Add new client socket to array
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    printf("Added new client socket %d at index %d.\n", new_socket, i);
                    break;
                }
            }
        }

        // Handle communication with clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (FD_ISSET(sd, &read_fds)) {
                valread = recv(sd, buffer, BUFFER_SIZE - 1, 0);
                if (valread == 0) {
                    // Client disconnected
                    getpeername(sd, (struct sockaddr *)&client_addr, &addrlen);
                    printf("Client disconnected: %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    buffer[valread] = '\0';
                    int client_index = get_client_index(sd);
                    if (client_index == -1) {
                        fprintf(stderr, "Error: Client index not found.\n");
                        continue;
                    }

                    // Process and broadcast the message
                    if (strncmp(buffer, "NICK ", 5) == 0) {
                        char *nickname = buffer + 5;
                        char *newline_pos = strchr(nickname, '\n');
                        if (newline_pos != NULL) {
                            *newline_pos = '\0';  // Remove the newline character
                        }
                        if (validate_nickname(nickname)) {
                            strcpy(client_nicknames[client_index], nickname);
                            send(sd, "OK\n", 3, 0);
                            printf("Valid nickname: %s\n", nickname);
                        } else {
                            send(sd, "ERR Invalid nickname\n", 21, 0);
                            close(sd);
                            client_sockets[client_index] = 0;
                        }
                    } else if (strncmp(buffer, "MSG ", 4) == 0) {
                        char message[BUFFER_SIZE];
                        snprintf(message, sizeof(message), "MSG %s: %s\n", client_nicknames[client_index], buffer + 4);
                        broadcast_message(message, sd);
                        printf("Received message from %s: %s\n", client_nicknames[client_index], buffer + 4);
                    }
                }
            }
        }
    }

    return 0;
}
