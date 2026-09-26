add_library(tokenizer_core
  arrow_io.cpp
  bpe_builder.cpp
  bpe_trainer.cpp
  corpus_input.cpp
  merge_table_io.cpp
  pretokenizer.cpp
)

target_include_directories(tokenizer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# MLX is part of the public interface: tensor types will surface in core headers,
# and the app + tests link core, so they inherit MLX transitively.
# simdjson is a private implementation detail of merge_table_io.cpp, and Arrow
# of arrow_io.cpp -- no simdjson or Arrow type appears in any header.
target_link_libraries(tokenizer_core
  PUBLIC mlx Threads::Threads
  PRIVATE simdjson::simdjson Arrow::arrow_shared
)

target_compile_options(tokenizer_core PRIVATE -Wall -Wextra -Wpedantic)
