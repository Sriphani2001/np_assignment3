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
#include <sstream>

#define MAX_MESSAGE_LENGTH 2048
#define MAX_NAME_LENGTH 12

using namespace std;

atomic<bool> isRunning(true);  // controls the running state
int serverSocket = 0;  // socket descriptor for server communication
string username;  // client's nickname

// flush the standard output
void flushOutput() {
    cout.flush();
}

// signal handler for graceful shutdown on sigint (ctrl+c)
void signalHandler(int signum) {
    isRunning = false;
    close(serverSocket);
    cout << "exiting chat..." << endl;
    exit(signum);
}

// sends messages to the server
void sendMessage() {
    string message;
    while (isRunning) {
        flushOutput();  // flush output before user input
        getline(cin, message);  // read user input

        // Remove the message length restriction
        // if (message.length() > 255) {
        //     cout << "message too long (" << message.length() << " characters). please limit to 255 characters." << endl;
        //     flushOutput();
        //     continue;
        // }

        string protocolMessage = "MSG " + message + "\n";
        if (send(serverSocket, protocolMessage.c_str(), protocolMessage.length(), 0) == -1) {
            cerr << "error: failed to send message to server." << endl;
            isRunning = false;
            break;
        }
    }
}

// receives messages from the server and handles TCP message fragmentation
void receiveMessage() {
    char buffer[MAX_MESSAGE_LENGTH] = {};
    string partialMessage = "";  // Buffer to hold partial messages

    while (isRunning) {
        int receive = recv(serverSocket, buffer, MAX_MESSAGE_LENGTH, 0);
        if (receive > 0) {
            partialMessage += string(buffer, receive);  // Append received data to buffer

            size_t newlinePos;
            // Process each complete message (terminated by \n)
            while ((newlinePos = partialMessage.find('\n')) != string::npos) {
                string completeMessage = partialMessage.substr(0, newlinePos); // Get the complete message
                partialMessage.erase(0, newlinePos + 1);  // Remove processed message from buffer

                // Now process the complete message
                string protocol, senderUsername, content;
                istringstream iss(completeMessage);
                iss >> protocol >> senderUsername;
                getline(iss, content);

                // Check if the message is using the MSG protocol
                if (protocol == "MSG") {
                    cout << "[" << senderUsername << "]: " << content.substr(1) << endl;  // Print the message content
                } else if (protocol == "JOIN") {
                    cout << senderUsername << " has joined the chat." << endl;
                } else if (protocol == "EXIT") {
                    cout << senderUsername << " has left the chat." << endl;
                } else if (protocol == "ERROR" && senderUsername == username) {
                    cout << username << ": error - only 255 characters allowed in a message." << endl;
                }
                flushOutput();
            }
        } else if (receive == 0) {
            cout << "server disconnected. exiting chat..." << endl;
            isRunning = false;
            break;
        } else {
            cerr << "error: failed to receive message from server." << endl;
            isRunning = false;
            break;
        }
        memset(buffer, 0, sizeof(buffer));  // Clear buffer after each message
    }
}

// main function
int main(int argc, char *argv[]) {
    // set up signal handler for ctrl+c
    signal(SIGINT, signalHandler);

    if (argc < 3) {
        cout << "usage: " << argv[0] << " <ip:port> <nickname>" << endl;
        return 0;
    }

    string host, port;
    string serverInfoString = argv[1];
    size_t colonPos = serverInfoString.find(':');
    if (colonPos == string::npos) {
        cerr << "error: invalid format. use <ip:port>" << endl;
        return 1;
    }
    host = serverInfoString.substr(0, colonPos);
    port = serverInfoString.substr(colonPos + 1);

    string nickname = argv[2];
    if (nickname.length() >= MAX_NAME_LENGTH || !regex_match(nickname, regex("^[A-Za-z0-9_]+$"))) {
        cerr << "error: invalid nickname. must be less than " << MAX_NAME_LENGTH << " characters and contain only letters, numbers, or underscores." << endl;
        return 1;
    }
    username = nickname;

    cout << "connecting to " << host << ":" << port << " as " << username << "..." << endl;

    struct addrinfo hints{}, *serverInfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &serverInfo) != 0) {
        cerr << "error: error resolving server address." << endl;
        return 1;
    }

    serverSocket = socket(serverInfo->ai_family, serverInfo->ai_socktype, serverInfo->ai_protocol);
    if (serverSocket == -1) {
        cerr << "error: error creating socket." << endl;
        freeaddrinfo(serverInfo);
        return 1;
    }

    if (connect(serverSocket, serverInfo->ai_addr, serverInfo->ai_addrlen) == -1) {
        cerr << "error: error connecting to server." << endl;
        close(serverSocket);
        freeaddrinfo(serverInfo);
        return 1;
    }
    freeaddrinfo(serverInfo);

    char serverProtocol[MAX_MESSAGE_LENGTH] = {};
    if (read(serverSocket, serverProtocol, sizeof(serverProtocol)) <= 0) {
        cerr << "error: error reading server protocol." << endl;
        close(serverSocket);
        return 1;
    }
    cout << "server protocol: " << serverProtocol;
    flushOutput();

    if (string(serverProtocol).find("HELLO 1") == string::npos) {
        cerr << "error: server protocol not supported." << endl;
        close(serverSocket);
        return 1;
    }

    string nicknameProtocolMessage = "NICK " + username + "\n";
    if (send(serverSocket, nicknameProtocolMessage.c_str(), nicknameProtocolMessage.length(), 0) == -1) {
        cerr << "error: failed to send nickname to server." << endl;
        close(serverSocket);
        return 1;
    }

    char response[MAX_MESSAGE_LENGTH] = {};
    if (read(serverSocket, response, sizeof(response)) <= 0) {
        cerr << "error: error reading server response." << endl;
        close(serverSocket);
        return 1;
    }

    if (string(response).find("OK") == string::npos) {
        cerr << "error: nickname not accepted by server." << endl;
        close(serverSocket);
        return 1;
    }

    cout << "welcome to the chat!" << endl;
    flushOutput();

    thread sendThread(sendMessage);
    receiveMessage();  // Handle receiving messages in the main thread

    sendThread.join();
    close(serverSocket);  // close the socket when done
    return 0;
}
