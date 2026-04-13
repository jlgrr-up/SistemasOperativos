#include "MecanismosIPC.h"

#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <random>

#include <iostream>

Tick MecanismosIPC::generar_tick(ui id) {
    static std::mt19937_64 generar_nums_aleatorios(time(nullptr));//generador de números aleatorios con una semilla aleatoria, solo una vez
    static std::uniform_real_distribution<double> precio_dist(99.0, 101.0);//para vals decimales
    static std::uniform_real_distribution<double> volumen_dist(1.0, 100.0);
    static std::uniform_int_distribution<int> tipo_dist(0, 2);

    Tick t;
    t.id = id;
    t.timestamp_ns.raw_ns = MecanismosIPC::a_ns(CLOCK_MONOTONIC_RAW);
    t.timestamp_ns.mono_ns = MecanismosIPC::a_ns(CLOCK_MONOTONIC);
    t.precio = precio_dist(generar_nums_aleatorios);
    t.volumen = volumen_dist(generar_nums_aleatorios);
    t.tipo = static_cast<EventType>(tipo_dist(generar_nums_aleatorios));

    return t;
}

ui MecanismosIPC::a_ns(clockid_t clk) {
    struct timespec ts;
    clock_gettime(clk, &ts);
    return (ui)(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}




int MecanismosIPC::pipeIPC(){
    
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
        Measurement m_monotonic, m_raw;
        unsigned int count = 0;

        while (read(fd[0], &t, sizeof(Tick)) > 0) {//read recibe la pipe, la direccion de memoria donde se va a guardar el tick y el tamaño del tick, devuelve el numero de bytes leidos, si es 0 es porque no hay mas datos que leer

            ui now_monotonic = MecanismosIPC::a_ns(CLOCK_MONOTONIC);
            ui now_raw = MecanismosIPC::a_ns(CLOCK_MONOTONIC_RAW);
            ui latency_monotonic = now_monotonic - t.timestamp_ns.mono_ns;
            ui latency_raw = now_raw - t.timestamp_ns.raw_ns;

            m_monotonic.total_latency += latency_monotonic;
            m_raw.total_latency += latency_raw;
            count++;

            if (latency_monotonic < m_monotonic.min_latency) m_monotonic.min_latency = latency_monotonic;
            if (latency_monotonic > m_monotonic.max_latency) m_monotonic.max_latency = latency_monotonic;
            if (latency_raw < m_raw.min_latency) m_raw.min_latency = latency_raw;
            if (latency_raw > m_raw.max_latency) m_raw.max_latency = latency_raw;
        }

        close(fd[0]);

        if (count > 0) {
            linea;
            std::cout << "Mensajes recibidos: " << count << "\n";
            std::cout << std::endl;
            std::cout << "Latencia promedio monotonic clock (ns): " << m_monotonic.total_latency / count << "\n";
            std::cout << "Latencia minima monotonic clock(ns): " << m_monotonic.min_latency << "\n";
            std::cout << "Latencia maxima monotonic clock(ns): " << m_monotonic.max_latency << "\n";
            std::cout << std::endl;
            std::cout << "Latencia promedio raw clock (ns): " << m_raw.total_latency / count << "\n";
            std::cout << "Latencia minima raw clock(ns): " << m_raw.min_latency << "\n";
            std::cout << "Latencia maxima raw clock(ns): " << m_raw.max_latency << "\n";
        
            
        }
        _exit(0);//lowkirkenuinly kill child
 
    }

    else {//padre
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
 

