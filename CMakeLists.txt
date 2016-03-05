cmake_minimum_required(VERSION 3.3)
project(shell_like)
add_definitions(-std=c99)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES )

#add_executable(shell_like ${SOURCE_FILES})
add_executable(pipe_demo.x pipe_demo.c)
add_executable(1pipe.x 1pipe.c parsetools.c parsetools.h constants.h)
add_executable(pipes.x main.c parsetools.c parsetools.h constants.h)