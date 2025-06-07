cmake_minimum_required(VERSION 3.10)

add_executable(client
    src/main.cpp
    src/chat_client.cpp
    src/server_connection.cpp
)

target_compile_features(client PUBLIC cxx_std_17)

# Make include directories available for the target
target_include_directories(client PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Link against the common library
target_link_libraries(client PRIVATE 
    common
    Threads::Threads
)