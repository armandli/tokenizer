cmake_minimum_required(VERSION 3.25)
project(tokenizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(TOKENIZER_BUILD_TESTS "Build tests" ON)

# --- Third-party dependencies ------------------------------------------------
include(FetchContent)

# MLX: linear-algebra / tensor library for Apple silicon. Used from an existing
# system install (Homebrew or `pip install mlx`) rather than built from source.
# Add the Homebrew prefix to the search path so find_package works no matter
# which cmake binary is invoked.
if(APPLE)
  if(NOT DEFINED HOMEBREW_PREFIX)
    execute_process(COMMAND brew --prefix
      OUTPUT_VARIABLE HOMEBREW_PREFIX
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
  endif()
  if(NOT HOMEBREW_PREFIX AND EXISTS "/opt/homebrew")
    set(HOMEBREW_PREFIX "/opt/homebrew")
  endif()
  if(HOMEBREW_PREFIX)
    list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
  endif()
endif()
find_package(MLX CONFIG REQUIRED)
message(STATUS "Found MLX ${MLX_VERSION} (metal: ${MLX_BUILD_METAL})")

# CLI11: command-line parsing for the app. Header-only. Use a system install if
# one exists, otherwise fetch a pinned copy. SYSTEM keeps its headers out of
# -Wpedantic.
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.7.2
  SYSTEM
  FIND_PACKAGE_ARGS NAMES CLI11
)
set(CLI11_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cli11)

# simdjson: JSON parsing for merge-table load. Use a system install if one
# exists, otherwise fetch a pinned copy. SYSTEM keeps its headers out of
# -Wpedantic.
FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG v4.6.9
  SYSTEM
  FIND_PACKAGE_ARGS NAMES simdjson
)
set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(simdjson)
# ---------------------------------------------------------------------------

add_subdirectory(src)

if(TOKENIZER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
