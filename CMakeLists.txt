cmake_minimum_required(VERSION 3.13)
project(dropbox_server C)

set(CMAKE_C_STANDARD 99)

add_executable(dropbox_server main.c list.c handler.c client.c connection.c options.c)
