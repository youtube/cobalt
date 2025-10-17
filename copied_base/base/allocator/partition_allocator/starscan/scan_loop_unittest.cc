// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/scan_loop.h"

#include "base/allocator/partition_allocator/partition_alloc_base/cpu.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "build/build_config.h"

#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc::internal {

namespace {

class TestScanLoop final : public ScanLoop<TestScanLoop> {
  friend class ScanLoop<TestScanLoop>;

 public:
  explicit TestScanLoop(SimdSupport ss) : ScanLoop(ss) {}

  size_t visited() const { return visited_; }

  void Reset() { visited_ = 0; }

 private:
  static constexpr uintptr_t kRegularPoolMask = 0xffffff0000000000;
  static constexpr uintptr_t kBasePtr = 0x0000560000000000;

  static uintptr_t RegularPoolBase() { return kBasePtr; }
  static uintptr_t RegularPoolMask() { return kRegularPoolMask; }

  void CheckPointer(uintptr_t maybe_ptr) { ++visited_; }

  size_t visited_ = 0;
};

static constexpr uintptr_t kValidPtr = 0x000056789abcdef0;
static constexpr uintptr_t kInvalidPtr = 0x0000aaaaaaaaaaaa;
static constexpr uintptr_t kZeroPtr = 0x0;

// Tests all possible compbinations of incoming args.
template <size_t Alignment, typename... Args>
void TestOnRangeWithAlignment(TestScanLoop& sl,
                              size_t expected_visited,
                              Args... args) {
  alignas(Alignment) uintptr_t range[] = {args...};
  std::sort(std::begin(range), std::end(range));
  do {
    sl.Run(reinterpret_cast<uintptr_t>(std::begin(range)),
           reinterpret_cast<uintptr_t>(std::end(range)));
    EXPECT_EQ(expected_visited, sl.visited());
    sl.Reset();
  } while (std::next_permutation(std::begin(range), std::end(range)));
}

}  // namespace

TEST(PartitionAllocScanLoopTest, UnvectorizedWithRegularPool) {
  {
    TestScanLoop sl(SimdSupport::kUnvectorized);
    TestOnRangeWithAlignment<8>(sl, 0u, kInvalidPtr, kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kUnvectorized);
    TestOnRangeWithAlignment<8>(sl, 1u, kValidPtr, kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kUnvectorized);
    TestOnRangeWithAlignment<8>(sl, 2u, kValidPtr, kValidPtr, kInvalidPtr);
  }
  {
    // Make sure zeros are skipped.
    TestScanLoop sl(SimdSupport::kUnvectorized);
    TestOnRangeWithAlignment<8>(sl, 1u, kValidPtr, kInvalidPtr, kZeroPtr);
  }
}

#if defined(ARCH_CPU_X86_64)
TEST(PartitionAllocScanLoopTest, VectorizedSSE4) {
  base::CPU cpu;
  if (!cpu.has_sse41())
    return;
  {
    TestScanLoop sl(SimdSupport::kSSE41);
    TestOnRangeWithAlignment<16>(sl, 0u, kInvalidPtr, kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kSSE41);
    TestOnRangeWithAlignment<16>(sl, 1u, kValidPtr, kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kSSE41);
    TestOnRangeWithAlignment<16>(sl, 2u, kValidPtr, kValidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kSSE41);
    TestOnRangeWithAlignment<16>(sl, 3u, kValidPtr, kValidPtr, kValidPtr);
  }
}

TEST(PartitionAllocScanLoopTest, VectorizedAVX2) {
  base::CPU cpu;
  if (!cpu.has_avx2())
    return;
  {
    TestScanLoop sl(SimdSupport::kAVX2);
    TestOnRangeWithAlignment<32>(sl, 0u, kInvalidPtr, kInvalidPtr, kInvalidPtr,
                                 kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kAVX2);
    TestOnRangeWithAlignment<32>(sl, 1u, kValidPtr, kInvalidPtr, kInvalidPtr,
                                 kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kAVX2);
    TestOnRangeWithAlignment<32>(sl, 2u, kValidPtr, kValidPtr, kInvalidPtr,
                                 kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kAVX2);
    TestOnRangeWithAlignment<32>(sl, 3u, kValidPtr, kValidPtr, kValidPtr,
                                 kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kAVX2);
    TestOnRangeWithAlignment<32>(sl, 4u, kValidPtr, kValidPtr, kValidPtr,
                                 kValidPtr, kInvalidPtr);
  }
  {
    // Check that the residual pointer is also visited.
    TestScanLoop sl(SimdSupport::kAVX2);
    TestOnRangeWithAlignment<32>(sl, 5u, kValidPtr, kValidPtr, kValidPtr,
                                 kValidPtr, kValidPtr);
  }
}
#endif  // defined(ARCH_CPU_X86_64)

#if PA_CONFIG(STARSCAN_NEON_SUPPORTED)
TEST(PartitionAllocScanLoopTest, VectorizedNEON) {
  {
    TestScanLoop sl(SimdSupport::kNEON);
    TestOnRangeWithAlignment<16>(sl, 0u, kInvalidPtr, kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kNEON);
    TestOnRangeWithAlignment<16>(sl, 1u, kValidPtr, kInvalidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kNEON);
    TestOnRangeWithAlignment<16>(sl, 2u, kValidPtr, kValidPtr, kInvalidPtr);
  }
  {
    TestScanLoop sl(SimdSupport::kNEON);
    TestOnRangeWithAlignment<16>(sl, 3u, kValidPtr, kValidPtr, kValidPtr);
  }
  {
    // Don't visit zeroes.
    TestScanLoop sl(SimdSupport::kNEON);
    TestOnRangeWithAlignment<16>(sl, 1u, kInvalidPtr, kValidPtr, kZeroPtr);
  }
}
#endif  // PA_CONFIG(STARSCAN_NEON_SUPPORTED)

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(HAS_64_BIT_POINTERS)
