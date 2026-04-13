#include <iostream>
#include <cstdint>
#include <string>

#include "MecanismosIPC.h"

/*
    TODO:
    -menú y todo eso que nos permita escoger diferentes métodos de IPC y sus diferentes latencias de ticks
    los mecanismos de IPC que se podrían implementar son:
    -:) pipes (ya implementado) 
    -shared memory (memoria compartida)
    -message queues (colas de mensajes)
    -sockets (comunicación entre procesos a través de la red, incluso en la misma máquina)
    -memory-mapped files (archivos mapeados en memoria, que permiten compartir datos entre procesos a través de un archivo)
    -semaforos (un mecanismo de sincronización que permite a los procesos coordinar el acceso a recursos compartidos, aunque no es un método de IPC en sí mismo, puede ser utilizado junto con otros métodos de IPC para mejorar la eficiencia y la sincronización entre procesos)
    
    -contextualizar estos casos en un escenario de mercado real, por ende es necesario construir un orderbook
    -una vez hecho esto, podremos hacer simulaciones (ignorantes) del rendimiento de diferentes métodos de IPC en un escenario de mercado real, 
    con diferentes volúmenes de ticks, y así poder sacar conclusiones sobre cuál método es mejor para cada caso.
*/



/*
//#include <chrono> //esto ya no sirve, para la funcion 'viejita'
ui a_ns_viejita() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::high_resolution_clock::now().time_since_epoch()).count();
} //tendremos que reemplazar esto a clock_gettime(CLOCK_MONOTONIC, &ts) para hacer una syscall y q sea mas pro
*///al hacer time en lugar de chrono, obtenemos la latencia real del SO




int main(){
    std::string entrada_s;
    unsigned short entrada;
    bool cont = true; 

    while(cont == true){
        linea;
        std::cout << "PID: " << getpid() << "\n";
        std::cout << "Seleccione el método de IPC a probar:\n";
        std::cout << "1. Pipes\n";
        std::cout << "2. Salir\n";
        std::cout << "Ingrese su opción: ";
        std::cin >> entrada_s;
        entrada = std::stoi(entrada_s);
        
        switch(entrada){
            case 1:
                MecanismosIPC::pipeIPC();
                break;
            case 2:
                cont = false;
                break;
            default:
                std::cout << "Opción no válida, intente de nuevo.\n";
        }
    }
}