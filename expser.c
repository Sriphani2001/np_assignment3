#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <ifaddrs.h>
#include <math.h>
#include <netdb.h>
#include <regex.h>

#define MAX_CLIENTS 50
#define MAX_BUFFER_SIZE 2048
#define MAX_NAME_LENGTH 12
#define PROTOCOL_MESSAGE "HELLO 1\n"
#define OK_MESSAGE "OK\n"
#define ERROR_MESSAGE "ERROR\n"

static _Atomic unsigned int client_count = 0;
static int uid = 10;

// Client structure
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[MAX_NAME_LENGTH];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Utility function to handle errors
void handle_error(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

// Add client to the client list
void add_client_to_queue(client_t *client) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = client;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Remove client from the client list
void remove_client_from_queue(int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->uid == uid) {
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send message to all clients except the sender
void send_message_to_all(char *message, int sender_uid) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->uid != sender_uid) {
            if (send(clients[i]->sockfd, message, strlen(message), 0) <= 0) {
                printf("ERROR: Failed to send message to client (uid=%d)\n", clients[i]->uid);
                fflush(stdout);
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Handle client communication
void *handle_client(void *arg) {
    char buffer[MAX_BUFFER_SIZE];
    int leave_flag = 0;

    client_count++;
    client_t *client = (client_t *)arg;

    while (1) {
        if (leave_flag) break;

        int receive = recv(client->sockfd, buffer, MAX_BUFFER_SIZE, 0);
        if (receive > 0) {
            if (strlen(buffer) > 0) {
                char message[MAX_BUFFER_SIZE] = {0};
                char formatted_message[MAX_BUFFER_SIZE] = {0};

                // Extract and format the message
                snprintf(message, sizeof(message), "%s", buffer + 3);
                message[strcspn(message, "\n")] = '\0';  // Remove newline character

                // Check for valid message length
                int charcount = strlen(message) - 1;
                if (charcount < 257) {
                    snprintf(formatted_message, sizeof(formatted_message), "MSG %s %s\n", client->name, message);
                    printf("%s: %s\n", client->name, message);
                    fflush(stdout);
                    send_message_to_all(formatted_message, client->uid);
                } else {
                    snprintf(buffer, sizeof(buffer), "ERROR %s: Message too long\n", client->name);
                    send_message_to_all(buffer, client->uid);
                }
            }
        } else if (receive == 0) {
            sprintf(buffer, "%s left the chat\n", client->name);
            printf("%s", buffer);
            fflush(stdout);
            leave_flag = 1;
        } else {
            printf("ERROR: Client (uid=%d) communication error\n", client->uid);
            fflush(stdout);
            leave_flag = 1;
        }
        memset(buffer, 0, MAX_BUFFER_SIZE); // Clear buffer
    }

    close(client->sockfd);
    remove_client_from_queue(client->uid);
    free(client);
    client_count--;
    pthread_detach(pthread_self());

    return NULL;
}

// Initialize server socket
int initialize_server_socket(const char *host, const char *port) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int sockfd;
    if (getaddrinfo(host, port, &hints, &res) != 0) {
        handle_error("ERROR: Failed to resolve socket address");
    }

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) handle_error("ERROR: Server socket creation failed");

    int option = 1;
    if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option)) < 0) {
        handle_error("ERROR: setsockopt failed");
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        handle_error("ERROR: Server socket bind failed");
    }

    freeaddrinfo(res);
    return sockfd;
}

// Main server function
int main(int argc, char **argv) {
    if (argc != 2) {
        printf("ERROR: Usage: %s <host:port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Parse host and port from command line argument
    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (!host || !port) {
        printf("ERROR: Invalid host or port format. Use <host:port>\n");
        exit(EXIT_FAILURE);
    }
    printf("Host: %s, Port: %s\n", host, port);

    int server_sockfd = initialize_server_socket(host, port);

    // Listen for incoming connections
    if (listen(server_sockfd, MAX_CLIENTS) < 0) {
        handle_error("ERROR: Server listen failed");
    }
    printf("Server listening on %s:%s...\n", host, port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sockfd < 0) {
            printf("ERROR: Accept client connection failed\n");
            continue;
        }

        if ((client_count + 1) == MAX_CLIENTS) {
            printf("ERROR: Maximum clients reached. Rejected: ");
            printf(":%d\n", client_addr.sin_port);
            close(client_sockfd);
            continue;
        }

        // Send protocol version message
        if (send(client_sockfd, PROTOCOL_MESSAGE, strlen(PROTOCOL_MESSAGE), 0) <= 0) {
            printf("ERROR: Failed to send protocol message\n");
            close(client_sockfd);
            continue;
        }

        // Handle NICK message
        char nick_buffer[MAX_BUFFER_SIZE] = {0};
        if (recv(client_sockfd, nick_buffer, sizeof(nick_buffer), 0) <= 0) {
            printf("ERROR: Receiving NICK message failed\n");
            close(client_sockfd);
            continue;
        }

        char client_name[MAX_NAME_LENGTH];
        sscanf(nick_buffer, "NICK %s", client_name);
        client_name[strcspn(client_name, "\n")] = '\0';

        regex_t regex;
        if (regcomp(&regex, "^[A-Za-z0-9_]+$", REG_EXTENDED) != 0) {
            handle_error("ERROR: Could not compile regex");
        }

        if (regexec(&regex, client_name, 0, NULL, 0) == 0 && strlen(client_name) < MAX_NAME_LENGTH) {
            if (send(client_sockfd, OK_MESSAGE, strlen(OK_MESSAGE), 0) <= 0) {
                printf("ERROR: Sending OK message failed\n");
                close(client_sockfd);
                continue;
            }

            // Add client to queue
            client_t *client = (client_t *)calloc(1, sizeof(client_t));
            client->address = client_addr;
            client->sockfd = client_sockfd;
            client->uid = uid++;
            strcpy(client->name, client_name);

            printf("%s joined the chat\n", client->name);
            add_client_to_queue(client);
            pthread_t tid;
            pthread_create(&tid, NULL, &handle_client, (void*)client);

        } else {
            // Invalid nickname
            send(client_sockfd, ERROR_MESSAGE, strlen(ERROR_MESSAGE), 0);
            close(client_sockfd);
        }
    }

    close(server_sockfd);
    return 0;
}
