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

#include <cstdlib>          //for automatic detection of OS

#include <map>          //map for storing our catbox file names
#include <sys/types.h>      //prerequisit for pwd.h
#include <pwd.h>            //this one is for getting password structure in /etc/password

#define SERVER_IP "127.0.0.1"       //local host
#define PORT 8081                   //8080 for local testing, 8081 for cloudflare test
#define BUFFER_SIZE 1024           //message buffer size

std::mutex console_mutex;
std::atomic<bool> client_active(true);
std::map<std::string, std::string> uploadedFiles; // Global map for files

using ownmux = std::lock_guard<std::mutex>;

std::string uploadFileToCatbox(const std::string& filePath);
std::string getUserHomeDirectory();
void downloadFile(const std::string& fileName, const std::string& fileUrl);
void play_sound(const std::string& filepath);       //decleration

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

    while(client_active.load()){
        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "You: ";
        }
        //moved "You: " uptop as the receive thread handles it
        //and we simply need to print it once
        std::getline(std::cin, message);

        if(message == "exit"){      //user tries to exit application
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
        else if(message.rfind("/upload ", 0) == 0){         //user tries to upload file
            size_t firstSpace = message.find(' ');
            size_t secondSpace = message.find(' ', firstSpace + 1);

            if (firstSpace != std::string::npos && secondSpace != std::string::npos) {
                std::string filePath = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                std::string fileName = message.substr(secondSpace + 1);
                
                std::string fileUrl = uploadFileToCatbox(filePath);
                if (!fileUrl.empty()) {
                    std::string serverMessage = "/upload_complete " + fileName + " " + fileUrl;
                    send(client_socket, serverMessage.c_str(), serverMessage.length(), 0);
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "File '" << fileName << "' uploaded successfully. URL sent to server.\n";
                } else {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "File upload failed.\n";
                }
            } else {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "Invalid '/upload' command. Use: /upload <filepath> <filename>\n";
            }
        }
        else if (message.rfind("/download ", 0) == 0) {     //user tries to download file
            std::string fileName = message.substr(message.find(' ') + 1);
            
            if (uploadedFiles.count(fileName)) {
                downloadFile(fileName, uploadedFiles[fileName]);
            } else {
                std::lock_guard<std::mutex> lock(console_mutex);
                std::cout << "File '" << fileName << "' not found in the list of available files.\n";
            }
        }
        else{
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
    }
    std::lock_guard<std::mutex> lock(console_mutex);
    std::cout << "Send message thread terminated\n";
}

void receive_message(int client_socket){
    char buffer[BUFFER_SIZE];
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
            std::string receivedMessage(buffer);
            if (receivedMessage.rfind("/upload_complete ", 0) == 0) {
                size_t firstSpace = receivedMessage.find(' ');
                size_t secondSpace = receivedMessage.find(' ', firstSpace + 1);
                std::string fileName = receivedMessage.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                std::string fileUrl = receivedMessage.substr(secondSpace + 1);
                
                uploadedFiles[fileName] = fileUrl;
                play_sound("click.wav");
                {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "\n[INFO] New file available: " << fileName << '\n';
                    std::cout << "-> " << std::flush;
                }
            } else {
                play_sound("click.wav");
                {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "\n" << buffer << '\n';
                    std::cout << "-> " << std::flush;
                }
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
    std::cout << "Use '/upload <filepath> <filename>' to upload files.\n";
    std::cout << "Use '/download <filename>' do download files.\n";

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

std::string uploadFileToCatbox(const std::string& filePath) {
    //remember - CURL USES POST (not a big deal but something to remember)
    std::string command = "curl -s -F \"fileToUpload=@" + filePath + "\" -F \"reqtype=fileupload\" https://catbox.moe/user/api.php";
    
    //basically runs a new process - a "pipe" here
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "";
    }
    
    char buffer[1024];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    
    pclose(pipe);       
    
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    
    return result;
}

std::string getUserHomeDirectory() {
    #ifdef _WIN32
        const char* homeDir = std::getenv("USERPROFILE");
        return (homeDir != nullptr) ? std::string(homeDir) : "";
    #else
        //getuid - makes sense
        struct passwd *pw = getpwuid(getuid());     //part of the pwd.h library
        //pwd is for passwords. Passwords cauze path to homedirectory is in /etc/passwords
        return (pw != nullptr) ? std::string(pw->pw_dir) : "";
    #endif
}

void downloadFile(const std::string& fileName, const std::string& fileUrl) {
    std::string downloadsPath = getUserHomeDirectory() + "/Downloads/";
    
    std::string createDirCommand = "mkdir -p " + downloadsPath;
    system(createDirCommand.c_str());
    
    std::string command = "curl -o \"" + downloadsPath + fileName + "\" \"" + fileUrl + "\"";
    
    if (system(command.c_str()) == 0) {     //check return value. similar to "return 0" we use. 0 = success
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "File '" << fileName << "' downloaded to '" << downloadsPath << "' successfully.\n";
    } else {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "File download failed.\n";
    }
}

void play_sound(const std::string& filepath){
    {
        //requires aplay to be installed on linux
    }
    #ifdef _WIN32
        // Windows
        std::string command = "start " + filepath;
        system(command.c_str());
    #elif __APPLE__
        // macOS
        std::string command = "afplay " + filepath + " &";
        system(command.c_str());
    #elif __linux__
        // Linux using aplay for .wav files
        std::string command = "aplay -q " + filepath + " &";
        system(command.c_str());
    #endif
}



