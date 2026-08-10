include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
if(POLICY CMP0148)
    cmake_policy(SET CMP0148 NEW)
endif()
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(tokenizer_tests
    core/tokenizer_test.cpp
)

target_link_libraries(tokenizer_tests PRIVATE tokenizer_core GTest::gtest_main)

target_compile_options(tokenizer_tests PRIVATE -Wall -Wextra -Wpedantic)

gtest_discover_tests(tokenizer_tests)
