#include <chrono>
#include <iostream>

int main(){
    std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    std::cout << now.time_since_epoch().count() << std::endl;
    return 0;
}