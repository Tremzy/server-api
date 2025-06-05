#include <iostream>
#include <string>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int PORT = 8080;

std::string get_utc_time_json() {
    std::time_t now = std::time(nullptr);
    std::tm *gmt = std::gmtime(&now); 

    char buff[64];
    std::strftime(buff, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmt);

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
    int server_fd, new_socket;
    sockaddr_in address{};
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
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