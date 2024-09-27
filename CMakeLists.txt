cmake_minimum_required(VERSION 3.29)
project(CrowFS C)

set(CMAKE_C_STANDARD 11)

add_executable(CrowFS main.c
        crowfs.c
        crowfs.h)
