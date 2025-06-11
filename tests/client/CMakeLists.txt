add_executable(
    client_tests
    chat_client_test.cpp
    # client_integration_test.cpp
)

# Link against GTest and the 'client_lib'.
target_link_libraries(
    client_tests
    PRIVATE
    GTest::gtest_main
    GTest::gmock_main
    client_lib
)

# Discover tests for CTest.
include(GoogleTest)
gtest_discover_tests(client_tests)