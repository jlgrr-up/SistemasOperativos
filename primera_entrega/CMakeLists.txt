cmake_minimum_required(VERSION 3.10)

project(HFTBOT)

set(CMAKE_CXX_STANDARD 17)

add_executable(analyzer tester.cpp MecanismosIPC.cpp)

add_executable(server server.cpp MecanismosIPC.cpp)

add_executable(client main.cpp MecanismosIPC.cpp StockBook.cpp)

target_link_libraries(analyzer rt pthread)
target_link_libraries(server rt pthread)
target_link_libraries(client rt pthread)