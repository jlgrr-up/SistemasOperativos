#include <fstream>
#include <mutex>
#include <thread>
#include <iostream>

std::mutex txt_mutex;

void escribir_en_txt(std::string nombre_archivo, std::string a_escribir){
    std::lock_guard<std::mutex> guard(txt_mutex);
    std::fstream txt;
    txt.open(nombre_archivo, std::ios::app);
	txt.write(a_escribir.c_str(), a_escribir.size());
	txt.close();//cierra el archivo

}

int main() {
    std::cout << "Iniciando escritura en el archivo..." << std::endl;

	std::string nombre_archivo = "doc.txt";    

    std::thread t1(escribir_en_txt, nombre_archivo, "Hola desde el hilo 1\n");
    std::thread t2(escribir_en_txt, nombre_archivo, "Hola desde el hilo 2\n");
    std::thread t3(escribir_en_txt, nombre_archivo, "Hola desde el hilo 3\n");
    std::thread t4(escribir_en_txt, nombre_archivo, "Hola desde el hilo 4\n");
    std::thread t5(escribir_en_txt, nombre_archivo, "Hola desde el hilo 5\n");

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();

    std::cout << "Escritura en el archivo completada." << std::endl;
	return 0;
}