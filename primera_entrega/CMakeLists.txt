cmake_minimum_required(VERSION 3.10)

project(HFTBOT)

set(CMAKE_CXX_STANDARD 17)

add_executable(analyzer main.cpp MecanismosIPC.h)
