add_executable(tokenizer
    main.cpp
)

target_link_libraries(tokenizer PRIVATE tokenizer_core CLI11::CLI11)

target_compile_options(tokenizer PRIVATE -Wall -Wextra -Wpedantic)
