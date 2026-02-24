#include <iostream>
#include <cstdint>


#include <chrono>
#include <random>
#include <ctime>

#include <unistd.h>
#include <sys/wait.h>

#include <limits>

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

int main(){
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

