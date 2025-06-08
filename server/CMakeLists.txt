cmake_minimum_required(VERSION 3.10)

add_executable(server
    src/main.cpp
    src/server.cpp
    src/epoll_manager.cpp
    src/client_manager.cpp
    src/client_session.cpp
)

target_compile_features(server PRIVATE cxx_std_17)

target_include_directories(server PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(server PRIVATE
    common
)