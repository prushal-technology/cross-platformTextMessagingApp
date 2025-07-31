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

#define SERVER_IP "127.0.0.1"
#define PORT 8080       //a free port for local hosting
#define BUFFER_SIZE 1024
#define TOTAL_CONNECTIONS 5

std::mutex console_mutex;
std::atomic<bool> server_active(true);    //this for the entire server

void handle_client(int client_socket, struct sockaddr_in client_addr){
    char buffer[BUFFER_SIZE] = {0};
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "Client connected from: " << inet_ntoa(client_addr.sin_addr)
            << " " << client_addr.sin_port << '\n';
    }

    while(server_active.load()){
        std::memset(buffer, 0, BUFFER_SIZE);

        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes_received < 0){
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                perror("Failed to receive message from client\n");
            }
            break;
        }
        else if(bytes_received == 0){
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "Client has disconnected\n";
            }
            break;
        }
        else{
            buffer[bytes_received] = '\0';
            {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "Client: " << buffer << '\n';
            }
            if(std::string(buffer) == "exit"){
                {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "Client " << inet_ntoa(client_addr.sin_addr) 
                        << " " << ntohl(client_addr.sin_port) << " has disconnected\n";
                }
                break;
            }
        }
    }
    
    close(client_socket);
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << "Client " << inet_ntoa(client_addr.sin_addr) << " " 
        << ntohl(client_addr.sin_port) << " thread has been terminated\n";
}

using ownmux = std::lock_guard<std::mutex>;

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

    std::vector<std::thread> client_threads;

    //lambda function
    std::thread console_input_thread([&server_fd](){
        std::string input;
        while(server_active.load()){
            std::getline(std::cin, input);
            if(input == "disconnect"){
                {
                    ownmux lock(console_mutex);
                    std::cout << "Server will shutdown\n";
                }
                server_active.store(false);

                int dummy_sock = -1;
                try{
                    dummy_sock = socket(AF_INET, SOCK_STREAM, 0);
                    if(dummy_sock < 0){
                        ownmux lock(console_mutex);
                        perror("Error creating dummy socket for unblocking accept()\n");
                    }
                    else{
                        struct sockaddr_in serv_addr;
                        serv_addr.sin_family = AF_INET;
                        serv_addr.sin_port = htons(PORT);
                        inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

                        //connection needs to reach accept queue - that's all
                        connect(dummy_sock, (struct sockaddr*)& serv_addr, sizeof(serv_addr));

                        //don't check if worked or nah - just close
                        close(dummy_sock);
                        dummy_sock = -1;
                        {
                            ownmux lock(console_mutex);
                            std::cout << "Dummy connection made to unblock aceept().\n";
                        }
                    }
                }catch(const std::exception& e){
                    ownmux lock(console_mutex);
                    //all what does is return an explanation string
                    std::cerr << "Exception during dummy connection: " << e.what() << '\n';
                }



                if (server_fd != -1) {
                    close(server_fd);
                    // No need to set server_fd = -1 here as main is about to exit
                    {
                        std::lock_guard<std::mutex> lock(console_mutex);
                        std::cout << "Listening socket (server_fd) closed by console thread.\n";
                    }
                }
                break; // Exit console input loop
            }
        }
        ownmux lock(console_mutex);
        std::cout << "Console input thread has been terminated\n";
    });

    {
        ownmux lock(console_mutex);
        std::cout << "Listening on Port " << PORT << '\n';
    }

    while(server_active.load()){
        int incoming_socket = accept(server_fd, (struct sockaddr*)& client_addr, &addr_len);
        if(incoming_socket < 0){
            if(!server_active.load()){
                ownmux lock(console_mutex);
                std::cout << "Accept operation interrupted due to server shutdown. Exiting accepting loop\n";

            }
            else{
                ownmux lock(console_mutex);
                perror("Critical Error: Client connection accpetance failure\n");
            }
            break;
        }

        // If a connection was successfully accepted before shutdown, handle it
        // Check server_active one more time after accept in case it became false *just* after accept returned.
        if (!server_active.load()) {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "Accepted connection during shutdown phase, closing immediately.\n";
            close(incoming_socket); // Close the newly accepted socket
            break; // Exit the loop
        }

        client_threads.emplace_back(handle_client, incoming_socket, client_addr);

        //this simply takes the last pushed thread, 
        // then detaches it from std::thread (the class the created the object)
        client_threads.back().detach();
    }

    if(console_input_thread.joinable()){
        console_input_thread.join();
    }

    //our incoming socket function TAKES CARE OF THIS!
    // close(incoming_socket);
    if(server_fd != -1){
        close(server_fd);
        ownmux lock(console_mutex);
        std::cout << "Listening (serrver) socket has been closed\n";
    }

    ownmux lock(console_mutex);
    std::cout << "Closing application\n";
    return 0;
}