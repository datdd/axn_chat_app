cmake_minimum_required(VERSION 3.10)
project(ChatApplication VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)

message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "CMake Version: ${CMAKE_VERSION}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")