#include <iostream> 
#include <cstring> //memcpy

#include <sys/socket.h> //socket, bind, sendto
#include <netinet/in.h> //sockaddr_in, htons, ntohs
#include <arpa/inet.h> // inet_pton
#include <unistd.h> //close, fork, _exit

#include "MecanismosIPC.h"
#include "StockBook.h"

#include <fstream> 

struct PacketRaw {
    char data[128];//mssgtype data[0] + payload data[1-127]
    ssize_t len;
    ui recv_ts;
}; //paquete recibido en binario. 

struct SharedMemoryBuffer {
    PacketRaw packets[num_buffer];
    ui in;
    ui out;
    bool done; //para que el consumidor sepa cuando el productor terminó de producir
}; //buffer entre proceso padre e hijo, e
//el padre escribe los paquetes recibidos del socket UDP, y el hijo los consume para parsearlos y escribir eventos parseados en la shared memory de la estrategia

//documentación eventos: https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf


struct StrategyEvent {
    char messageType;
    uint32_t stockLocate;
    uint64_t orderReferenceNumber;
    uint32_t shares;
    uint32_t price;
    char buySellIndicator;
    char eventCode;
    char stock[9];
    char attribution[5];
    uint64_t receiveTimestamp;
    uint64_t exchangeTimestamp;
};

#define ESTRATEGIA_CAPACIDAD 4096
struct StrategySharedMemoryBuffer {
    StrategyEvent events[ESTRATEGIA_CAPACIDAD];
    ui in;
    ui out;
    bool done;
};


uint16_t parse_u16(const char* ptr) {
    uint16_t val;
    memcpy(&val, ptr, sizeof(val));
    return ntohs(val);//network to host short
}
uint64_t parse_u48(const char* ptr) {
    uint64_t val = 0;
    for (int i = 0; i < 6; i++) { //los campos de 6 bytes en ITCH (como el exchangeTimestamp) se almacenan como big-endian, así que los parseamos byte a byte para obtener el valor correcto
        val = (val << 8) | (uint64_t)(unsigned char)ptr[i];
        //shift left para hacer espacio para el siguiente byte, y luego hacemos un OR con el siguiente byte para agregarlo al valor final
    }

    return val;
}

static StockBook MarketOrderBook[10000]; // un libro de órdenes por cada stock, indexados por stockLocate0

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

    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = INADDR_ANY; //escuchar en todas las interfaces de red disponibles
    address.sin_port = htons(port);

    // 3. Bind socket
    if (
        bind( //asociar el socket con la dirección y puerto configurados
            server_fd,
            (struct sockaddr *)&address,
            sizeof(address)
        ) < 0) {
        perror("Bind failed");
        return EXIT_FAILURE;
    }

    std::cout << "ITCH Receiver listening on port " << port << "...\n";


    //cosas de la shared memory:
    const char* shm_name = "/hft_ITCH";//create and readwrite
    //crear un objeto de memoria compartida con el nombre dado, con permisos de lectura y escritura, y con permisos de acceso 0666 (lectura y escritura para todos los usuarios
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666); 
    ftruncate(shm_fd, sizeof(SharedMemoryBuffer));//configurar el tamaño de la memoria compartida al tamaño de nuestro buffer

    SharedMemoryBuffer* shm = (SharedMemoryBuffer*)mmap(//memory map para mapear la memoria compartida en el espacio de direcciones del proceso, lo que nos devuelve un puntero a la dirección donde se encuentra la memoria compartida mapeada, que casteamos a nuestro tipo de buffer
        0, //direccion de memoria, el sistema decide
        sizeof(SharedMemoryBuffer), //tamaño del mapeo
        PROT_READ | PROT_WRITE, //protecciones de lectura y escritura
        MAP_SHARED, //compartido entre procesos
        shm_fd,//file descriptor de la memoria compartida
        0); //offset, no nos interesa
    shm->in = 0;
    shm->out = 0;
    shm->done = false;


    const char* shm_name_estrategia = "/hft_estrategia";//create and readwrite
    int shm_fd_estrategia = shm_open(shm_name_estrategia, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd_estrategia, sizeof(StrategySharedMemoryBuffer));
    StrategySharedMemoryBuffer* shm_estrategia = (StrategySharedMemoryBuffer*)mmap(
        0, //direccion de memoria, el sistema decide
        sizeof(StrategySharedMemoryBuffer), //tamaño del mapeo
        PROT_READ | PROT_WRITE, //protecciones de lectura y escritura
        MAP_SHARED, //compartido entre procesos
        shm_fd_estrategia,//file descriptor de la memoria compartida
        0); //offset, no nos interesa
    shm_estrategia->in = 0;
    shm_estrategia->out = 0;
    shm_estrategia->done = false;


    int pid_parser = fork();
    if (pid_parser < 0) {
        std::cerr << "no se pudo creo el proceso hijo" << std::endl;
        return EXIT_FAILURE;
    }

    if (pid_parser == 0) {//proceso hijo, parser del ITCH
      int counter = 0;
        
      while (true){  
            
            if (shm->in == shm->out) { // buffer vacío, no hay paquetes para consumir
                if (shm->done) {shm->done = true; break; } //si el productor terminó de producir y el buffer está vacío, terminamos de consumir
                continue;
            };

            PacketRaw p = shm->packets[shm->out]; // consumir el paquete
            shm->out = (shm->out + 1) % num_buffer; // circular buffer, avanzamos el índice de salida
            //aanzamos el índice de salida para que el productor pueda seguir escribiendo en el buffer, mientras nosotros procesamos el paquete que acabamos de consumir
            StrategyEvent msg;
            bool valido = false;

            switch (p.data[0]) { //el primer byte del paquete es el message type
                case 'S': {
                    std::ofstream myFile("testparse.txt", std::ios::app);
                    msg.eventCode = p.data[11];
                    msg.stockLocate = parse_u16(p.data + 1);
                    msg.receiveTimestamp = p.recv_ts;   
                    msg.exchangeTimestamp = parse_u48(p.data + 5);
                    msg.messageType = p.data[0];
                    valido = true;

                    if (counter < 5  && myFile.is_open()) { 
                        myFile << "\n=========================\n" << 
                        "\nMESSAGETYPE: " << msg.messageType <<
                        "\nSTOCKLOCATE: " << msg.stockLocate <<
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
                    msg.messageType = p.data[0];
                    msg.stockLocate = parse_u16(p.data + 1);
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
                    valido = true;

                    if (counter < 5  && myFile.is_open()) { 
                        myFile << "\n=========================\n" << 
                        "\nMESSAGETYPE: " << msg.messageType <<
                        "\nSTOCKLOCATE: " << msg.stockLocate <<
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
                    msg.messageType = p.data[0];
                    msg.stockLocate = parse_u16(p.data + 1);
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
                    valido = true;
                    if (counter < 5  && myFile.is_open()) { 
                        myFile << "\n=========================\n" << 
                        "\nMESSAGETYPE: " << msg.messageType <<
                        "\nSTOCKLOCATE: " << msg.stockLocate <<
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
            if (valido) {
                //escribimos el evento parseado en la shared memory de la estrategia, para que la estrategia pueda consumirlo
                int next_estrategia = (shm_estrategia->in + 1) % ESTRATEGIA_CAPACIDAD; // siguiente posición de escritura, circular buffer
                while (next_estrategia == shm_estrategia->out) { // si el siguiente índice de escritura es igual al índice de lectura, el buffer está lleno
                    continue; // esperamos a que el consumidor consuma algún evento para liberar espacio
                }
                shm_estrategia->events[shm_estrategia->in] = msg; // escribir el evento en la posición actual de escritura
                shm_estrategia->in = next_estrategia; // avanzar el índice de escritura
            }
        }

        munmap(shm, sizeof(SharedMemoryBuffer)); //desmapear la memoria compartida del espacio de direcciones del proceso      
        munmap(shm_estrategia, sizeof(SharedMemoryBuffer)); //desmapear la memoria compartida del espacio de direcciones del proceso      

        _exit(0);//counting or not counting gang violence
    }
    int pid_estrategia = fork();
    if (pid_estrategia < 0) {
        perror("fork failed");
        exit(1);
    }
    if (pid_estrategia == 0){ //nieto ig
        std::cout << "Detector de Asks y Bids iniciado. Monitoreando mercado...\n";
        while(true){
            if (shm_estrategia->in == shm_estrategia->out) { // buffer vacío, no hay eventos para consumir
                if (shm_estrategia->done) {shm_estrategia->done = true; break; } //si el productor terminó de producir y el buffer está vacío, terminamos de consumir
                continue;
            }

            StrategyEvent ev = shm_estrategia->events[shm_estrategia->out]; // consumir el evento
            shm_estrategia->out = (shm_estrategia->out + 1) % ESTRATEGIA_CAPACIDAD; // circular buffer, avanzamos el índice de salida

            uint16_t current_stock = ev.stockLocate;
            uint32_t old_bid = 0;
            uint32_t old_ask = 0;
            bool book_updated = false;


            if (ev.messageType == 'A' || ev.messageType == 'F') { // si es una orden nueva o una orden modificada (que en este caso la tratamos como una orden nueva, ya que no tenemos el mensaje de cancelación implementado)
                //esto es un & para obtener una referencia al libro de órdenes del stock correspondiente, para poder modificarlo directamente sin necesidad de copiarlo
                StockBook& book = MarketOrderBook[current_stock];

                old_bid = book.bestbid; //esto sirve para comparar si el Top of Book cambió después de procesar el evento, y así decidir si imprimimos un mensaje de actualización del libro
                old_ask = book.bestask; //lo mismo que el old_bid pero para el best ask

                int slot = book.find_slot(ev.orderReferenceNumber);
                if (slot != -1) {
                    book.orders[slot].orderRefNum = ev.orderReferenceNumber;
                    book.orders[slot].shares = ev.shares;
                    book.orders[slot].price = ev.price;
                    book.orders[slot].buySellIndicator = ev.buySellIndicator;
                    book.orders[slot].active = true;

                    // Actualizar el Top of Book si llega un precio competitivo
                    if (ev.buySellIndicator == 'B' && ev.price > book.bestbid) {
                        book.bestbid = ev.price;
                    } 
                    else if (ev.buySellIndicator == 'S' && ev.price < book.bestask) {
                        book.bestask = ev.price;
                    }
                    book_updated = true;
                }
            }
            if (book_updated) {
                StockBook& book = MarketOrderBook[current_stock];
                
                if (book.bestbid != old_bid || book.bestask != old_ask) {
                    // Formatear precio dividiendo entre 10,000 (Estándar ITCH)
                    double real_bid = book.bestbid / 10000.0;
                    double real_ask = 0.0;

                    if (book.bestask != 0xFFFFFFFF) {
                        real_ask = book.bestask / 10000.0;
                    }

                    std::cout << "Stock ID: " << current_stock  << " | Best BID: $" << real_bid << " <---> Best ASK: $" << real_ask << "\n";
                }
            }
        }
        munmap(shm, sizeof(SharedMemoryBuffer));
        munmap(shm_estrategia, sizeof(StrategySharedMemoryBuffer));
        _exit(0);
    }
    else { //proceso padre, receptor del ITCH

        while (true) {

            //cosas del servidor UDP
            PacketRaw pck;
            //clear buffer
            memset(pck.data, 0, sizeof(pck.data));
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);

            
            //receive binary packet
            ssize_t bytes_received = //ssize_t es un tipo entero con signo que se utiliza para representar el tamaño de los datos recibidos, y es el tipo de retorno de la función recvfrom
                recvfrom( //función que recibe datos de un socket
                    server_fd,//file descriptor del socket desde el cual se recibirán los datos
                    pck.data,//puntero al buffer donde se almacenarán los datos recibidos
                    sizeof(pck.data),//tamaño del buffer
                    0,//flags, en este caso 0 para comportamiento por defecto
                    (struct sockaddr *)&sender_addr,//puntero a una estructura donde se almacenará la dirección del remitente de los datos
                    &sender_len//puntero a una variable que inicialmente contiene el tamaño de la estructura de dirección, y que al retornar contendrá el tamaño real de la dirección del remitente
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
        shm_unlink(shm_name_estrategia); //eliminar la memoria compartida
        
    }
    close(server_fd);

    return 0;
}