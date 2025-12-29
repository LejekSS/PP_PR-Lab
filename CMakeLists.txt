cmake_minimum_required(VERSION 3.29)
project(PR_Lab C)

set(CMAKE_C_STANDARD 11)

add_executable(PR_Lab main.c
        watek_komunikacyjny.c
        watek_glowny.c
        util.c)
