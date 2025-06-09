cmake_minimum_required(VERSION 3.10)
project(ChatApplication VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Include the FetchContent module to download dependencies
include(FetchContent)

# Declare the dependency on GoogleTest using FetchContent
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)

# Enable testing
enable_testing()
add_subdirectory(tests)

message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "CMake Version: ${CMAKE_VERSION}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")