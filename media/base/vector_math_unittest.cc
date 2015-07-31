// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES
#include <cmath>

#include "base/command_line.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "media/base/vector_math.h"
#include "media/base/vector_math_testing.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;
using std::fill;

// Command line switch for runtime adjustment of benchmark iterations.
static const char kBenchmarkIterations[] = "vector-math-iterations";
static const int kDefaultIterations = 10;

// Default test values.
static const float kScale = 0.5;
static const float kInputFillValue = 1.0;
static const float kOutputFillValue = 3.0;

namespace media {

class VectorMathTest : public testing::Test {
 public:
  static const int kVectorSize = 8192;

  VectorMathTest() {
    // Initialize input and output vectors.
    input_vector.reset(static_cast<float*>(base::AlignedAlloc(
        sizeof(float) * kVectorSize, vector_math::kRequiredAlignment)));
    output_vector.reset(static_cast<float*>(base::AlignedAlloc(
        sizeof(float) * kVectorSize, vector_math::kRequiredAlignment)));
  }

  void FillTestVectors(float input, float output) {
    // Setup input and output vectors.
    fill(input_vector.get(), input_vector.get() + kVectorSize, input);
    fill(output_vector.get(), output_vector.get() + kVectorSize, output);
  }

  void VerifyOutput(float value) {
    for (int i = 0; i < kVectorSize; ++i)
      ASSERT_FLOAT_EQ(output_vector.get()[i], value);
  }

  int BenchmarkIterations() {
    int vector_math_iterations = kDefaultIterations;
    std::string iterations(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kBenchmarkIterations));
    if (!iterations.empty())
      base::StringToInt(iterations, &vector_math_iterations);
    return vector_math_iterations;
  }

 protected:
  int benchmark_iterations;
  scoped_ptr_malloc<float, base::ScopedPtrAlignedFree> input_vector;
  scoped_ptr_malloc<float, base::ScopedPtrAlignedFree> output_vector;

  DISALLOW_COPY_AND_ASSIGN(VectorMathTest);
};

// Ensure each optimized vector_math::FMAC() method returns the same value.
TEST_F(VectorMathTest, FMAC) {
  static const float kResult = kInputFillValue * kScale + kOutputFillValue;

  {
    SCOPED_TRACE("FMAC");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC(
        input_vector.get(), kScale, kVectorSize, output_vector.get());
    VerifyOutput(kResult);
  }

  {
    SCOPED_TRACE("FMAC_C");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC_C(
        input_vector.get(), kScale, kVectorSize, output_vector.get());
    VerifyOutput(kResult);
  }

#if defined(ARCH_CPU_X86_FAMILY) && defined(__SSE__)
  {
    SCOPED_TRACE("FMAC_SSE");
    FillTestVectors(kInputFillValue, kOutputFillValue);
    vector_math::FMAC_SSE(
        input_vector.get(), kScale, kVectorSize, output_vector.get());
    VerifyOutput(kResult);
  }
#endif
}

// Benchmark for each optimized vector_math::FMAC() method.  Original benchmarks
// were run with --vector-fmac-iterations=200000.
TEST_F(VectorMathTest, FMACBenchmark) {
  static const int kBenchmarkIterations = BenchmarkIterations();

  printf("Benchmarking %d iterations:\n", kBenchmarkIterations);

  // Benchmark FMAC_C().
  FillTestVectors(kInputFillValue, kOutputFillValue);
  TimeTicks start = TimeTicks::HighResNow();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    vector_math::FMAC_C(
        input_vector.get(), kScale, kVectorSize, output_vector.get());
  }
  double total_time_c_ms = (TimeTicks::HighResNow() - start).InMillisecondsF();
  printf("FMAC_C took %.2fms.\n", total_time_c_ms);

#if defined(ARCH_CPU_X86_FAMILY) && defined(__SSE__)
  // Benchmark FMAC_SSE() with unaligned size.
  ASSERT_NE((kVectorSize - 1) % (vector_math::kRequiredAlignment /
            sizeof(float)), 0U);
  FillTestVectors(kInputFillValue, kOutputFillValue);
  start = TimeTicks::HighResNow();
  for (int j = 0; j < kBenchmarkIterations; ++j) {
    vector_math::FMAC_SSE(
        input_vector.get(), kScale, kVectorSize - 1, output_vector.get());
  }
  double total_time_sse_unaligned_ms =
      (TimeTicks::HighResNow() - start).InMillisecondsF();
  printf("FMAC_SSE (unaligned size) took %.2fms; which is %.2fx faster than"
         " FMAC_C.\n", total_time_sse_unaligned_ms,
         total_time_c_ms / total_time_sse_unaligned_ms);

  // Benchmark FMAC_SSE() with aligned size.
  ASSERT_EQ(kVectorSize % (vector_math::kRequiredAlignment / sizeof(float)),
            0U);
  FillTestVectors(kInputFillValue, kOutputFillValue);
  start = TimeTicks::HighResNow();
  for (int j = 0; j < kBenchmarkIterations; ++j) {
    vector_math::FMAC_SSE(
        input_vector.get(), kScale, kVectorSize, output_vector.get());
  }
  double total_time_sse_aligned_ms =
      (TimeTicks::HighResNow() - start).InMillisecondsF();
  printf("FMAC_SSE (aligned size) took %.2fms; which is %.2fx faster than"
         " FMAC_C and %.2fx faster than FMAC_SSE (unaligned size).\n",
         total_time_sse_aligned_ms, total_time_c_ms / total_time_sse_aligned_ms,
         total_time_sse_unaligned_ms / total_time_sse_aligned_ms);
#endif
}

}  // namespace media
