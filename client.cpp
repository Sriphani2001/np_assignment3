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
#include <vector>

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

// Helper function to remove the "MSG <nickname>" prefix
string stripMessagePrefix(const string& message) {
    size_t firstSpace = message.find(' ');
    if (firstSpace == string::npos) return message;  // Return the message as-is if there's no space
    size_t secondSpace = message.find(' ', firstSpace + 1);
    if (secondSpace == string::npos) return message;  // Return the message as-is if there's no second space

    // Only strip if the message starts with "MSG "
    if (message.substr(0, firstSpace) == "MSG") {
        return message.substr(secondSpace + 1);  // Return the content after "MSG <nickname> "
    }

    // If no "MSG" is found, return the original message
    return message;
}

// Helper function to split a string by delimiter
vector<string> split(const string& s, const string& delimiter) {
    vector<string> tokens;
    size_t start = 0, end = 0;
    while ((end = s.find(delimiter, start)) != string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + delimiter.length();
    }
    tokens.push_back(s.substr(start));  // Add the last token
    return tokens;
}

// receives messages from the server and handles TCP message fragmentation
void receiveMessage() {
    char buffer[MAX_MESSAGE_LENGTH] = {};
    string partialMessage = "";  // Buffer to hold partial messages

    while (isRunning) {
        int receive = recv(serverSocket, buffer, MAX_MESSAGE_LENGTH, 0);
        if (receive > 0) {
            partialMessage += string(buffer, receive);  // Append received data to buffer

            // Process complete messages (split by newline)
            vector<string> messages = split(partialMessage, "\n");
            
            // The last message might be incomplete, so we keep it in the partial buffer
            partialMessage = messages.back();
            messages.pop_back();  // Remove the incomplete message from processing

            // Process each complete message
            for (const string& completeMessage : messages) {
                if (completeMessage.empty()) continue;

                // Strip the "MSG <nickname>" prefix once and print the message
                string strippedMessage = stripMessagePrefix(completeMessage);
                cout << strippedMessage << endl;  // Print the stripped message (content only)

                // If the server sends the special disconnect message or closes the connection:
                if (completeMessage == "QUIT") {
                    isRunning = false;
                    break;
                }
            }
        } else if (receive == 0) {
            // Connection closed by server
            cout << username << ": server disconnected. exiting chat..." << endl;
            isRunning = false;
        } else {
            cerr << "error: failed to receive message from server." << endl;
            isRunning = false;
        }

        // Clear the buffer after each use
        memset(buffer, 0, MAX_MESSAGE_LENGTH);
    }
}

// sends messages to the server
void sendMessage() {
    string message;
    while (isRunning) {
        flushOutput();  // flush output before user input
        getline(cin, message);  // read user input

        // Check if the input is a raw message like "2C7ABE39", which should be sent as-is
        if (message == "2C7ABE39") {
            // Send the raw message without adding "MSG <nickname>"
            if (send(serverSocket, message.c_str(), message.length(), 0) == -1) {
                cerr << "error: failed to send raw message to server." << endl;
                isRunning = false;
                break;
            }
        } else {
            // Otherwise, send the message with the "MSG <nickname>" prefix
            string protocolMessage = "MSG " + username + " " + message + "\n";
            if (send(serverSocket, protocolMessage.c_str(), protocolMessage.length(), 0) == -1) {
                cerr << "error: failed to send message to server." << endl;
                isRunning = false;
                break;
            }
        }
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
    int bytesReceived = recv(serverSocket, serverProtocol, sizeof(serverProtocol), 0);
    if (bytesReceived <= 0) {
        cerr << "error: error reading server protocol." << endl;
        close(serverSocket);
        return 1;
    }
    cout << "server protocol: " << string(serverProtocol, bytesReceived);
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
    bytesReceived = recv(serverSocket, response, sizeof(response), 0);  // Use recv instead of read
    if (bytesReceived <= 0) {
        cerr << "error: error reading server response." << endl;
        close(serverSocket);
        return 1;
    }

    // Process the server's response message (combined OK and fake message)
    string responseStr(response, bytesReceived);
    cout << "server response: " << responseStr;

    if (responseStr.find("OK") != string::npos) {
        cout << "welcome to the chat!" << endl;
    }

    // Handle the rest of the messages
    thread sendThread(sendMessage);
    receiveMessage();  // Handle receiving messages in the main thread

    sendThread.join();
    close(serverSocket);  // close the socket when done
    return 0;
}
