cmake_minimum_required(VERSION 3.13)
project(http-server)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(http-server main.c)
