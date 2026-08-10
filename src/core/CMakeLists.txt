add_library(tokenizer_core
    tokenizer.cpp
)

target_include_directories(tokenizer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(tokenizer_core PRIVATE -Wall -Wextra -Wpedantic)
