#include <iostream>
#include <fstream> 
#include <cstring> //memcpy
#include <sys/socket.h> //socket, bind, sendto
#include <netinet/in.h> //sockaddr_in, htons, ntohs
#include <arpa/inet.h> //inet_pton
#include <unistd.h>//close
#include <chrono>
#include <thread>

#include "MecanismosIPC.h"


int main() {
    //cargar datos 

    std::ifstream itch ("../datos/20190530.BX_ITCH_50", std::ios::binary);

    if (!itch) {
        std::cerr << "Error al abrir el archivo ITCH.\n";
        return EXIT_FAILURE;
    }

    // Socket okay
    int sockfd;
    struct sockaddr_in server_addr; //estructura que contiene la dirección del servidor
    int port = 8080;

    // 1. UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { //AF_INET para IPv4, SOCK_DGRAM para UDP, 0 para el protocolo por defecto (UDP en este caso)
        perror("Socket failed");
        return EXIT_FAILURE;
    }

    // destino
    server_addr.sin_family = AF_INET; //IPv4, los datos que vamos a enviar se mandarán a una dirección IPv4
    
    server_addr.sin_port = htons(port); //htons convierte el número de puerto al formato de red (big-endian), que es necesario para la comunicación de red

    //inet_pton convierte la dirección IP de texto a binario, y la almacena en server_addr.sin_addr. 
    //En este caso, estamos usando la dirección de loopback para enviar los datos al mismo host
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    std::cout << "Sending ITCH-style packets...\n";
    
    while (true) {
        
        ui length;
        //reinterpret_cast convierte el puntero a char* para poder leer los bytes del archivo, y luego leemos 2 bytes para obtener la longitud del mensaje ITCH
        itch.read(reinterpret_cast<char*>(&length), 2);

        if (itch.eof()) break;

        length = ntohs(length); //ntohs convierte la longitud del mensaje ITCH del formato de red (big-endian) al formato del host, para que podamos usarlo correctamente en nuestro programa      
        char buffer[2048];

        if (length > sizeof(buffer) || length == 0) {
            std::cerr << "1. Error al leer el mensaje ITCH..\n";
            break;
        }

        itch.read(buffer, length);//leemos el mensaje ITCH completo, que tiene una longitud de 'length' bytes, y lo almacenamos en 'buffer'

        if (itch.gcount() != length) {
            std::cerr << "2. Error al leer el mensaje ITCH.\n";
            break;
        }

        sendto(sockfd, //enviamos el mensaje ITCH a través del socket UDP al servidor, usando la función sendto.
               buffer, //el mensaje ITCH que queremos enviar, que está almacenado en 'buffer'
               length, //la longitud del mensaje ITCH que queremos enviar, que es 'length' bytes
               0, //flags, que en este caso es 0 porque no estamos usando ninguna bandera especial para el envío
               (struct sockaddr*)&server_addr, //la dirección del servidor a la que queremos enviar el mensaje ITCH, que está almacenada en 'server_addr'.
               sizeof(server_addr));  //el tamaño de la estructura de dirección del servidor, que es necesario para la función sendto
    
        std::this_thread::sleep_for(
        std::chrono::microseconds(1));
    }

    close(sockfd);

    return 0;
}