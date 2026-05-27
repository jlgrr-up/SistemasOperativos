#include <iostream>
#include <cstring>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "MecanismosIPC.h"

#include <fstream>

struct PacketRaw {
    char data[128];//mssgtype data[0] + payload data[1-127]
    ssize_t len;
    ui recv_ts;
};


struct SharedMemoryBuffer {
    PacketRaw packets[num_buffer];
    ui in;
    ui out;
    bool done; //para que el consumidor sepa cuando el productor terminó de producir
};

//por ahora, sólo contabilizamos estos eventos 
//documentación: https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf

struct SystemEventMessage { //mensaje tipo S (System Event Message)
    uint16_t stockLocate; //2 bytes. NASDAQ usa esto como id del simbolo
    uint16_t trackingNumber;
    ui receiveTimestamp;
    ui exchangeTimestamp;
    char eventCode;
    char messageType;
};


struct AddOrderMessageA { //mensaje tipo A (Add Order Message)
    uint16_t stockLocate;
    uint16_t trackingNumber;
    ui receiveTimestamp;
    ui exchangeTimestamp;
    ui orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[9];
    uint32_t price;
    char messageType;

    AddOrderMessageA(ui timestamp) {
        this->receiveTimestamp = timestamp;
        memset(stock, 0, sizeof(stock));
    }
};

struct AddOrderMessageF { //mensaje tipo A (Add Order Message)
    uint16_t stockLocate;
    uint16_t trackingNumber;
    ui receiveTimestamp;
    ui exchangeTimestamp;
    ui orderReferenceNumber;
    char buySellIndicator;
    uint32_t shares;
    char stock[9];
    uint32_t price;
    char attribution[5];
    char messageType;

    AddOrderMessageF(ui timestamp) {
        this->receiveTimestamp = timestamp;
        memset(stock, 0, sizeof(stock));
        memset(attribution, 0, sizeof(attribution));
    }
};

uint16_t parse_u16(const char* ptr) {
    uint16_t val;
    memcpy(&val, ptr, sizeof(val));
    return ntohs(val);
}
uint64_t parse_u48(const char* ptr) {
    uint64_t val = 0;
    for (int i = 0; i < 6; i++) {
        val = (val << 8) | (uint64_t)(unsigned char)ptr[i];
    }

    return val;
}

int main() {

    //vamos a hacer una shared memory, 
    //proceso padre que simula el receptor de ITCH, y un proceso hijo que simula el parser del ITCH

    int server_fd;//file descriptor del socket
    struct sockaddr_in address; //estructura que contiene la dirección del socket
    int addrlen = sizeof(address);//longitud de la estructura de dirección
    int port = 8080;//puerto en el que el servidor escuchará

    // Crear UDP socket
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {

        perror("Socket failed");

        return EXIT_FAILURE;
    }

    // 2. Configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // 3. Bind socket
    if (
        bind(
            server_fd,
            (struct sockaddr *)&address,
            sizeof(address)
        ) < 0
    ) {

        perror("Bind failed");

        return EXIT_FAILURE;
    }

    std::cout
        << "ITCH Receiver listening on port "
        << port
        << "...\n";


    //cosas de la shared memory:
    const char* shm_name = "/hft_ITCH";//create and readwrite
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedMemoryBuffer));

    SharedMemoryBuffer* shm = (SharedMemoryBuffer*)mmap(
        0, //direccion de memoria, el sistema decide
        sizeof(SharedMemoryBuffer), //tamaño del mapeo
        PROT_READ | PROT_WRITE, //protecciones de lectura y escritura
        MAP_SHARED, //compartido entre procesos
        shm_fd,//file descriptor de la memoria compartida
        0); //offset, no nos interesa
    shm->in = 0;
    shm->out = 0;
    shm->done = false;
    int pid = fork();
    if (pid < 0) {
        std::cerr << "no se pudo creo el proceso hijo" << std::endl;
        return EXIT_FAILURE;
    }

    if (pid == 0) {//proceso hijo, parser del ITCH
      int counter = 0;
        
      while (true){  
            
            if (shm->in == shm->out) { // buffer vacío, no hay paquetes para consumir
                if (shm->done) {shm->done = true; break; } //si el productor terminó de producir y el buffer está vacío, terminamos de consumir
                continue;
            };

            PacketRaw p = shm->packets[shm->out]; // consumir el paquete
            shm->out = (shm->out + 1) % num_buffer; // circular buffer, avanzamos el índice de salida

            switch (p.data[0]) { //el primer byte del paquete es el message type
                case 'S': {
                    std::ofstream myFile("testparse.txt", std::ios::app);
                    SystemEventMessage msg;
                    msg.eventCode = p.data[11];
                    msg.stockLocate = parse_u16(p.data + 1);
                    msg.trackingNumber = parse_u16(p.data + 3);
                    msg.receiveTimestamp = p.recv_ts;   
                    msg.exchangeTimestamp = parse_u48(p.data + 5);
                    msg.messageType = p.data[0];

                    if (counter < 5  && myFile.is_open()) { 
                        myFile << "\n=========================\n" << 
                        "\nMESSAGETYPE: " << msg.messageType <<
                        "\nSTOCKLOCATE: " << msg.stockLocate <<
                        "\nTRACKINGNUMBER: " << msg.trackingNumber <<
                        "\nRECEIVETIMESTAMP: " << msg.receiveTimestamp <<
                        "\nEXCHANGETIMESTAMP: " << msg.exchangeTimestamp <<
                        "\nLATENCY: " << (msg.exchangeTimestamp - msg.receiveTimestamp) <<
                        "\nEVENTCODE: " << msg.eventCode;
                        counter++;
                    }
                    myFile.close();

                    break;
                }
                case 'A': {
                    std::ofstream myFile("testparse.txt", std::ios::app);
                    AddOrderMessageA msg(p.recv_ts);
                    msg.messageType = p.data[0];
                    msg.stockLocate = parse_u16(p.data + 1);
                    msg.trackingNumber = parse_u16(p.data + 3);
                    msg.exchangeTimestamp = parse_u48(p.data + 5);
                    memcpy(&msg.orderReferenceNumber, p.data + 11, 8);
                    msg.orderReferenceNumber = be64toh(msg.orderReferenceNumber);                    
                    msg.buySellIndicator = p.data[19];
                    memcpy(&msg.shares, p.data+20, 4);
                    msg.shares = ntohl(msg.shares);
                    memcpy(msg.stock, p.data + 24 , 8);
                    msg.stock[8] = '\0'; // aseguramos que el string esté null-terminated
                    memcpy(&msg.price, p.data+32, 4);
                    msg.price = ntohl(msg.price);
                    double norm_price = msg.price / 10000.0; // los precios en ITCH están multiplicados por 10000

                    if (counter < 5  && myFile.is_open()) { 
                        myFile << "\n=========================\n" << 
                        "\nMESSAGETYPE: " << msg.messageType <<
                        "\nSTOCKLOCATE: " << msg.stockLocate <<
                        "\nTRACKINGNUMBER: " << msg.trackingNumber <<
                        "\nRECEIVETIMESTAMP: " << msg.receiveTimestamp <<
                        "\nEXCHANGETIMESTAMP: " << msg.exchangeTimestamp <<
                        "\nLATENCY: " << (msg.exchangeTimestamp - msg.receiveTimestamp) <<
                        "\nORDERREFERENCENUMBER: " << msg.orderReferenceNumber <<
                        "\nBUYSELLINDICATOR: " << msg.buySellIndicator <<
                        "\nSHARES: " << msg.shares <<
                        "\nSTOCK: " << msg.stock <<
                        "\nPRICE: " << norm_price;
                        counter++;
                    }
                    myFile.close();
                    break;
                }
                case 'F': {
                    std::ofstream myFile("testparse.txt", std::ios::app);
                    AddOrderMessageF msg(p.recv_ts);
                    msg.messageType = p.data[0];
                    msg.stockLocate = parse_u16(p.data + 1);
                    msg.trackingNumber = parse_u16(p.data + 3);
                    msg.exchangeTimestamp = parse_u48(p.data + 5);
                    memcpy(&msg.orderReferenceNumber, p.data + 11, 8);
                    msg.orderReferenceNumber = be64toh(msg.orderReferenceNumber);                    
                    msg.buySellIndicator = p.data[19];
                    memcpy(&msg.shares, p.data+20, 4);
                    msg.shares = ntohl(msg.shares);
                    memcpy(msg.stock, p.data + 24 , 8);
                    memcpy(&msg.price, p.data+32, 4);
                    msg.price = ntohl(msg.price);
                    double norm_price = msg.price / 10000.0; // los precios en ITCH están multiplicados por 10000
                    memcpy(msg.attribution, p.data + 36,4);
                    msg.attribution[4] = '\0'; // aseguramos que el string esté null-terminated
                    if (counter < 5  && myFile.is_open()) { 
                        myFile << "\n=========================\n" << 
                        "\nMESSAGETYPE: " << msg.messageType <<
                        "\nSTOCKLOCATE: " << msg.stockLocate <<
                        "\nTRACKINGNUMBER: " << msg.trackingNumber <<
                        "\nRECEIVETIMESTAMP: " << msg.receiveTimestamp <<
                        "\nEXCHANGETIMESTAMP: " << msg.exchangeTimestamp <<
                        "\nLATENCY: " << (msg.exchangeTimestamp - msg.receiveTimestamp) <<
                        "\nORDERREFERENCENUMBER: " << msg.orderReferenceNumber <<
                        "\nBUYSELLINDICATOR: " << msg.buySellIndicator <<
                        "\nSHARES: " << msg.shares <<
                        "\nSTOCK: " << msg.stock <<
                        "\nPRICE: " << norm_price <<
                        "\nATTRIBUTION: " << msg.attribution;
                        counter++;
                    }
                    myFile.close();
                    break;
                }
                default:
                    continue; //ignoremos otros tipos
                    //std::cout << "Received unknown message type: " << p.data[0] << "\n";
            }
            

        }

        munmap(shm, sizeof(SharedMemoryBuffer)); //desmapear la memoria compartida del espacio de direcciones del proceso      

        _exit(0);//counting or not counting gang violence
    }
    else { //proceso padre, receptor del ITCH

        while (true) {

            //cosas del servidor UDP
            PacketRaw pck;
            // clear buffer
            memset(pck.data, 0, sizeof(pck.data));
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);
            
            // 4. Receive binary packet
            ssize_t bytes_received =
                recvfrom(
                    server_fd,
                    pck.data,
                    sizeof(pck.data),
                    0,
                    (struct sockaddr *)&sender_addr,
                    &sender_len
                );

            if (bytes_received < 0) {
                perror("recvfrom failed");
                continue;
            }
            pck.len = bytes_received;
            pck.recv_ts = MecanismosIPC::a_ns(CLOCK_MONOTONIC);
            //comunicación con proceso hijo a través de la shared memory
            int next = (shm->in + 1) % num_buffer; // siguiente posición de escritura, circular buffer
            while (next == shm->out) { // si el siguiente índice de escritura es igual al índice de lectura, el buffer está lleno
                continue; // esperamos a que el consumidor consuma algún paquete para liberar espacio
            }
            shm->packets[shm->in] = pck; // escribir el paquete en la posición actual de escritura, a
            shm->in = next; // avanzar el índice de escritura
            /*
            std::ofstream myFile("output.txt", std::ios::app);
            if (myFile.is_open()) {
            myFile << "\n=========================\n"
            << "Received " << bytes_received << " bytes\n"
            << "Receive timestamp(ns): " << recv_ts << "\n"
            << "Message Type: " << message_type << "\n";}
            myFile.close();
            */           
        }

        shm->done = true; // señalamos que el productor terminó de producir, para que el consumidor pueda terminar de consumir lo que queda en el buffer y luego terminar también
        wait(nullptr);
        munmap(shm, sizeof(SharedMemoryBuffer)); //desmapear la memoria compartida del espacio de direcciones del proceso
        shm_unlink(shm_name); //eliminar la memoria compartida

    }
    close(server_fd);

    return 0;
}