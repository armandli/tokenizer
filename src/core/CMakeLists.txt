add_library(tokenizer_core
  bpe_builder.cpp
)

target_include_directories(tokenizer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# MLX is part of the public interface: tensor types will surface in core headers,
# and the app + tests link core, so they inherit MLX transitively.
target_link_libraries(tokenizer_core PUBLIC mlx)

target_compile_options(tokenizer_core PRIVATE -Wall -Wextra -Wpedantic)
