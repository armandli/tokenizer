add_library(tokenizer_core
  bpe_builder.cpp
  merge_table_io.cpp
)

target_include_directories(tokenizer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# MLX is part of the public interface: tensor types will surface in core headers,
# and the app + tests link core, so they inherit MLX transitively.
# simdjson is a private implementation detail of merge_table_io.cpp -- no
# simdjson type appears in any header.
target_link_libraries(tokenizer_core
  PUBLIC mlx
  PRIVATE simdjson::simdjson
)

target_compile_options(tokenizer_core PRIVATE -Wall -Wextra -Wpedantic)
