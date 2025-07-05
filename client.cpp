#include <iostream>
#include <sys/socket.h>     //main header file for socket programming
#include <netinet/in.h>     //for internet address family 
#include <arpa/inet.h>     
#include <unistd.h>         //only use so far - close(client_socket) uni as in UNIX      
#include <cstring>          //for c style null terminated strings (duh)

#define SERVER_IP "127.0.0.1"       //local host
#define PORT 8080                   //some port to listen for messages
#define BUFFER_SIZE 1024           //message buffer size

int main(){
    int client_socket;      //this is the socket aka gateway - basically the "door"
    struct sockaddr_in server_addr; // Structure to hold server's address information ; comes from <netinet/in.h>
    char buffer[BUFFER_SIZE] = {0};     //initialize entire array with 0

    //Create a socket

    if((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error: Socket creatoin failed\n");  //basically prints this message with the error
        return EXIT_FAILURE;             //basically "return 1"
    }

    server_addr.sin_family = AF_INET;       //af_inet tells us we're using IPv4 ; https://www.gta.ufrj.br/ensino/eel878/sockets/sockaddr_inman.html
    server_addr.sin_port = htons(PORT);     //htons converts PORT Number to network byte order

    //Check for server address

    if(inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <=0){
        perror("Error: Invalid address\n");
        close(client_socket);
        return EXIT_FAILURE;
    }

    //YOU FORGOT TO CONNECT CLIENT TO SERVER BRUH

    if(connect(client_socket, (struct sockaddr*)& server_addr, sizeof(server_addr))){
        perror("Connectoin Failed\n");
        close(client_socket);
        return EXIT_FAILURE;
    }

    std::cout << "Connected to server\n";
    std::cout << "Server IP: " << SERVER_IP << " : " << PORT << '\n';
    std::cout << "Type 'leave' to disconnect\n";

    std::string message;        //don't initialize it

    //Infinite loop to send messages 

    while(true){
        //inifinite loop to accept message 
        std::cout << "You: ";
        std::getline(std::cin, message);

        if(message == "leave"){
            std::cout << "Disconnecting...\n";
            break;          //exiting loop here
        }

        //hey hey: ssize_t is just size_t with a sign!
        ssize_t bytes_tosend = send(client_socket, message.c_str(), message.length(), 0);
        if(bytes_tosend == -1){
            perror("Error: Send failed");
            break;      //exit loop duh
        }
        else if(bytes_tosend == 0){
            std::cout << "Server unexpectedly closed connection\n";
            break;
        }

        //basically copies character (here ASCII 0) to buffer of size buffer_size
        //so we're gonna clear the buffer - that's what memset does
        std::memset(buffer, 0, BUFFER_SIZE);        //found in <cstring>
        
        //buffer_size - 1 to store the null terminator 
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if(bytes_received == -1){
            perror("Error: Receive failed\n");
            break;
        }
        else if(bytes_received == 0){
            std::cout << "Server disconnected\n";
            break;
        }
        else{
            buffer[bytes_received] = '\0';      //DON'T FORGET TO APPEND NULL TERMINATOR
            std::cout << "Server: " << buffer << '\n';
        }
    }


    //outside the while loop you need to close the socket
    close(client_socket);
    std::cout << "Socket closed\n";
    return 0;   
}



