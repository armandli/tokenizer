add_executable(tokenizer_unit_tests
  arrow_io_test.cpp
  bpe_builder_test.cpp
  bpe_trainer_test.cpp
  corpus_input_test.cpp
  tokenize_test.cpp
  decode_test.cpp
  merge_table_io_test.cpp
  pretokenizer_test.cpp
  gpt2_pattern_test.cpp
  gpt4_pattern_test.cpp
  mlx_smoke_test.cpp
)

# Arrow is linked here only so the tests can WRITE the .arrow fixtures they
# then read back; production code links it privately inside tokenizer_core.
target_link_libraries(tokenizer_unit_tests PRIVATE bpe_test_support Arrow::arrow_shared)

gtest_discover_tests(tokenizer_unit_tests PROPERTIES LABELS unit)
