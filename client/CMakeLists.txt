add_library(
    client_lib
    src/chat_client.cpp
    src/server_connection.cpp
)

target_include_directories(client_lib PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(client_lib PUBLIC common Threads::Threads)
target_compile_features(client_lib PRIVATE cxx_std_17)

add_executable(chat_client src/main.cpp)
target_link_libraries(chat_client PRIVATE client_lib)