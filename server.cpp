#include <iostream>
#include <netinet/in.h>
#include <unistd.h>         //for close()
#include <arpa/inet.h>
#include <cstring>

#define SERVER_IP "127.0.0.1"
#define PORT 8080       //a free port for local hosting
#define BUFFER_SIZE 1024
#define TOTAL_CONNECTIONS 5

int main(){
    int server_fd;          //literally our server socket bruh
    int incoming_socket;          //gonna be our server gateway
    struct sockaddr_in client_addr, server_addr;
    {
        //how sockaddr_in is defined
        // sin_port = port number 
        // sin_addr = internet adress 
        // sin_zero = size of sockadd
    }
    char buffer[BUFFER_SIZE] = {0};
    socklen_t addr_len = sizeof(client_addr);

    //Create a socket

    //the 0 basically tells system to choose default (TCP)
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error: Server socket not created\n");
        return EXIT_FAILURE;
    }

    //Initialize Server address structure

    server_addr.sin_family = AF_INET; 
    server_addr.sin_addr.s_addr = INADDR_ANY;   //listen on all networks
    server_addr.sin_port = htons(PORT);//gotta be different from client port

    //we don't need to look for client, client comes to us
    
    //Binding Socket

    if(bind(server_fd, (struct sockaddr*)& server_addr, sizeof(server_addr)) < 0){
        perror("Binding failed\n");
        close(server_fd);
    }

    //Listen to incoming connectoins

    if(listen(server_fd, TOTAL_CONNECTIONS) < 0){
        perror("Error: Listen failed\n");
        close(server_fd); 
        return EXIT_FAILURE;
    }

    std::cout << "Listening on Port " << PORT << '\n';

    incoming_socket = accept(server_fd, (struct sockaddr*)& client_addr, &addr_len);
    if(incoming_socket < 0){
        std::cout << "Aceeptance Failed\n";
        close(server_fd);
        return EXIT_FAILURE;
    }

    
    std::cout << "Client connected from " << inet_ntoa(client_addr.sin_addr) << " : "; //inet_ntoa returns in string readable form
    std::cout << ntohs(client_addr.sin_port) << '\n';
    std::cout << "Type 'disconnect' to disconnect the client\n";

    //Infinite loop to accept messages from the client
    
    std::string message;

    while(true){
        std::memset(buffer, 0, BUFFER_SIZE);        //clearing buffer like in client
        
        ssize_t bytes_received = recv(incoming_socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes_received < 0){
            perror("Receive Failed\n");
            break;
        }
        else if(bytes_received == 0){
            std::cout << "Client Disconnected\n";
            break;
        }
        else{
            buffer[BUFFER_SIZE] = '\0';
            std::cout << "Client: " << buffer << '\n';

            if(std::string(buffer) == "exit"){
                std::cout << "Client side disconnection.\n";
                break;
            }
        }
// ---------------------ONLY FOR TESTING----------------------
// ---------------------Only Clients send messages----------------
    
        std::cout << "Server: ";
        std::getline(std::cin, message);        //use the message variable to do the same
        if(message == "disconnect"){
            std::cout << "Closing Server\n";
            break;
        }
        
        ssize_t bytes_sent = send(incoming_socket, message.c_str(), message.length(), 0);
        if(bytes_sent == -1){
            perror("Failed to send message\n");
            // close(server_fd);    already done outside the loop
            break;
        }
        else if(bytes_sent == 0){
            perror("Client closed connection\n");
            break;
        } 
        
    }

    close(incoming_socket);
    close(server_fd);
    std::cout << "End of all communications\n";
    return 0;
}