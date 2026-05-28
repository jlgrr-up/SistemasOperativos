#include "StockBook.h"


StockBook::StockBook() {
    bestbid = 0;
    bestask = 0xFFFFFFFF;//máximo posible para que vaya bajando
    memset(orders, 0, sizeof(orders));
}

int StockBook::find_slot(uint64_t refNum) {
        //cadsa orden tiene un orderRefNum único, así que primero buscamos si ya existe una orden con ese orderRefNum para actualizarla. 
        //Si no existe, buscamos una orden inactiva para insertar la nueva orden. Si no hay órdenes inactivas, retornamos -1 indicando que no se pudo encontrar un slot disponible.
        unsigned int start_slot = refNum % 5000; // empezamos a buscar desde un índice basado en el orderRefNum para distribuir las órdenes de manera más uniforme
        for (int i = 0; i < 5000; ++i) {
            unsigned int slot = (start_slot + i) % 5000; // búsqueda circular, si llegamos al final del array volvemos al inicio
            if (orders[slot].orderRefNum == refNum || !orders[slot].active) {
                return slot;
            }
        }
        return -1;
};