#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <csignal>

const int PORT = 8080;
const int MAX_CLIENTS = 255;
static int server_fd;

struct ClientInfo {
    std::string ip_addr;
    int port;
    size_t lastConnected;
    int connections;
};

static ClientInfo* clientList = new ClientInfo[MAX_CLIENTS];
size_t clLen = 0;

ClientInfo* search_client(const std::string &client_ip) {
    for (int i = 0; i < clLen; i++) {
        if (clientList[i].ip_addr == client_ip) {
            return &clientList[i];
        }
    }
    return nullptr;
}

void input_thread_func() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.find("quit") != std::string::npos || line.find("exit") != std::string::npos || line.find("stop") != std::string::npos) {
            std::cout << "Exiting...\n";
            close(server_fd);
            exit(1);
        }
        else if(line.find("clients") != std::string::npos) {
            if (clLen > 0) {
                std::cout << "Client list:\n";
                for(int i = 0; i < clLen; i++) {
                    std::cout << "- Client [" << i+1 << "]:\n\tIP:" << clientList[i].ip_addr <<"\n\tClient port: " << clientList[i].port << "\n\tLast seen: " << clientList[i].lastConnected << "\n\tTotal connections: " << clientList[i].connections << "\n";
                }
            }
            else {
                std::cout << "Client list is empty.\n";
            }
        }
    }
}

void sighandler(int sigid) {
    if(sigid == SIGINT) {
        std::cout << "Exiting...\n";
        close(server_fd);
        exit(1);
    }
}

std::string get_utc_time_json() {
    std::time_t now = std::time(nullptr);
    std::tm *gmt = std::gmtime(&now); 

    char buff[64];
    std::strftime(buff, sizeof(buff), "%Y-%m-%dT%H:%M:%SZ", gmt);

    std::ostringstream oss;
    oss << "{ \"utc:\": \"" << buff << "\" }";
    return oss.str();
}

std::string http_response(const std::string &body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-type: application/json\r\n";
    oss << "Content-length: " << body.length() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

int main() {
    std::signal(SIGINT, sighandler);
    std::thread input_thread(input_thread_func);
    int new_socket;
    sockaddr_in address{};
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    if (listen(server_fd, 3) < 0) {
        return 1;
    }

    std::cout << "Listening on port: " << PORT << "...\n";

    while (true) {
        new_socket = accept(server_fd, (sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }
        
        
        char buffer[3000] = {0};
        read(new_socket, buffer, sizeof(buffer));
        std::string request(buffer);
        
        if (request.length() < 1) {
            continue;
        }

        char ip_addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), ip_addr, INET_ADDRSTRLEN);
        int client_port = ntohs(address.sin_port);

        ClientInfo* current_client = search_client(std::string(ip_addr));
        if (current_client != nullptr) {
            current_client->lastConnected = std::time(nullptr);
            current_client->connections += 1;
        }
        else {
            ClientInfo new_client;
            new_client.ip_addr = ip_addr;
            new_client.port = client_port;
            new_client.connections = 1;
            new_client.lastConnected = std::time(nullptr);
            clientList[clLen] = new_client;
            clLen++;
        }
        
        std::cout << "Received request:\n" << request << "\n";

        if (request.starts_with("GET /api/utc")) {
            std::string body = get_utc_time_json();
            std::string response = http_response(body);
            send(new_socket, response.c_str(), response.length(), 0);
        }
        else {
            const std::string not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            send(new_socket, not_found.c_str(), not_found.length(), 0);
        }
        close(new_socket);
    }
    return 0;
}