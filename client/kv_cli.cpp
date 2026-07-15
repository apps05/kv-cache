#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>

void sendCommand(int sock, const std::string& cmd) {
    std::string wireCmd = cmd + "\r\n";
    write(sock, wireCmd.c_str(), wireCmd.size());

    char buffer[4096];
    std::string response;
    while (true) {
        ssize_t bytesRead = read(sock, buffer, sizeof(buffer));
        if (bytesRead <= 0) {
            std::cerr << "Connection closed by server." << std::endl;
            break;
        }
        for (ssize_t i = 0; i < bytesRead; ++i) {
            response += buffer[i];
            if (buffer[i] == '\n') {
                std::cout << response;
                return;
            }
        }
    }
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 6380;
    std::string command;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--target=") == 0) {
            std::string target = arg.substr(9);
            size_t colon = target.find(':');
            if (colon != std::string::npos) {
                host = target.substr(0, colon);
                if (host == "localhost") host = "127.0.0.1";
                port = std::stoi(target.substr(colon + 1));
            }
        } else {
            if (!command.empty()) command += " ";
            command += arg;
        }
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        return 1;
    }

    if (!command.empty()) {
        sendCommand(sock, command);
    } else {
        std::cout << "kv_cli interactive mode. Type commands (e.g. GET key, SET key val, STATS). Ctrl+C to exit.\n";
        while (true) {
            std::cout << "> ";
            std::string line;
            if (!std::getline(std::cin, line)) break;
            if (line.empty()) continue;
            sendCommand(sock, line);
        }
    }

    close(sock);
    return 0;
}
