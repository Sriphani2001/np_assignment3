#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <regex>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <signal.h>
#include <cmath>
#include <fcntl.h>

#define MAX_CLIENTS 50
#define MAX_BUFFER_SIZE 2048
#define MAX_NAME_LENGTH 12
#define PROTOCOL_MESSAGE "HELLO 1\n"
#define OK_MESSAGE "OK\n"
#define ERROR_MESSAGE "ERROR\n"

std::atomic<unsigned int> client_count(0);
int uid = 10;

// Client structure
struct Client {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    std::string name;
};

std::vector<Client*> clients(MAX_CLIENTS, nullptr);
std::mutex clients_mutex;

// Utility function to handle errors
void handle_error(const std::string &message) {
    perror(message.c_str());
    exit(EXIT_FAILURE);
}

// Add client to the client list
void add_client_to_queue(Client *client) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &c : clients) {
        if (!c) {
            c = client;
            break;
        }
    }
}

// Remove client from the client list
void remove_client_from_queue(int uid) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &c : clients) {
        if (c && c->uid == uid) {
            c = nullptr;
            break;
        }
    }
}

// Send message to all clients except the sender
void send_message_to_all(const std::string &message, int sender_uid) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &c : clients) {
        if (c && c->uid != sender_uid) {
            if (send(c->sockfd, message.c_str(), message.length(), 0) <= 0) {
                std::cout << "ERROR: Failed to send message to client (uid=" << c->uid << ")\n";
                fflush(stdout);  // Added flushing of stdout
                break;
            }
        }
    }
}

// Handle client communication
void handle_client(Client *client) {
    char buffer[MAX_BUFFER_SIZE];
    bool leave_flag = false;

    client_count++;

    while (true) {
        if (leave_flag) break;

        int receive = recv(client->sockfd, buffer, MAX_BUFFER_SIZE, 0);
        if (receive > 0) {
            if (strlen(buffer) > 0) {
                // Validate and parse the incoming message
                std::string buffer_str(buffer);
                if (buffer_str.rfind("MSG ", 0) == 0) { // Ensure the message starts with "MSG "
                    std::string message = buffer_str.substr(4);  // Extract message after "MSG "
                    message.erase(message.find_last_not_of(" \n\r\t") + 1);  // Remove trailing newline character

                    if (message.length() <= 255) {
                        std::string formatted_message = "MSG " + client->name + " " + message + "\n";
                        std::cout << client->name << ": " << message << std::endl;
                        fflush(stdout);  // Added flushing of stdout
                        send_message_to_all(formatted_message, client->uid);
                    } else {
                        std::string error_message = "ERROR " + client->name + ": Message too long\n";
                        send_message_to_all(error_message, client->uid);
                    }
                } else {
                    // Invalid message format
                    std::string error_message = "ERROR Invalid message format\n";
                    send(client->sockfd, error_message.c_str(), error_message.length(), 0);
                }
            }
        } else if (receive == 0) {
            std::cout << client->name << " left the chat\n";
            fflush(stdout);  // Added flushing of stdout
            std::string leave_message = "MSG " + client->name + " has left the chat\n";
            send_message_to_all(leave_message, client->uid);
            leave_flag = true;
        } else {
            std::cout << "ERROR: Client (uid=" << client->uid << ") communication error\n";
            fflush(stdout);  // Added flushing of stdout
            leave_flag = true;
        }
        memset(buffer, 0, MAX_BUFFER_SIZE); // Clear buffer
    }

    close(client->sockfd);
    remove_client_from_queue(client->uid);
    delete client;
    client_count--;
}

// Initialize server socket
int initialize_server_socket(const char *host, const char *port) {
    struct addrinfo hints{}, *res;
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
    if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), reinterpret_cast<char*>(&option), sizeof(option)) < 0) {
        handle_error("ERROR: setsockopt failed");
    }

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        handle_error("ERROR: Server socket bind failed");
    }

    freeaddrinfo(res);
    return sockfd;
}

// Signal handler for graceful server shutdown
void signal_handler(int sig) {
    std::cout << "\nShutting down server gracefully...\n";
    fflush(stdout);  // Added flushing of stdout
    exit(EXIT_SUCCESS);
}

// Main server function
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "ERROR: Usage: " << argv[0] << " <host:port>\n";
        fflush(stderr);  // Added flushing of stderr
        return EXIT_FAILURE;
    }

    // Parse host and port from command line argument
    char *host = strtok(argv[1], ":");
    char *port = strtok(nullptr, ":");
    if (!host || !port) {
        std::cerr << "ERROR: Invalid host or port format. Use <host:port>\n";
        fflush(stderr);  // Added flushing of stderr
        return EXIT_FAILURE;
    }
    std::cout << "Host: " << host << ", Port: " << port << std::endl;
    fflush(stdout);  // Added flushing of stdout

    // Register signal handler for graceful shutdown
    signal(SIGINT, signal_handler);

    int server_sockfd = initialize_server_socket(host, port);

    // Set the server socket to non-blocking mode
    fcntl(server_sockfd, F_SETFL, O_NONBLOCK);

    // Listen for incoming connections
    if (listen(server_sockfd, MAX_CLIENTS) < 0) {
        handle_error("ERROR: Server listen failed");
    }
    std::cout << "Server listening on " << host << ":" << port << "...\n";
    fflush(stdout);  // Added flushing of stdout

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sockfd < 0) {
            // If no client is ready, continue looping (non-blocking mode)
            continue;
        }

        if ((client_count + 1) == MAX_CLIENTS) {
            std::cerr << "ERROR: Maximum clients reached. Rejected: ";
            std::cerr << ":" << client_addr.sin_port << std::endl;
            fflush(stderr);  // Added flushing of stderr
            close(client_sockfd);
            continue;
        }

        // Send protocol version message
        if (send(client_sockfd, PROTOCOL_MESSAGE, strlen(PROTOCOL_MESSAGE), 0) <= 0) {
            std::cerr << "ERROR: Failed to send protocol message\n";
            fflush(stderr);  // Added flushing of stderr
            close(client_sockfd);
            continue;
        }

        // Handle NICK message
        char nick_buffer[MAX_BUFFER_SIZE] = {0};
        if (recv(client_sockfd, nick_buffer, sizeof(nick_buffer), 0) <= 0) {
            std::cerr << "ERROR: Receiving NICK message failed\n";
            fflush(stderr);  // Added flushing of stderr
            close(client_sockfd);
            continue;
        }

        char client_name[MAX_NAME_LENGTH + 1];  // Ensure space for null terminator
        sscanf(nick_buffer, "NICK %s", client_name);
        client_name[strcspn(client_name, "\n")] = '\0';

        std::regex nickname_regex("^[A-Za-z0-9_]+$");
        if (std::regex_match(client_name, nickname_regex) && strlen(client_name) <= MAX_NAME_LENGTH) {
            if (send(client_sockfd, OK_MESSAGE, strlen(OK_MESSAGE), 0) <= 0) {
                std::cerr << "ERROR: Sending OK message failed\n";
                fflush(stderr);  // Added flushing of stderr
                close(client_sockfd);
                continue;
            }

            // Add client to queue
            auto *client = new Client;
            client->address = client_addr;
            client->sockfd = client_sockfd;
            client->uid = uid++;
            client->name = client_name;

            std::cout << client->name << " joined the chat\n";
            fflush(stdout);  // Added flushing of stdout
            add_client_to_queue(client);
            std::thread(handle_client, client).detach();

        } else {
            // Invalid nickname
            send(client_sockfd, ERROR_MESSAGE, strlen(ERROR_MESSAGE), 0);
            close(client_sockfd);
        }
    }

    close(server_sockfd);
    return 0;
}
