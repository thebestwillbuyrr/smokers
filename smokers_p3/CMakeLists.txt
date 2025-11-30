cmake_minimum_required(VERSION 3.10)
project(smokers_p3 C)

set(CMAKE_C_STANDARD 11)

# Чтобы работали POSIX API
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(p3_agent
        p3_agent.c
        smokers_p3_common.h)

add_executable(p3_smoker
        p3_smoker.c
        smokers_p3_common.h)

add_executable(p3_observer
        p3_observer.c
        smokers_p3_common.h)
