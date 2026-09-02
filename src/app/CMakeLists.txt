add_executable(tokenize_bpe
    tokenize_bpe.cpp
)

target_link_libraries(tokenize_bpe PRIVATE tokenizer_core CLI11::CLI11)

target_compile_options(tokenize_bpe PRIVATE -Wall -Wextra -Wpedantic)

add_executable(build_bpe
    build_bpe.cpp
)

target_link_libraries(build_bpe PRIVATE tokenizer_core CLI11::CLI11)

target_compile_options(build_bpe PRIVATE -Wall -Wextra -Wpedantic)

add_executable(decode_bpe
    decode_bpe.cpp
)

target_link_libraries(decode_bpe PRIVATE tokenizer_core CLI11::CLI11)

target_compile_options(decode_bpe PRIVATE -Wall -Wextra -Wpedantic)
