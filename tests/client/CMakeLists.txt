add_executable(
    client_tests
    client_integration_test.cc
)

# Link against GTest and the 'client_lib'.
target_link_libraries(
    client_tests
    PRIVATE
    GTest::gtest_main
    client_lib
)

# Discover tests for CTest.
include(GoogleTest)
gtest_discover_tests(client_tests)