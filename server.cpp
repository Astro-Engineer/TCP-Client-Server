#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/wait.h>
//Added libraries
#include <unordered_map>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
//Set IP and PORT for main server
#define SERVER_IP "127.0.0.1"
#define PORT "23994" 
//Create global variables to hold the data from the list
//2 formats, one for name:server, another for server:names
std::unordered_map<std::string, int> departmentMap;
std::unordered_map<int, std::vector<std::string>> swappedMap;

void handleClient(int newSocket, int& currentClientID);

int main() {
    int currentClientID = 0;
    std::cout<<"Main server is up and running"<<std::endl;
    std::ifstream inputFile("list.txt");  
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return 1;
    }
    std::string line;
    int currentNumber = -1;
    while (std::getline(inputFile, line)) {
        if (line.empty()) {
            continue;
        }
        //Extract number from string
        std::string numberPart;
        size_t pos = 0;
        while (pos < line.length() && isdigit(line[pos])) {
            numberPart += line[pos];
            ++pos;
        }
        if (!numberPart.empty()) {
            currentNumber = std::stoi(numberPart);
        }
        //Split line by semicolon 
        std::istringstream ss(line.substr(pos));
        std::string department;

        while (std::getline(ss, department, ';')) {
            departmentMap[department] = currentNumber;
        }
    }

    inputFile.close();
    std::cout<<"Main server has read the department list from list.txt."<<std::endl;
    
    for (const auto& pair : departmentMap) {
        swappedMap[pair.second].push_back(pair.first);
    }
    std::cout<<"Total number of Backend Servers: "<<swappedMap.size()<<std::endl;
    for (const auto& pair : swappedMap) {
        std::cout << pair.first << " contains "<<pair.second.size()<<" distinct departments"<<std::endl;
    }

    int sockfd, newSocket;
    struct addrinfo hints, *res, *p;
    struct sockaddr_storage clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);
    int yes = 1;
    int rv;

    //Set up hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //Get address info
    if ((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
        return 1;
    }

    //Loop through and bind to the first socket
    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            std::cerr << "server: socket error" << std::endl;
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            std::cerr << "server: setsockopt error" << std::endl;
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            std::cerr << "server: bind error" << std::endl;
            continue;
        }
        break;
    }

    freeaddrinfo(res);
    if (p == NULL) {
        std::cerr << "server: failed to bind" << std::endl;
        return 2;
    }
    if (listen(sockfd, 10) == -1) {
        std::cerr << "server: listen error" << std::endl;
        return 3;
    }
    while (1) {
        newSocket = accept(sockfd, (struct sockaddr *)&clientAddr, &clientAddrSize);
        if (newSocket == -1) {
            std::cerr << "server: accept error" << std::endl;
            continue;
        }
        currentClientID ++;
        if (!fork()) {
            close(sockfd);  
            //Setup instance of client
            handleClient(newSocket, currentClientID);
            close(newSocket);
            exit(0);
        }
        close(newSocket);  
    }
    close(sockfd);
    return 0;
}

void handleClient(int newSocket, int& currentClientID) {
    char buffer[1024];
    int bytesRead;
    //Assign a client ID 
    int clientID = currentClientID;
    //Send the client 
    send(newSocket, &clientID, sizeof(clientID), 0);
    while (1) {
        bytesRead = recv(newSocket, buffer, sizeof(buffer), 0);

        if (bytesRead <= 0) {
            break;
        }
        //Process message 
        std::string receivedMessage(buffer, bytesRead);
        //Extract clientID from message
        int clientIDFromMessage = 0;
        size_t clientIDPos = receivedMessage.find("ClientID:");
        if (clientIDPos != std::string::npos) {
            clientIDPos += strlen("ClientID:");
            clientIDFromMessage = std::stoi(receivedMessage.substr(clientIDPos));
            
        }
        int portID = 0;
        size_t portIDPos = receivedMessage.find("PortID:");
        if (portIDPos != std::string::npos) {
            portIDPos += strlen("PortID:");
            portID = std::stoi(receivedMessage.substr(portIDPos));
            
        }
        receivedMessage = receivedMessage.substr(0, clientIDPos-9);

        std::cout << "Main server has received the request on Department " << 
        receivedMessage << " from client " << clientIDFromMessage << " using TCP over port " <<
        portID << std::endl;
        std::string responseStr;
        
        if (departmentMap.count(receivedMessage) > 0) {
        //Department in server
            int value = departmentMap[receivedMessage];
            responseStr = "Client has received results from Main Server: " + receivedMessage + 
            " is associated with backend server " + std::to_string(value) + ".";
            std::cout<<receivedMessage<<" shows up in backend server "<<std::to_string(value)<<std::endl;
            std::cout<<"The main server has sent searching result to client "<<clientIDFromMessage<<
            " using TCP over port "<<PORT<<std::endl;
        } else {
        //Not in server
            responseStr = receivedMessage + " not found.";
            std::cout<<receivedMessage<<" does not show up in backend server ";
            bool first = true;
            for(const auto& entry : swappedMap){
                if (!first) {
                    std::cout << ", ";
                }
                std::cout << entry.first;
                first = false;
                
            }
            std::cout<<std::endl;
            std::cout<<"The main server has sent \"Department Name: Not found\" to client "<<
            clientIDFromMessage<<" using TCP over port "<<PORT<<std::endl;
        }
        const char *response = responseStr.c_str();
        //Send a response back to client
        send(newSocket, response, strlen(response), 0);
    }
}