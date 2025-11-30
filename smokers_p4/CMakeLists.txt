cmake_minimum_required(VERSION 3.10)
project(smokers_p4 C)

set(CMAKE_C_STANDARD 11)

# POSIX threads + semaphores
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(p4_agent
        p4_agent.c
        smokers_p4_common.h)

add_executable(p4_smoker
        p4_smoker.c
        smokers_p4_common.h)

add_executable(p4_observer
        p4_observer.c
        smokers_p4_common.h)
