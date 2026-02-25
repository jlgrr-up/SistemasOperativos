#include <iostream>
#include <cstdint>


#include <chrono>
#include <random>
#include <ctime>

#include <unistd.h>
#include <sys/wait.h>

#include <limits>
#include <string>


/*
    TODO:
    -menú y todo eso que nos permita escoger diferentes métodos de IPC y sus diferentes latencias de ticks
    los mecanismos de IPC que se podrían implementar son:
    -pipes (ya implementado)
    -shared memory (memoria compartida)
    -message queues (colas de mensajes)
    -sockets (comunicación entre procesos a través de la red, incluso en la misma máquina)
    -memory-mapped files (archivos mapeados en memoria, que permiten compartir datos entre procesos a través de un archivo)
    -eventfd (un mecanismo de notificación entre procesos que permite a un proceso esperar a que ocurra un evento en otro proceso sin necesidad de hacer una syscall para cada operación de espera)
    -semaforos (un mecanismo de sincronización que permite a los procesos coordinar el acceso a recursos compartidos, aunque no es un método de IPC en sí mismo, puede ser utilizado junto con otros métodos de IPC para mejorar la eficiencia y la sincronización entre procesos)
    

    -esto implicará globalizar la función para medir la latencia (main) para que diferentes procesos puedan medirlo sin escribir código de más.
    -contextualizar estos casos en un escenario de mercado real, por ende es necesario construir un orderbook
    -una vez hecho esto, podremos hacer simulaciones (ignorantes) del rendimiento de diferentes métodos de IPC en un escenario de mercado real, 
    con diferentes volúmenes de ticks, y así poder sacar conclusiones sobre cuál método es mejor para cada caso.
*/

typedef uint64_t ui;

enum class EventType : uint8_t {
    NEW_ORDER = 0,
    CANCEL_ORDER = 1,
    TRADE = 2
};

struct Tick {
    ui id;              
    ui timestamp_ns; 
    double precio;            
    double volumen;           
    EventType tipo;           
};

ui a_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::high_resolution_clock::now().time_since_epoch()).count();
} //tendremos que reemplazar esto a clock_gettime(CLOCK_MONOTONIC, &ts) para hacer una syscall y q sea mas pro

Tick generar_tick(ui id) {
    static std::mt19937_64 generar_nums_aleatorios(time(nullptr));//generador de números aleatorios con una semilla aleatoria, solo una vez
    static std::uniform_real_distribution<double> precio_dist(99.0, 101.0);//para vals decimales
    static std::uniform_real_distribution<double> volumen_dist(1.0, 100.0);
    static std::uniform_int_distribution<int> tipo_dist(0, 2);

    Tick t;
    t.id = id;
    t.timestamp_ns = a_ns();
    t.precio = precio_dist(generar_nums_aleatorios);
    t.volumen = volumen_dist(generar_nums_aleatorios);
    t.tipo = static_cast<EventType>(tipo_dist(generar_nums_aleatorios));

    return t;
}

int pipeIPC(){
    std::cout << std::string(20, '-') << std::endl;
    const unsigned int num_ticks = 100000;

    int fd[2];//read write
    if (pipe(fd) < 0) {
        std::cerr << "no se creo la pipe" << std::endl;
        return 1;
    }

    int pid = fork();

    if(pid < 0){
        std::cerr << "no se pudo creo el proceso hijo" << std::endl;
        return 1;
    }

    //proceso hijo
    if(pid == 0){//pid del pipe es file descriptor 0
        close(fd[1]); //NO VOY A ESCRIBIR, SOLO LEER
        //inicializamos variables para medir latencias ^_^
        Tick t;
        ui total_latency = 0;
        ui count = 0;
        ui min_latency = std::numeric_limits<ui>::max();
        ui max_latency = 0;

        while (read(fd[0], &t, sizeof(Tick)) > 0) {//read recibe la pipe, la direccion de memoria donde se va a guardar el tick y el tamaño del tick, devuelve el numero de bytes leidos, si es 0 es porque no hay mas datos que leer

            ui now = a_ns();
            ui latency = now - t.timestamp_ns;

            total_latency += latency;
            count++;

            if (latency < min_latency) min_latency = latency;
            if (latency > max_latency) max_latency = latency;
        }

        close(fd[0]);

        if (count > 0) {
            std::cout << "Mensajes recibidos: " << count << "\n";
            std::cout << "Latencia promedio (ns): " << total_latency / count << "\n";
            std::cout << "Latencia minima (ns): " << min_latency << "\n";
            std::cout << "Latencia maxima (ns): " << max_latency << "\n";
        }
    }
    else {
        close(fd[0]);//no lee

        for (ui i = 0; i < num_ticks; i++) {
            Tick t = generar_tick(i);
            write(fd[1], &t, sizeof(Tick));
        }

        close(fd[1]); // importante para terminar el read en hijo
        wait(nullptr);
    }


    return 0;
}
 
int main(){
    int entrada;
    bool cont = true; 

    while(cont){
        std::cout << "Seleccione el método de IPC a probar:\n";
        std::cout << "1. Pipes\n";
        std::cout << "2. Salir\n";
        std::cout << "Ingrese su opción: ";
        std::cin >> entrada;

        switch(entrada){
            case 1:
                pipeIPC();
                break;
            case 2:
                cont = false;
                break;
            default:
                std::cout << "Opción no válida, intente de nuevo.\n";
        }
    }
}