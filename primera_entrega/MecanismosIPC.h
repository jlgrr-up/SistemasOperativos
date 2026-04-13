#include <cstdint>
#include <limits>

#include <ctime>

#include <unistd.h>
#include <sys/wait.h>
#include <string>

#include <cstdlib>

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

/*
struct SharedBuffer {
    Tick ticks[num_ticks];
    volatile unsigned short write_index;
    volatile unsigned short read_index;
};
*/

#pragma endregion



class MecanismosIPC {
private:
    static ui a_ns(clockid_t);
public: 
    static int pipeIPC();
    
    static Tick generar_tick(ui);
};