#include <cstdint>
#include <cstring>

struct BookOrder {
    uint64_t orderRefNum = 0; // número de referencia de la orden, único para cada orden
    uint32_t shares = 0; // cantidad de acciones
    uint32_t price = 0; // precio en formato entero (precio real multiplicado por 10,000)
    char buySellIndicator = 0; // 'B' o 'S'
    bool active = false; // indica si la orden está activa (no cancelada ni completamente ejecutada). 
};

class StockBook{
    public:
        BookOrder orders[5000]; // un stock puede tener hasta 5000 órdenes activas simultáneamente
        uint32_t bestbid;
        uint32_t bestask;
        
        StockBook();
        int find_slot(uint64_t refNum); // encuentra el índice de la orden con el orderRefNum dado, o el índice de una orden inactiva para insertar una nueva orden
};