#include <iostream>
#include <fstream>
#include <string>

using namespace std;

int main() {
	setlocale(LC_ALL, "Spanish");
	string nombre_archivo = "doc.txt";
	string a_escribir = "";
	cout << string(20, '-') << endl;
	cout << "¿Qué le quieres escribir al archivo?" << endl;
	getline(cin, a_escribir);
	a_escribir = a_escribir + "\n";

	fstream mi_txt;
	mi_txt.open(nombre_archivo, ios::app);//segundo parámetro es el modo en el que abrimos el doc, 'app' es de append, añadimos datos al final del documento. 
	mi_txt.write(a_escribir.c_str(), a_escribir.size());//primer parámetro es string, y segundo es el tamaño del texto a escribir
	mi_txt.close();//cierra el archivo

	return EXIT_SUCCESS;
}