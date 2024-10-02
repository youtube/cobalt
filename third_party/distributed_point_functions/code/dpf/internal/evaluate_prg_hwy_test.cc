// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dpf/internal/evaluate_prg_hwy.h"

#include "absl/numeric/int128.h"
#include "dpf/aes_128_fixed_key_hash.h"
#include "dpf/internal/status_matchers.h"
#include "gtest/gtest.h"
#include "hwy/aligned_allocator.h"

// clang-format off
#define HWY_IS_TEST 1;
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "dpf/internal/evaluate_prg_hwy_test.cc"  // NOLINT
#include "hwy/foreach_target.h"
// clang-format on
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace distributed_point_functions {
namespace dpf_internal {
namespace HWY_NAMESPACE {

constexpr absl::uint128 kKey0 =
    absl::MakeUint128(0x0000000000000000, 0x0000000000000000);
constexpr absl::uint128 kKey1 =
    absl::MakeUint128(0x1111111111111111, 0x1111111111111111);

void TestOutputMatchesNoHwyVersion(int num_seeds, int num_levels) {
  // Generate seeds.
  hwy::AlignedFreeUniquePtr<absl::uint128[]> seeds_in, paths;
  hwy::AlignedFreeUniquePtr<bool[]> control_bits_in;
  if (num_seeds > 0) {
    seeds_in = hwy::AllocateAligned<absl::uint128>(num_seeds);
    ASSERT_NE(seeds_in, nullptr);
    paths = hwy::AllocateAligned<absl::uint128>(num_seeds);
    ASSERT_NE(paths, nullptr);
    control_bits_in = hwy::AllocateAligned<bool>(num_seeds);
    ASSERT_NE(control_bits_in, nullptr);
  }
  for (int i = 0; i < num_seeds; ++i) {
    // All of these are arbitrary.
    seeds_in[i] = absl::MakeUint128(i, i + 1);
    paths[i] = absl::MakeUint128(23 * i + 42, 42 * i + 23);
    control_bits_in[i] = (i % 7 == 0);
  }
  hwy::AlignedFreeUniquePtr<absl::uint128[]> seeds_out;
  hwy::AlignedFreeUniquePtr<bool[]> control_bits_out;
  if (num_seeds > 0) {
    seeds_out = hwy::AllocateAligned<absl::uint128>(num_seeds);
    ASSERT_NE(seeds_out, nullptr);
    control_bits_out = hwy::AllocateAligned<bool>(num_seeds);
    ASSERT_NE(control_bits_out, nullptr);
  }

  // Generate correction words.
  hwy::AlignedFreeUniquePtr<absl::uint128[]> correction_seeds;
  hwy::AlignedFreeUniquePtr<bool[]> correction_controls_left,
      correction_controls_right;
  if (num_levels > 0) {
    correction_seeds = hwy::AllocateAligned<absl::uint128>(num_levels);
    ASSERT_NE(correction_seeds, nullptr);
    correction_controls_left = hwy::AllocateAligned<bool>(num_levels);
    ASSERT_NE(correction_controls_left, nullptr);
    correction_controls_right = hwy::AllocateAligned<bool>(num_levels);
    ASSERT_NE(correction_controls_right, nullptr);
  }
  for (int i = 0; i < num_levels; ++i) {
    correction_seeds[i] = absl::MakeUint128(i + 1, i);
    correction_controls_left[i] = (i % 23 == 0);
    correction_controls_right[i] = (i % 42 != 0);
  }

  // Set up PRGs.
  DPF_ASSERT_OK_AND_ASSIGN(
      auto prg_left,
      distributed_point_functions::Aes128FixedKeyHash::Create(kKey0));
  DPF_ASSERT_OK_AND_ASSIGN(
      auto prg_right,
      distributed_point_functions::Aes128FixedKeyHash::Create(kKey1));

  // Evaluate with Highway enabled.
  DPF_ASSERT_OK(EvaluateSeeds(
      num_seeds, num_levels, seeds_in.get(), control_bits_in.get(), paths.get(),
      correction_seeds.get(), correction_controls_left.get(),
      correction_controls_right.get(), prg_left, prg_right, seeds_out.get(),
      control_bits_out.get()));

  // Evaluate without highway.
  hwy::AlignedFreeUniquePtr<absl::uint128[]> seeds_out_wanted;
  hwy::AlignedFreeUniquePtr<bool[]> control_bits_out_wanted;
  if (num_seeds > 0) {
    seeds_out_wanted = hwy::AllocateAligned<absl::uint128>(num_seeds);
    ASSERT_NE(seeds_out_wanted, nullptr);
    control_bits_out_wanted = hwy::AllocateAligned<bool>(num_seeds);
    ASSERT_NE(control_bits_out_wanted, nullptr);
  }
  DPF_ASSERT_OK(EvaluateSeedsNoHwy(
      num_seeds, num_levels, seeds_in.get(), control_bits_in.get(), paths.get(),
      correction_seeds.get(), correction_controls_left.get(),
      correction_controls_right.get(), prg_left, prg_right,
      seeds_out_wanted.get(), control_bits_out_wanted.get()));

  // Check that both evaluations are equal, if there was anything to evaluate.
  if (num_levels > 0) {
    for (int i = 0; i < num_seeds; ++i) {
      EXPECT_EQ(seeds_out[i], seeds_out_wanted[i]);
      EXPECT_EQ(control_bits_out[i], control_bits_out_wanted[i]);
    }
  }
}

void TestAll() {
  for (int num_seeds : {0, 1, 2, 101, 128, 1000}) {
    for (int num_levels : {0, 1, 2, 32, 63, 64, 127}) {
      TestOutputMatchesNoHwyVersion(num_seeds, num_levels);
    }
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace dpf_internal
}  // namespace distributed_point_functions
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace distributed_point_functions {
namespace dpf_internal {
HWY_BEFORE_TEST(EvaluatePrgHwyTest);
HWY_EXPORT_AND_TEST_P(EvaluatePrgHwyTest, TestAll);
}  // namespace dpf_internal
}  // namespace distributed_point_functions

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif