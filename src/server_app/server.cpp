#include <iostream>
#include <netinet/in.h>
#include <unistd.h>         //for close()
#include <arpa/inet.h>
#include <cstring>

//for multithreading
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

//for message format and timestamp
#include <iomanip>
#include <chrono>
#include <ctime>

//redesign how client communicates with server, and add server broadcast feature
#include <map>

#define SERVER_IP "127.0.1.1"
#define PORT 8080       //a free port for local hosting
#define BUFFER_SIZE 1024
#define TOTAL_CONNECTIONS 10        //this can be whatever

std::map<int, std::string> clients; //so key=clientsocket and value=username
std::mutex clients_mutex;

std::mutex console_mutex;
std::atomic<bool> server_active(true);    //this for the entire server

using ownmux = std::lock_guard<std::mutex>;

void broadcast_message(const std::string& message, int sender_socket);  //forward decl

void handle_client(int client_socket, struct sockaddr_in client_addr){
    char buffer[BUFFER_SIZE] = {0};
    std::string username;       //for username
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "Client connected from: " << inet_ntoa(client_addr.sin_addr)
            << " " << client_addr.sin_port << '\n';
    }

    //for getting username
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        username = buffer;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients[client_socket] = username;
        }

        std::string join_msg = "[SERVER] " + username + " has joined.";
        {
            ownmux lock(console_mutex);
            std::cout << join_msg << '\n';
        }
        broadcast_message(join_msg, client_socket);
    }
    else {
        // Handle a failed initial username transmission
        {
            ownmux lock(console_mutex);
            perror("Failed to receive username from client");
        }
        close(client_socket);
        return;
    }

    while (server_active.load()) {
        std::memset(buffer, 0, BUFFER_SIZE);
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0) {
            // Error or client disconnected
            break;
        }

        buffer[bytes_received] = '\0';
        std::string received_msg(buffer);

        if (received_msg == "exit") {
            break;
        }
        
        if (received_msg.rfind("/upload_complete ", 0) == 0) {
            // The server's role is just to broadcast this message to all other clients.
            broadcast_message(received_msg, client_socket);
            
            {
                ownmux lock(console_mutex);
                std::cout << "[INFO] " << username << " uploaded a file." << std::endl;
            }
        }
        else{
            //message broadcasting logic
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::tm* local_tm = std::localtime(&now_c);
            
            std::stringstream ss;
            ss << "[" << std::put_time(local_tm, "%H:%M") << "] " << username << ": " << received_msg;
            std::string formatted_message = ss.str();
            
            // Log to server console
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << formatted_message << std::endl;
            }
            
            // Broadcast to other clients
            broadcast_message(formatted_message, client_socket);
        }
    }
    
    std::string leave_msg = "[SERVER] " + username + " has left the chat.";
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << leave_msg << std::endl;
    }

    //Remove client from the map
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(client_socket);
    }

    // Announce departure to remaining clients
    broadcast_message(leave_msg, -1); // -1 indicates a server message, not from a specific client

    close(client_socket);
}


void broadcast_message(const std::string& message, int sender_socket){
    ownmux lock(clients_mutex);
    for(const auto& pair: clients){
        int client_sock = pair.first;
        if(client_sock != sender_socket){
            //why above - we don't wanna send message back to sender duh
            send(client_sock, message.c_str(), message.length(), 0);
        }
    }
}

int main(){
    int server_fd;
    int incoming_socket;
    struct sockaddr_in client_addr, server_addr;

    char buffer[BUFFER_SIZE] = {0};
    socklen_t addr_len = sizeof(client_addr);

    //Create socket

    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error: Server socket not created\n");
        return EXIT_FAILURE;
    }

    //Initialize Server address structure

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if(bind(server_fd, (struct sockaddr*)& server_addr, sizeof(server_addr))< 0){
        perror("Binding failure\n");
        close(server_fd);
    }

    if(listen(server_fd, TOTAL_CONNECTIONS)< 0){
        perror("Error: Listening failed\n");
        close(server_fd);
        return EXIT_FAILURE;
    }

    // std::vector<std::thread> client_threads;

    //lambda function
    std::thread console_input_thread([&server_fd](){
        std::string input;
        while(server_active.load()){
            std::getline(std::cin, input);
            if(input == "disconnect"){
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "Server shutdown initiated..." << std::endl;
                server_active.store(false);
                
                // This is a common technique to unblock a blocking call like accept()
                // It connects to the server itself to generate an event.
                int dummy_sock = socket(AF_INET, SOCK_STREAM, 0);
                struct sockaddr_in serv_addr;
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(PORT);
                inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
                connect(dummy_sock, (struct sockaddr*)& serv_addr, sizeof(serv_addr));
                close(dummy_sock);
                
                // Closing the server's listening socket will also cause accept() to fail.
                shutdown(server_fd, SHUT_RDWR);
                close(server_fd);
                break;
            }
        }
    });

    {
        ownmux lock(console_mutex);
        std::cout << "Listening on Port " << PORT << '\n';
    }

    while (server_active.load()) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int incoming_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

        if (incoming_socket < 0) {
            if (!server_active.load()) {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "Server shutting down. No longer accepting new clients." << std::endl;
            } else {
                perror("Client connection acceptance failure");
            }
            break;
        }

        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "New connection from " << inet_ntoa(client_addr.sin_addr)
                      << ":" << ntohs(client_addr.sin_port) << std::endl;
        }

        // Create a new thread for the connected client.
        std::thread client_thread(handle_client, incoming_socket, client_addr);
        client_thread.detach(); // Detach to allow it to run independently.
    }

    if (console_input_thread.joinable()) {
        console_input_thread.join();
    }

    std::cout << "Server application has closed." << std::endl;
    return 0;
}