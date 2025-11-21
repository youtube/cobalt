// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>

#include <cmath>
#include <random>
#include <vector>

#include "starboard/media.h"
#include "starboard/nplb/performance_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

constexpr int kVideoResolutions[][2] = {
    {kSbMediaVideoResolutionDimensionInvalid,
     kSbMediaVideoResolutionDimensionInvalid},
    {640, 480},
    {1280, 720},
    {1920, 1080},
    {2560, 1440},
    {3840, 2160},
    {7680, 4320},
};

constexpr int kBitsPerPixelValues[] = {
    kSbMediaBitsPerPixelInvalid,
    8,
    10,
    12,
};

constexpr SbMediaVideoCodec kVideoCodecs[] = {
    kSbMediaVideoCodecNone,

    kSbMediaVideoCodecH264,   kSbMediaVideoCodecH265, kSbMediaVideoCodecMpeg2,
    kSbMediaVideoCodecTheora, kSbMediaVideoCodecVc1,  kSbMediaVideoCodecAv1,
    kSbMediaVideoCodecVp8,    kSbMediaVideoCodecVp9,
};

constexpr SbMediaType kMediaTypes[] = {
    kSbMediaTypeAudio,
    kSbMediaTypeVideo,
};

// Minimum audio and video budgets required by the 2024 Youtube Hardware
// Requirements.
constexpr int kMinAudioBudget = 5 * 1024 * 1024;
constexpr int kMinVideoBudget1080p = 30 * 1024 * 1024;
constexpr int kMinVideoBudget4kSdr = 50 * 1024 * 1024;
constexpr int kMinVideoBudget4kHdr = 80 * 1024 * 1024;
constexpr int kMinVideoBudget8k = 200 * 1024 * 1024;

int GetVideoBufferBudget(SbMediaVideoCodec codec,
                         int frame_width,
                         bool is_hdr) {
  if (codec != kSbMediaVideoCodecAv1 && codec != kSbMediaVideoCodecH264 &&
      codec != kSbMediaVideoCodecVp9) {
    return kMinVideoBudget1080p;
  }

  static const bool kSupports8k =
      SbMediaCanPlayMimeAndKeySystem(
          "video/mp4; codecs=\"av01.0.16M.08\"; width=7680; height=4320", "") ==
      kSbMediaSupportTypeProbably;
  static const bool kSupports4kHdr =
      SbMediaCanPlayMimeAndKeySystem(
          "video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=3840; "
          "height=2160",
          "") == kSbMediaSupportTypeProbably;
  static const bool kSupports4kSdr =
      SbMediaCanPlayMimeAndKeySystem(
          "video/webm; codecs=\"vp9\"; width=3840; height=2160", "") ==
      kSbMediaSupportTypeProbably;

  int video_budget = kMinVideoBudget1080p;
  if (kSupports8k && frame_width > 3840) {
    video_budget = kMinVideoBudget8k;
  } else if (frame_width > 1920 && frame_width <= 3840) {
    if (kSupports4kHdr && is_hdr) {
      video_budget = kMinVideoBudget4kHdr;
    } else if (kSupports4kSdr && !is_hdr) {
      video_budget = kMinVideoBudget4kSdr;
    }
  }

  return video_budget;
}

std::vector<void*> TryToAllocateMemory(int size,
                                       int allocation_unit,
                                       int alignment) {
  int total_allocated = 0;
  std::vector<void*> allocated_ptrs;
  if (allocation_unit != 0) {
    allocated_ptrs.reserve(std::ceil(size / allocation_unit));
  }
  while (total_allocated < size) {
    // When |allocation_unit| == 0, randomly allocate a size between 100k -
    // 500k.
    int allocation_increment = allocation_unit != 0
                                   ? allocation_unit
                                   : (std::rand() % 500 + 100) * 1024;
    void* allocated_memory = nullptr;
    std::ignore =
        posix_memalign(&allocated_memory, alignment, allocation_increment);
    EXPECT_NE(allocated_memory, nullptr);
    if (!allocated_memory) {
      return allocated_ptrs;
    }
    allocated_ptrs.push_back(allocated_memory);
    total_allocated += allocation_increment;
  }
  return allocated_ptrs;
}

}  // namespace

TEST(SbMediaBufferTest, VideoCodecs) {
  // Perform a check to determine if new codecs have been added to the
  // enum, but not the array. If the compiler warns about a missing case here,
  // the value must be added to the array and the switch case.
  SbMediaVideoCodec codec = kVideoCodecs[0];
  switch (codec) {
    case kVideoCodecs[0]:
    case kVideoCodecs[1]:
    case kVideoCodecs[2]:
    case kVideoCodecs[3]:
    case kVideoCodecs[4]:
    case kVideoCodecs[5]:
    case kVideoCodecs[6]:
    case kVideoCodecs[7]:
    case kVideoCodecs[8]:
      break;
  }
}

TEST(SbMediaBufferTest, MediaTypes) {
  // Perform a check to determine if new types have been added to the
  // enum, but not the array. If the compiler warns about a missing case here,
  // the value must be added to the array and the switch case.
  SbMediaType type = kMediaTypes[0];
  switch (type) {
    case kMediaTypes[0]:
    case kMediaTypes[1]:
      break;
  }
}

TEST(SbMediaBufferTest, Alignment) {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kMediaTypes); ++i) {
    // The test will be run more than once, it's redundant but allows us to keep
    // the test logic in one place.
    int alignment = SbMediaGetBufferAlignment();

    // SbMediaGetBufferAlignment() was deprecated in Starboard 16, its return
    // value is no longer used when allocating media buffers.  This is verified
    // explicitly here by ensuring its return value is sizeof(void*).
    // The app MAY take best effort to allocate media buffers aligned to an
    // optimal alignment for the platform, but not guaranteed.
    // An implementation that has specific alignment requirement should check
    // the alignment of the incoming buffer, and make a copy when necessary.
    EXPECT_EQ(static_cast<size_t>(alignment), sizeof(void*));
  }
}

TEST(SbMediaBufferTest, AllocationUnit) {
  EXPECT_GE(SbMediaGetBufferAllocationUnit(), 0);

  if (SbMediaGetBufferAllocationUnit() != 0) {
    EXPECT_GE(SbMediaGetBufferAllocationUnit(), 64 * 1024);
  }

  int allocation_unit = SbMediaGetBufferAllocationUnit();
  std::vector<void*> allocated_ptrs;
  int initial_buffer_capacity = SbMediaGetInitialBufferCapacity();
  if (initial_buffer_capacity > 0) {
    allocated_ptrs = TryToAllocateMemory(initial_buffer_capacity,
                                         allocation_unit, sizeof(void*));
  }

  for (void* ptr : allocated_ptrs) {
    free(ptr);
  }
}

TEST(SbMediaBufferTest, AudioBudget) {
  EXPECT_GE(SbMediaGetAudioBufferBudget(), kMinAudioBudget);
}

TEST(SbMediaBufferTest, GarbageCollectionDurationThreshold) {
  int kMinGarbageCollectionDurationThreshold = 30'000'000LL;   // 30 seconds
  int kMaxGarbageCollectionDurationThreshold = 240'000'000LL;  // 240 seconds
  int threshold = SbMediaGetBufferGarbageCollectionDurationThreshold();
  EXPECT_GE(threshold, kMinGarbageCollectionDurationThreshold);
  EXPECT_LE(threshold, kMaxGarbageCollectionDurationThreshold);
}

TEST(SbMediaBufferTest, InitialCapacity) {
  EXPECT_GE(SbMediaGetInitialBufferCapacity(), 0);
}

TEST(SbMediaBufferTest, Padding) {
  // SbMediaGetBufferPadding() was deprecated in Starboard 16, its return value
  // is no longer used when allocating media buffers.  This is verified
  // explicitly here by ensuring its return value is 0.
  // An implementation that has specific padding requirement should make a
  // copy of the incoming buffer when necessary.
  EXPECT_EQ(SbMediaGetBufferPadding(), 0);
}

TEST(SbMediaBufferTest, PoolAllocateOnDemand) {
  // Just don't crash.
  SbMediaIsBufferPoolAllocateOnDemand();
}

TEST(SbMediaBufferTest, ProgressiveBudget) {
  const int kMinProgressiveBudget = 8 * 1024 * 1024;
  int audio_budget = SbMediaGetAudioBufferBudget();
  for (auto video_codec : kVideoCodecs) {
    for (auto bits_per_pixel : kBitsPerPixelValues) {
      int video_budget_1080p =
          SbMediaGetVideoBufferBudget(video_codec, 1920, 1080, bits_per_pixel);
      for (auto resolution : kVideoResolutions) {
        int progressive_budget = SbMediaGetProgressiveBufferBudget(
            video_codec, resolution[0], resolution[1], bits_per_pixel);
        EXPECT_LT(progressive_budget, video_budget_1080p + audio_budget)
            << "Progressive budget must be less than sum of 1080p video "
               "budget and audio budget";
        EXPECT_GE(progressive_budget, kMinProgressiveBudget);
      }
    }
  }
}

TEST(SbMediaBufferTest, UsingMemoryPool) {
  EXPECT_TRUE(SbMediaIsBufferUsingMemoryPool())
      << "This function is deprecated. Media buffer pools are always "
      << "used in Starboard 16 and newer. Please see starboard/CHANGELOG.md";
}

TEST(SbMediaBufferTest, VideoBudget) {
  for (auto codec : kVideoCodecs) {
    for (auto resolution : kVideoResolutions) {
      for (auto bits_per_pixel : kBitsPerPixelValues) {
        int video_budget =
            GetVideoBufferBudget(codec, resolution[0], bits_per_pixel > 8);
        EXPECT_GE(SbMediaGetVideoBufferBudget(codec, resolution[0],
                                              resolution[1], bits_per_pixel),
                  video_budget);
      }
    }
  }
}

TEST(SbMediaBufferTest, ValidatePerformance) {
  TEST_PERF_FUNCNOARGS_DEFAULT(SbMediaGetBufferAllocationUnit);
  TEST_PERF_FUNCNOARGS_DEFAULT(SbMediaGetAudioBufferBudget);
  TEST_PERF_FUNCNOARGS_DEFAULT(
      SbMediaGetBufferGarbageCollectionDurationThreshold);
  TEST_PERF_FUNCNOARGS_DEFAULT(SbMediaGetInitialBufferCapacity);
  TEST_PERF_FUNCNOARGS_DEFAULT(SbMediaIsBufferPoolAllocateOnDemand);
  TEST_PERF_FUNCNOARGS_DEFAULT(SbMediaIsBufferUsingMemoryPool);

  for (auto resolution : kVideoResolutions) {
    for (auto bits_per_pixel : kBitsPerPixelValues) {
      for (auto codec : kVideoCodecs) {
        TEST_PERF_FUNCWITHARGS_DEFAULT(SbMediaGetProgressiveBufferBudget, codec,
                                       resolution[0], resolution[1],
                                       bits_per_pixel);
        TEST_PERF_FUNCWITHARGS_DEFAULT(SbMediaGetVideoBufferBudget, codec,
                                       resolution[0], resolution[1],
                                       bits_per_pixel);
      }
    }
  }
}

}  // namespace nplb
