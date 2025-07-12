#include <iostream>
#include <sys/socket.h>     //main header file for socket programming
#include <netinet/in.h>     //for internet address family 
#include <arpa/inet.h>     
#include <unistd.h>         //only use so far - close(client_socket) uni as in UNIX      
#include <cstring>          //for c style null terminated strings (duh)
#include <mutex>
#include <thread>
#include <string>

#define SERVER_IP "127.0.0.1"       //local host
#define PORT 8080                   //some port to listen for messages
#define BUFFER_SIZE 1024           //message buffer size

std::mutex count_mutex;

void send_message(int client_socket){
    std::string message;
    while(true){
        {   //put std::lock_guard in parenthesis so that it goes out of scope
            //Thus "You" will only be printed when the console is free
            std::lock_guard<std::mutex> lock(count_mutex);
            std::cout << "You: ";
        }
        std::getline(std::cin, message);

        if(message == "exit"){
            {
                std::lock_guard<std::mutex> lock(count_mutex);
                std::cout << "Disconnecting...\n";
            }
            break;

        }

        //will send to server, we don't need to check
        ssize_t bytesToSend = send(client_socket, message.c_str(), message.length(), 0);
        if(bytesToSend < 0){
            std::lock_guard<std::mutex> lock(count_mutex);
            perror("Could not send message\n");
            break;
        }
        else if(bytesToSend == 0){
            {
                std::lock_guard<std::mutex> lock(count_mutex);
                perror("Server has disconnected\n");
            }
            break;
        }
    }
}

void receive_message(int client_socket){
    char buffer[BUFFER_SIZE]; //local buffer
    while(true){
        std::memset(buffer, 0, BUFFER_SIZE);

        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes_received < 0){
            std::lock_guard<std::mutex> lock(count_mutex);
            perror("Failed to receive data from the server\n");
            break;
        }
        else if(bytes_received == 0){
            perror("Server disconnected\n");
            break;
        }
        else{
            buffer[BUFFER_SIZE] = '\0';
            {
                std::lock_guard<std::mutex> lock(count_mutex);
                std::cout << "Server: " << buffer << '\n';
                // std::flush; ?
            }
        }
    }

}

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
    std::cout << "Type 'exit' to disconnect\n";

    // std::string message;        //don't initialize it

    //so the function we want to parallize + the parameter
    std::thread send_thread(send_message, client_socket);
    std::thread receive_thread(receive_message, client_socket);

    send_thread.join();

    //outside the while loop you need to close the socket
    close(client_socket);
    std::cout << "Socket closed\n";

    receive_thread.join();
    std::cout << "Exiting application\n";
    return 0;   
}



