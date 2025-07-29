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
std::atomic<bool> server_active(true);

void handle_client(int client_socket, struct sockaddr_in client_addr){
    char buffer[BUFFER_SIZE] = {0};
    std::cout << "Client connected from: " << inet_ntoa(client_addr.sin_addr) 
        << " " << client_addr.sin_port << '\n';

    while(server_active.load()){
        std::memset(buffer, 0, BUFFER_SIZE);

        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes_received < 0){
            perror("Failed to receive message from client\n");
            break;
        } 
        else if(bytes_received == 0){
            std::cout << "Client has disconnected\n";
            break;
        }
        else{
            buffer[bytes_received] = '\0';
            std::cout << "Client: " << buffer << '\n';
            
            if(std::string(buffer) == "exit"){
                std::cout << "Client " << inet_ntoa(client_addr.sin_addr) << " " << ntohl(client_addr.sin_port) << " has disconnected\n";
                break;
            }
        }
    }

    close(client_socket);
    std::cout << "Client " << inet_ntoa(client_addr.sin_addr) << " " << ntohl(client_addr.sin_port) << " thread has been terminated\n";
}

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

    std::vector<std::thread> client_threads;
    
    // https://en.cppreference.com/w/cpp/thread/thread.html
    std::thread console_input_thread([](){
        std::string input;
        while(server_active.load()){
            std::getline(std::cin, input);
            if(input == "disconnect"){
                std::cout << "Server will shutdown\n";
                server_active.store(false);        //tells all threads to terminate
                break;
            }
        }
    });


    
    std::cout << "Listening on Port " << PORT << '\n';

    
    while(server_active.load()){
        int incoming_socket = accept(server_fd, (struct sockaddr*)& client_addr, &addr_len);
        if(incoming_socket < 0){
            perror("Client connection error\n");
            break;
        }
        //create new thread
        client_threads.emplace_back(handle_client, incoming_socket, client_addr);

        //detach safely return all the thread's resources to the operating system
        client_threads.back().detach();
    }

    // check cppreference documentation
    //joinable = running, so if it is, we use join() to wait till it finishes 
    //running and then terminate it
    if(console_input_thread.joinable()){
        console_input_thread.join();
    }


    close(incoming_socket);
    close(server_fd);
    std::cout << "End of all communications\n";
    return 0;
}