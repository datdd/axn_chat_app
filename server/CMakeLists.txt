add_library(
    server_lib
    src/server.cpp
    src/epoll_manager.cpp
    src/client_manager.cpp
    src/client_session.cpp
)

target_include_directories(server_lib PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(server_lib PUBLIC common)
target_compile_features(server_lib PRIVATE cxx_std_17)

add_executable(chat_server src/main.cpp)
target_link_libraries(chat_server PRIVATE server_lib)