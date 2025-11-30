cmake_minimum_required(VERSION 3.16)
project(smokers_p1 C)

set(CMAKE_C_STANDARD 11)

add_executable(smokers_p1 smokers_p1.c
        smokers_p1.c)

target_link_libraries(smokers_p1 pthread)
