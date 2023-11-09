#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstdlib>
#include <ctime>
//Set IP for Server (not Port)
#define SERVER_IP "127.0.0.1"

int main() {
    int clientSocket;
    struct addrinfo hints, *res, *p;
    int status;
    char buffer[1024];
    int numBytes;
    int clientID;
    //Set up hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(SERVER_IP, "0", &hints, &res)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
        return 1;
    }
    //Loop through and bind to the first available local port
    for (p = res; p != nullptr; p = p->ai_next) {
        if ((clientSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;  
        }
        if (bind(clientSocket, p->ai_addr, p->ai_addrlen) == 0) {
            break;  
        }
        close(clientSocket);  
    }
    if (p == nullptr) {
        std::cerr << "Client: failed to bind to a local port." << std::endl;
        return 2;
    }
    //Retrieve the name of the socket to get assigned port
    struct sockaddr my_addr;
    socklen_t addrlen = sizeof(my_addr);
    int getsock_check = getsockname(clientSocket, &my_addr, &addrlen);

    if (getsock_check == -1) {
        perror("getsockname");
        exit(1);
    }
    int localPort = ntohs(((struct sockaddr_in *)&my_addr)->sin_port);
    freeaddrinfo(res);  
    // Connect to server
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(23994); 
    inet_pton(AF_INET, SERVER_IP, &serverAddress.sin_addr);

    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        close(clientSocket);
        std::cerr << "Client: connect error" << std::endl;
        return 3;
    }
    //Receive the client ID
    int receivedBytes = recv(clientSocket, &clientID, sizeof(clientID), 0);

    if (receivedBytes == sizeof(clientID)) {
    } else {
        std::cerr << "Failed to receive the client ID from the server." << std::endl;
        close(clientSocket);
        return 3;
    }
    std::cout << "Client is up and running." <<std::endl;
    std::string message;
    while (true) {
    std::cout << "Enter Department Name: ";
    std::getline(std::cin, message);
    message = message + "ClientID:" + std::to_string(clientID) + "PortID:" + std::to_string(localPort);

    //Send the message to server
    send(clientSocket, message.c_str(), message.size(), 0);

    size_t clientIDPos = message.find("ClientID:");
    clientIDPos += strlen("ClientID:");
    std::string parsedMessage = message.substr(0, clientIDPos-9);

    std::cout<<"Client has sent Department "<<parsedMessage<<" to Main Server using TCP."<<std::endl;
    //Receive response from the server
    numBytes = recv(clientSocket, buffer, sizeof(buffer), 0);

    if (numBytes <= 0) {
        std::cerr << "Connection closed or error in receiving data." << std::endl;
        break;
    }

    //Process the received response
    buffer[numBytes] = '\0';
    std::cout << buffer << std::endl;
    std::cout<<"-----Start a new query-----"<<std::endl;
    }
    close(clientSocket);
    return 0;
}



