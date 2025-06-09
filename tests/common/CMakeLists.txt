add_executable(
    common_tests
    protocol_test.cpp
    socket_test.cpp
)

# Link the executable against GTest and the 'common' library itself.
target_link_libraries(
    common_tests
    PRIVATE
    GTest::gtest_main
    common
)

# Discover all individual tests within the executable and add them to CTest.
include(GoogleTest)
gtest_discover_tests(common_tests)