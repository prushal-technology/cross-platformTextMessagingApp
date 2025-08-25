#include <iostream>
#include <sys/socket.h>     //main header file for socket programming
#include <netinet/in.h>     //for internet address family 
#include <arpa/inet.h>     
#include <unistd.h>         //only use so far - close(client_socket) uni as in UNIX      
#include <cstring>          //for c style null terminated strings (duh)

#include <mutex>
#include <thread>
#include <string>
#include <atomic>

#define SERVER_IP "127.0.1.1"       //local host
#define PORT 8080                   //some port to listen for messages
#define BUFFER_SIZE 1024           //message buffer size

std::mutex console_mutex;
std::atomic<bool> client_active(true);

void send_message(int client_socket){
    std::string message;

    //handshake + prompting user for username
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "Please enter a username: ";
    }
    std::getline(std::cin, message);

    // Send the username to the server
    ssize_t bytes_sent = send(client_socket, message.c_str(), message.length(), 0);
    if(bytes_sent < 0){
        std::lock_guard<std::mutex> lock(console_mutex);
        perror("Error sending username\n");
        return;
    }

    // Main message loop
    while(client_active.load()){
        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "You: ";
        }
        std::getline(std::cin, message);

        if(message == "exit"){
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "Disconnecting...\n";
            }
            bytes_sent = send(client_socket, message.c_str(), message.length(), 0);
            if(bytes_sent < 0){
                std::lock_guard<std::mutex> lock(console_mutex);
                perror("Error sending 'exit' message\n");
            }
            client_active.store(false);
            break;
        }

        ssize_t bytes_to_send = send(client_socket, message.c_str(), message.length(), 0);
        if(bytes_to_send < 0){
            std::lock_guard<std::mutex> lock(console_mutex);
            perror("Could not send message\n");
            break;
        }
        else if(bytes_to_send == 0){
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                perror("Server has disconnected\n");
                client_active.store(false);
            }
            break;
        }
    }
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << "Send message thread terminated\n";
}

void receive_message(int client_socket){
    char buffer[BUFFER_SIZE]; //local buffer
    while(client_active.load()) {
        std::memset(buffer, 0, BUFFER_SIZE);

        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes_received < 0){
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                perror("Failed to receive data from the server\n");
                client_active.store(false);
            }
            break;
        }
        else if(bytes_received == 0){
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                client_active.store(false);
                perror("Server disconnected\n");
            }
            break;
        }
        else{
            buffer[BUFFER_SIZE] = '\0';
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "Server: " << buffer << '\n';
                // std::flush; ?
            }
        }
    }
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << "Receive message thread terminated\n";
    std::cout << "Server has shutdown.\n";
}

int main(){
    int client_socket;      //this is the socket aka gateway - basically the "door"
    struct sockaddr_in server_addr; // Structure to hold server's address information ; comes from <netinet/in.h>
    // char buffer[BUFFER_SIZE] = {0};     //initialize entire array with 0

    //Create a socket

    //0 means in protocol means TCP
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
        perror("Connection Failed\n");
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

    if(client_socket != -1){
        close(client_socket);
        client_socket = -1;     //closes client_socket
        std::cout << "Client socket closed\n";
    }

    //wait for receive thread to finish cleanup
    receive_thread.join();
    std::cout << "Exiting client application\n";
    return 0;  
}



