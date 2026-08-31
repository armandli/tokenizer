add_executable(tokenizer_unit_tests
  bpe_builder_test.cpp
  mlx_smoke_test.cpp
)

target_link_libraries(tokenizer_unit_tests PRIVATE bpe_test_support)

gtest_discover_tests(tokenizer_unit_tests PROPERTIES LABELS unit)
