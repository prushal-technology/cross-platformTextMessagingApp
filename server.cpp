#include <iostream>
#include <netinet/in.h>
// #include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 3000       //a free port for local hosting
#define BUFFER_SIZE 1024

int main(){
    int server_socket;          //gonna be our server gateway
    struct sockaddr_in client_addr;
    {
        //how sockaddr_in is defined
        // sin_port = port number 
        // sin_addr = internet adress 
        // sin_zero = size of sockadd
    }
    char buffer[BUFFER_SIZE] = {0};

    //create a socket
    //the 0 basically tells system to choose default (TCP)
    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error: Server socket not created\n");
        return EXIT_FAILURE;
    }

    //Initialize socket structure
    client_addr.sin_family = AF_INET; 
    client_addr.sin_port = htons(PORT);//gotta be different from client port

    //we don't need to look for client, client comes to us
    
    std::cout << "Starting server...";
    std::string message;

    while(true){
        
    }

}