#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <csignal>
#include <fstream>

const int PORT = 8080;
const int MAX_CLIENTS = 255;
static int server_fd;
const int MINSECPERTENCONS = 3;
const int MAX_THREADS = 512;
const int MAX_INFRACTIONS = 25;
const int BAN_TIME_SEC = 180;
const int INFTIMEOUTSEC = 360;
std::thread threadList[MAX_THREADS];

struct ClientInfo {
    std::string ip_addr;
    int port;
    size_t lastConnected[25];
    int connections;
    int infractions;
    time_t bannedUntil;
    time_t firstInf;
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

void shift_cons(ClientInfo &client) {
    if (client.connections <= 0) return;
    for (int i = 0; i < client.connections - 1; i++) {
        client.lastConnected[i] = client.lastConnected[i + 1];
    }
    client.lastConnected[client.connections - 1] = 0;
    client.connections--;
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
                    if (clientList[i].connections > 0) {
                        const std::time_t lstCon = static_cast<time_t>(
                            clientList[i].lastConnected[clientList[i].connections - 1]
                        );
                        std::tm* gmt = std::gmtime(&lstCon);
                        char buffer[64];
                        std::strftime(buffer, sizeof(buffer), "%Y. %m. %d., %H:%M:%S", gmt);
                        std::cout << "- Client [" << i+1 << "]:"
                                << "\n\tIP: " << clientList[i].ip_addr
                                << "\n\tClient port: " << clientList[i].port
                                << "\n\tLast seen: " << buffer
                                << "\n\tTotal connections: " << clientList[i].connections
                                << "\n\tInfractions: " << clientList[i].infractions
                                << "\n\tInfraction time out in: " << clientList[i].firstInf + INFTIMEOUTSEC - std::time(nullptr)
                                << "\n";
                    } else {
                        std::cout << "- Client [" << i+1 << "] has no connections logged.\n";
                    }
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

std::string http_response_json(const std::string &body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-type: application/json\r\n";
    oss << "Content-length: " << body.length() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

std::string http_response_html(const std::string &body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-type: text/html\r\n";
    oss << "Content-length: " << body.length() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

std::string read_html(const std::string &path) {
    std::ifstream file(path);
    if (!file) return "";
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

void handle_request(int new_socket, sockaddr_in address) {
    char ip_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(address.sin_addr), ip_addr, INET_ADDRSTRLEN);
    int client_port = ntohs(address.sin_port);
    
    ClientInfo* current_client = search_client(std::string(ip_addr));
    if (current_client != nullptr) {
        if (current_client->connections >= 24) {
            shift_cons(*current_client);
        }

        if (current_client->bannedUntil > std::time(nullptr)) {
            close(new_socket);
            return;
        }

        current_client->connections += 1;
        current_client->lastConnected[current_client->connections-1] = std::time(nullptr);
        if (current_client->connections >= 10) {
            if (current_client->lastConnected[current_client->connections-1] - current_client->lastConnected[current_client->connections-10] < MINSECPERTENCONS) {
                std::cout << "[" << current_client->ip_addr << "] " << "violated connection throttling\nDeferring connection...\n";

                if (current_client->infractions >= 1 && current_client->firstInf + INFTIMEOUTSEC < std::time(nullptr)) {
                    current_client->infractions = 0;
                }

                if (current_client->infractions < 1) {
                    current_client->firstInf = std::time(nullptr);
                }

                current_client->infractions++;
                if (current_client->infractions > MAX_INFRACTIONS) {
                    std::cout << "[" << current_client->ip_addr << "] " << " has been banned for commiting too many violations\n";
                    current_client->bannedUntil = std::time(nullptr)+BAN_TIME_SEC;
                    current_client->infractions = 0;
                }

                close(new_socket);
                return;
            }
        }
    }
    else {
        ClientInfo new_client;
        new_client.ip_addr = ip_addr;
        new_client.port = client_port;
        new_client.connections = 1;
        new_client.lastConnected[0] = std::time(nullptr);
        new_client.infractions = 0;
        new_client.bannedUntil = 0;
        new_client.firstInf = 0;
        clientList[clLen] = new_client;
        clLen++;
    }
    
    char buffer[3000] = {0};
    read(new_socket, buffer, sizeof(buffer));
    std::string request(buffer);
    
    if (request.length() < 1) {
        return;
    }
    std::cout << "Received request:\n" << request << "\n";
    
    if (request.starts_with("GET / HTTP/1.1")) {
        std::string body = read_html("src/htdocs/index.html");
        std::string response = http_response_html(body);
        send(new_socket, response.c_str(), response.length(), 0);
    }
    else if (request.starts_with("GET /api/utc")) {
        std::string body = get_utc_time_json();
        std::string response = http_response_json(body);
        send(new_socket, response.c_str(), response.length(), 0);
    }
    else {
        const std::string not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(new_socket, not_found.c_str(), not_found.length(), 0);
    }

    close(new_socket);
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

        bool assigned = false;
        for (int i = 0; i < MAX_THREADS; i++) {
            if (threadList[i].joinable()) {
                threadList[i].join();
            }
            if (!threadList[i].joinable()) {
                threadList[i] = std::thread(handle_request, new_socket, address);
                assigned = true;
                std::cout << "Offloading to index [" << i << "] in the thread pool\n"; 
                break;
            }
        }
        if (!assigned) {
            close(new_socket);
            std::cout << "Thread pool is full, cannot handle new request\n";
        }
         
        std::thread passRequest(handle_request, new_socket, address);
        passRequest.detach();
    }
    return 0;
}