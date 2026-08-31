#include <mlx/mlx.h>

#include <gtest/gtest.h>

// Smoke test: MLX headers compile and libmlx links into the test build.
// (tokenizer_core links MLX PUBLIC, so every target that links core gets it.)

namespace mx = mlx::core;

TEST(MlxSmokeTest, ArrayArithmeticEvaluates) {
  auto a = mx::array({1.0f, 2.0f, 3.0f});
  auto b = mx::array({4.0f, 5.0f, 6.0f});

  auto total = mx::sum(a + b); // lazy: [5, 7, 9] -> 21
  mx::eval(total);

  EXPECT_FLOAT_EQ(total.item<float>(), 21.0f);
}
