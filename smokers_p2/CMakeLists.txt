cmake_minimum_required(VERSION 3.26)
project(smokers_p2 C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(smokers_p2
        p2_agent.c
        p2_smoker.c
        smokers_p2_common.h)
