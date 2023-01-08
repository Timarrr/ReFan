cmake_minimum_required(VERSION 3.10)
project("ReFan" VERSION 0.1)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(refan refan.cpp)
target_link_libraries(refan confini)