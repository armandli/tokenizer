add_executable(tokenizer_integration_tests
  bpe_pipeline_test.cpp
)

# Arrow is linked here only so the tests can WRITE the .arrow fixtures they
# then read back; production code links it privately inside tokenizer_core.
target_link_libraries(tokenizer_integration_tests PRIVATE bpe_test_support Arrow::arrow_shared)

gtest_discover_tests(tokenizer_integration_tests PROPERTIES LABELS integration)
