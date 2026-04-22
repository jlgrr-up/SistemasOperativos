#pragma region includes

#include <cstdint>
#include <limits>

#include <ctime>

#include <unistd.h>
#include <sys/wait.h>
#include <string>

#include <cstdlib>

//4 mssg queues
#include <sys/ipc.h>
#include <sys/msg.h>

//4 shared memory
#include <sys/mman.h>
#include <fcntl.h>

#pragma endregion

#pragma region estructuras_y_generales

#define linea std::cout << std::string(20, '-') << std::endl

const unsigned int num_ticks = 100000;

typedef uint64_t ui;

enum class EventType : uint8_t {
    NEW_ORDER = 0,
    CANCEL_ORDER = 1,
    TRADE = 2
};

struct Timestamp {
    ui raw_ns;
    ui mono_ns;
};

struct Tick {
    ui id;              
    Timestamp timestamp_ns; 
    double precio;            
    double volumen;           
    EventType tipo;           
};


struct Measurement {
    ui total_latency;
    ui min_latency;
    ui max_latency;

    Measurement(){
        total_latency = 0;
        min_latency = std::numeric_limits<ui>::max();
        max_latency = 0;
    }
};


struct SharedBuffer {
    long msg_type;
    Tick tick;
}; //4 queues 

#define num_buffer 1024

struct SharedMemoryBuffer {
    Tick ticks[num_buffer];
    ui in;
    ui out; 
    bool done; //para que el consumidor sepa cuando el productor terminó de producir, aunque también se podría hacer con un mensaje especial como en las colas de mensajes

    SharedMemoryBuffer() {
        in = 0;
        out = 0;
        done = false;
    }
};



#pragma endregion


class MecanismosIPC {
private:
    static ui a_ns(clockid_t);
    static void print_measurement(int, Measurement, Measurement);
    static Tick generar_tick(ui);
public: 
    static int pipeIPC();
    static int msgQueueIPC();
    static int sharedMemory();    
};