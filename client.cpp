#include <iostream>
#include <string>
#include <regex>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sstream>  // For std::istringstream

#define MAX_MESSAGE_LENGTH 2048
#define MAX_NAME_LENGTH 12

std::atomic<bool> isRunning(true);  // Flag to control the running state
int serverSocket = 0;  // Socket descriptor for server communication
std::string username;  // Client's nickname

// Function to flush the standard output
void flushOutput() {
    std::cout.flush();
}

// Function to send messages to the server
void sendMessage() {
    std::string message;

    while (isRunning) {
        flushOutput();  // Ensure output is flushed before user input
        std::getline(std::cin, message);  // Read user input

        // Check if message exceeds maximum length
        if (message.length() > 255) {
            std::cout << "Message too long (" << message.length() << " characters). Please limit to 255 characters." << std::endl;
            continue;
        }

        // Create the message protocol and send to the server
        std::string protocolMessage = "MSG " + message + "\n";
        send(serverSocket, protocolMessage.c_str(), protocolMessage.length(), 0);
    }
}

// Function to receive messages from the server
void receiveMessage() {
    char buffer[MAX_MESSAGE_LENGTH] = {};

    while (isRunning) {
        int receive = recv(serverSocket, buffer, MAX_MESSAGE_LENGTH, 0);  // Receive message from server

        if (receive > 0) {
            std::string message(buffer);

            // Parse protocol, username, and message content
            std::string protocol, senderUsername, content;
            std::istringstream iss(message);
            iss >> protocol >> senderUsername;
            std::getline(iss, content);

            // Handle message according to protocol
            if (protocol == "MSG") {
                // Print the sender's username and the message
                std::cout << "[" << senderUsername << "]: " << content.substr(1) << std::endl;  // Remove leading space from content
            } else if (protocol == "JOIN") {
                // Print a message indicating that a user has joined the chat
                std::cout << senderUsername << " has joined the chat." << std::endl;
            } else if (protocol == "EXIT") {
                // Print a message indicating that a user has left the chat
                std::cout << senderUsername << " has left the chat." << std::endl;
            } else if (protocol == "ERROR" && senderUsername == username) {
                std::cout << username << ": ERROR - Only 255 characters allowed in a message." << std::endl;
            }
            flushOutput();
        } else if (receive == 0) {
            // Server disconnected
            std::cout << "Server disconnected. Exiting chat..." << std::endl;
            isRunning = false;
            break;
        }
        memset(buffer, 0, sizeof(buffer));  // Clear buffer
    }
}

// Main function
int main(int argc, char *argv[]) {
    // Check command-line arguments
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <IP:Port> <Nickname>" << std::endl;
        return 0;
    }

    // Parse server IP and port from arguments
    std::string host, port;
    std::string serverInfoString = argv[1];  // Renamed to avoid conflict
    size_t colonPos = serverInfoString.find(':');
    if (colonPos == std::string::npos) {
        std::cerr << "ERROR: Invalid format. Use <IP:Port>" << std::endl;
        return 1;
    }
    host = serverInfoString.substr(0, colonPos);
    port = serverInfoString.substr(colonPos + 1);

    // Validate the nickname using regex
    std::string nickname = argv[2];
    if (nickname.length() >= MAX_NAME_LENGTH || !std::regex_match(nickname, std::regex("^[A-Za-z0-9_]+$"))) {
        std::cerr << "ERROR: Invalid nickname. Must be less than " << MAX_NAME_LENGTH << " characters and contain only letters, numbers, or underscores." << std::endl;
        return 1;
    }
    username = nickname;

    // Display connection information
    std::cout << "Connecting to " << host << ":" << port << " as " << username << "..." << std::endl;

    // Setup server address info
    struct addrinfo hints{}, *serverInfo;  // Corrected declaration
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve server address
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &serverInfo) != 0) {
        std::cerr << "ERROR: Error resolving server address." << std::endl;
        return 1;
    }

    // Create a socket
    serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
    if (serverSocket == -1) {
        std::cerr << "ERROR: Error creating socket." << std::endl;
        freeaddrinfo(serverInfo);
        return 1;
    }

    // Connect to server
    if (connect(serverSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) == -1) {
        std::cerr << "ERROR: Error connecting to server." << std::endl;
        close(serverSocket);
        freeaddrinfo(serverInfo);
        return 1;
    }
    freeaddrinfo(serverInfo);

    // Read server protocol greeting
    char serverProtocol[MAX_MESSAGE_LENGTH] = {};
    if (read(serverSocket, serverProtocol, sizeof(serverProtocol)) <= 0) {
        std::cerr << "ERROR: Error reading server protocol." << std::endl;
        close(serverSocket);
        return 1;
    }
    std::cout << "Server Protocol: " << serverProtocol;

    // Verify server protocol
    if (std::string(serverProtocol) != "HELLO 1\n") {
        std::cerr << "ERROR: Server protocol not supported." << std::endl;
        close(serverSocket);
        return 1;
    }

    // Send nickname to server
    std::string nicknameProtocolMessage = "NICK " + username + "\n";
    send(serverSocket, nicknameProtocolMessage.c_str(), nicknameProtocolMessage.length(), 0);

    // Receive server response to nickname
    char response[MAX_MESSAGE_LENGTH] = {};
    if (read(serverSocket, response, sizeof(response)) <= 0) {
        std::cerr << "ERROR: Error reading server response." << std::endl;
        close(serverSocket);
        return 1;
    }

    // Check if nickname is accepted
    if (std::string(response) != "OK\n") {
        std::cerr << "ERROR: Nickname not accepted by server." << std::endl;
        close(serverSocket);
        return 1;
    }

    std::cout << "Welcome to the chat!" << std::endl;

    // Create threads for sending and receiving messages
    std::thread sendThread(sendMessage);
    std::thread receiveThread(receiveMessage);

    // Main loop to keep the application running
    while (isRunning) {
        // Exit handling
        std::string command;
        std::getline(std::cin, command);
        if (command == "/quit") {
            isRunning = false;
        }
    }

    // Close socket and exit
    close(serverSocket);
    sendThread.join();
    receiveThread.join();
    std::cout << "Disconnected from server." << std::endl;
    return 0;
}
