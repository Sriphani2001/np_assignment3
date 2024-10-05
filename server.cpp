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

using namespace std;

atomic<unsigned int> client_count(0);
int uid = 10;

// client structure
struct Client {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    string name;
};

vector<Client*> clients(MAX_CLIENTS, nullptr);
mutex clients_mutex;

// utility function to handle errors
void handle_error(const string &message) {
    perror(message.c_str());
    exit(EXIT_FAILURE);
}

// add client to the client list
void add_client_to_queue(Client *client) {
    lock_guard<mutex> lock(clients_mutex);
    for (auto &c : clients) {
        if (!c) {
            c = client;
            break;
        }
    }
}

// remove client from the client list
void remove_client_from_queue(int uid) {
    lock_guard<mutex> lock(clients_mutex);
    for (auto &c : clients) {
        if (c && c->uid == uid) {
            c = nullptr;
            break;
        }
    }
}

// send message to all clients except the sender
void send_message_to_all(const string &message, int sender_uid) {
    lock_guard<mutex> lock(clients_mutex);
    for (auto &c : clients) {
        if (c && c->uid != sender_uid) {
            if (send(c->sockfd, message.c_str(), message.length(), 0) <= 0) {
                cout << "error: failed to send message to client (uid=" << c->uid << ")\n";
                fflush(stdout);  // flush to ensure output is shown immediately
                break;
            }
        }
    }
}

// handle client communication
void handle_client(Client *client) {
    char buffer[MAX_BUFFER_SIZE];
    bool leave_flag = false;

    client_count++;

    while (true) {
        if (leave_flag) break;

        int receive = recv(client->sockfd, buffer, MAX_BUFFER_SIZE, 0);
        if (receive > 0) {
            if (strlen(buffer) > 0) {
                // validate and parse the incoming message
                string buffer_str(buffer);
                if (buffer_str.rfind("MSG ", 0) == 0) { // ensure the message starts with "MSG "
                    string message = buffer_str.substr(4);  // extract message after "MSG "
                    message.erase(message.find_last_not_of(" \n\r\t") + 1);  // remove trailing newline character

                    if (message.length() <= 255) {
                        string formatted_message = "MSG " + client->name + " " + message + "\n";
                        cout << client->name << ": " << message << endl;
                        fflush(stdout);  // flush stdout to ensure it is shown immediately
                        send_message_to_all(formatted_message, client->uid);
                    } else {
                        string error_message = "ERROR " + client->name + ": message too long\n";
                        send_message_to_all(error_message, client->uid);
                    }
                } else {
                    // invalid message format
                    string error_message = "ERROR invalid message format\n";
                    send(client->sockfd, error_message.c_str(), error_message.length(), 0);
                }
            }
        } else if (receive == 0) {
            cout << client->name << " left the chat\n";
            fflush(stdout);  // flush stdout
            string leave_message = "MSG " + client->name + " has left the chat\n";
            send_message_to_all(leave_message, client->uid);
            leave_flag = true;
        } else {
            cout << "error: client (uid=" << client->uid << ") communication error\n";
            fflush(stdout);  // flush stdout
            leave_flag = true;
        }
        memset(buffer, 0, MAX_BUFFER_SIZE); // clear buffer
    }

    close(client->sockfd);
    remove_client_from_queue(client->uid);
    delete client;
    client_count--;
}

// initialize server socket with retries for socket creation, setting options, and binding
int initialize_server_socket(const char *host, const char *port) {
    struct addrinfo hints{}, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int sockfd;
    int retry_count = 5;
    while (retry_count--) {
        if (getaddrinfo(host, port, &hints, &res) != 0) {
            cerr << "error: failed to resolve socket address. retrying...\n";
            fflush(stderr);  // flush stderr
            sleep(1);
            continue;
        }

        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            cerr << "error: server socket creation failed. retrying...\n";
            fflush(stderr);  // flush stderr
            freeaddrinfo(res);
            sleep(1);
            continue;
        }

        int option = 1;
        if (setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), reinterpret_cast<char*>(&option), sizeof(option)) < 0) {
            cerr << "error: setsockopt failed. retrying...\n";
            fflush(stderr);  // flush stderr
            close(sockfd);
            freeaddrinfo(res);
            sleep(1);
            continue;
        }

        if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
            cerr << "error: server socket bind failed. retrying...\n";
            fflush(stderr);  // flush stderr
            close(sockfd);
            freeaddrinfo(res);
            sleep(1);
            continue;
        }

        freeaddrinfo(res);
        return sockfd; // successful socket creation and binding
    }

    handle_error("fatal error: server socket initialization failed after retries");
    return -1; // should never reach here due to exit in handle_error
}

// signal handler for graceful server shutdown
void signal_handler(int sig) {
    cout << "\nshutting down server gracefully...\n";
    fflush(stdout);  // flush stdout
    exit(EXIT_SUCCESS);
}

// main server function
int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "error: usage: " << argv[0] << " <host:port>\n";
        fflush(stderr);  // flush stderr
        return EXIT_FAILURE;
    }

    // parse host and port from command line argument
    char *host = strtok(argv[1], ":");
    char *port = strtok(nullptr, ":");
    if (!host || !port) {
        cerr << "error: invalid host or port format. use <host:port>\n";
        fflush(stderr);  // flush stderr
        return EXIT_FAILURE;
    }
    cout << "host: " << host << ", port: " << port << endl;
    fflush(stdout);  // flush stdout

    // register signal handler for graceful shutdown
    signal(SIGINT, signal_handler);

    int server_sockfd = initialize_server_socket(host, port);

    // set the server socket to non-blocking mode
    fcntl(server_sockfd, F_SETFL, O_NONBLOCK);

    // listen for incoming connections
    if (listen(server_sockfd, MAX_CLIENTS) < 0) {
        handle_error("error: server listen failed");
    }
    cout << "server listening on " << host << ":" << port << "...\n";
    fflush(stdout);  // flush stdout

    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sockfd < 0) {
            // if no client is ready, continue looping (non-blocking mode)
            continue;
        }

        if ((client_count + 1) == MAX_CLIENTS) {
            cerr << "error: maximum clients reached. rejected: ";
            cerr << ":" << client_addr.sin_port << endl;
            fflush(stderr);  // flush stderr
            close(client_sockfd);
            continue;
        }

        // send protocol version message
        if (send(client_sockfd, PROTOCOL_MESSAGE, strlen(PROTOCOL_MESSAGE), 0) <= 0) {
            cerr << "error: failed to send protocol message\n";
            fflush(stderr);  // flush stderr
            close(client_sockfd);
            continue;
        }

        // handle NICK message
        char nick_buffer[MAX_BUFFER_SIZE] = {0};
        if (recv(client_sockfd, nick_buffer, sizeof(nick_buffer), 0) <= 0) {
            cerr << "error: receiving NICK message failed\n";
            fflush(stderr);  // flush stderr
            close(client_sockfd);
            continue;
        }

        char client_name[MAX_NAME_LENGTH + 1];  // ensure space for null terminator
        sscanf(nick_buffer, "NICK %s", client_name);
        client_name[strcspn(client_name, "\n")] = '\0';

        regex nickname_regex("^[A-Za-z0-9_]+$");
        if (regex_match(client_name, nickname_regex) && strlen(client_name) <= MAX_NAME_LENGTH) {
            if (send(client_sockfd, OK_MESSAGE, strlen(OK_MESSAGE), 0) <= 0) {
                cerr << "error: sending OK message failed\n";
                fflush(stderr);  // flush stderr
                close(client_sockfd);
                continue;
            }

            // add client to queue
            auto *client = new Client;
            client->address = client_addr;
            client->sockfd = client_sockfd;
            client->uid = uid++;
            client->name = client_name;

            cout << client->name << " joined the chat\n";
            fflush(stdout);  // flush stdout
            add_client_to_queue(client);
            thread(handle_client, client).detach();

        } else {
            // invalid nickname
            send(client_sockfd, ERROR_MESSAGE, strlen(ERROR_MESSAGE), 0);
            close(client_sockfd);
        }
    }

    close(server_sockfd);
    return 0;
}
