cmake_minimum_required(VERSION 3.10)

# Define a static library named 'common'
add_library(common STATIC
    src/protocol.cpp
    src/socket.cpp
)

# Specify the C++ standard to use
target_compile_features(common PUBLIC cxx_std_11)

# Make the "include" directory available for any target that links against 'common'
target_include_directories(common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)