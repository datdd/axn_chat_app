add_executable(
    server_tests
    client_manager_test.cpp
    server_integration_test.cpp
)

target_link_libraries(
    server_tests
    PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    server_lib
)

# Discover tests for CTest.
include(GoogleTest)
gtest_discover_tests(server_tests)