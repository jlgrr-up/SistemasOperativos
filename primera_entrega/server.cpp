#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <thread>

#include "MecanismosIPC.h"


int main() {
    //cargar datos 

    std::ifstream itch (
        "datos/20190530.BX_ITCH_50",
        std::ios::binary
    );

    if (!itch) {
        std::cerr << "Error al abrir el archivo ITCH.\n";
        return EXIT_FAILURE;
    }

    // Socket okay
    int sockfd;
    struct sockaddr_in server_addr;
    int port = 8080;

    // 1. Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket failed");
        return EXIT_FAILURE;
    }

    // 2. Configure destination address
    server_addr.sin_family = AF_INET;
    
    server_addr.sin_port = htons(port);

    inet_pton(AF_INET,
              "127.0.0.1",
              &server_addr.sin_addr);

    std::cout << "Sending ITCH-style packets...\n";
    
    while (true) {
        
        ui length;
        itch.read(reinterpret_cast<char*>(&length), 2);

        if (itch.eof()) break;

        length = ntohs(length);// ITCH messages are prefixed with a 2-byte length field      
        char buffer[2048];

        if (length > sizeof(buffer) || length == 0) {
            std::cerr << "1. Error al leer el mensaje ITCH..\n";
            break;
        }

        itch.read(buffer, length);

        if (itch.gcount() != length) {
            std::cerr << "2. Error al leer el mensaje ITCH.\n";
            break;
        }

        sendto(sockfd,
               buffer,
               length,
               0,
               (struct sockaddr*)&server_addr,
               sizeof(server_addr));
    
        std::this_thread::sleep_for(
        std::chrono::microseconds(1));
    }

    close(sockfd);

    return 0;
}