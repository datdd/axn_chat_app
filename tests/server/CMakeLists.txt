add_executable(
    server_tests
    client_manager_test.cc
    server_integration_test.cc
)

target_link_libraries(
    server_tests
    PRIVATE
    GTest::gtest_main
    server_lib
)

# Discover tests for CTest.
include(GoogleTest)
gtest_discover_tests(server_tests)