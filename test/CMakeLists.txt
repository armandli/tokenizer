# GoogleTest: prefer a system install. Other system dependencies (MLX) put
# ${prefix}/include on the compile line, so a system GoogleTest there would
# shadow a FetchContent copy's headers and cause an ABI mismatch against its
# library. Using the system package keeps headers and library in lock-step;
# fall back to a pinned FetchContent copy only when no system GoogleTest exists.
find_package(GTest CONFIG QUIET)
if(GTest_FOUND)
    message(STATUS "Using system GoogleTest ${GTest_VERSION}")
else()
    message(STATUS "System GoogleTest not found; fetching v1.15.2")
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
endif()

include(GoogleTest)

option(TOKENIZER_TEST_SANITIZERS "Build test binaries with AddressSanitizer/UBSan" OFF)

# Everything the unit and integration test targets have in common: the code
# under test, GoogleTest's main(), the shared header-only helpers, and warnings.
add_library(bpe_test_support INTERFACE)
target_include_directories(bpe_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/support)
target_link_libraries(bpe_test_support INTERFACE tokenizer_core GTest::gtest_main)
target_compile_options(bpe_test_support INTERFACE -Wall -Wextra -Wpedantic)

if(TOKENIZER_TEST_SANITIZERS)
  target_compile_options(bpe_test_support INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(bpe_test_support INTERFACE -fsanitize=address,undefined)
endif()

add_subdirectory(unit)
add_subdirectory(integration)
