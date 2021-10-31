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

#include "starboard/media.h"
#include "starboard/nplb/performance_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
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
};

constexpr int kBitsPerPixelValues[] = {
    kSbMediaBitsPerPixelInvalid, 8, 10, 12,
};

constexpr SbMediaVideoCodec kVideoCodecs[] = {
    kSbMediaVideoCodecNone,

    kSbMediaVideoCodecH264,   kSbMediaVideoCodecH265, kSbMediaVideoCodecMpeg2,
    kSbMediaVideoCodecTheora, kSbMediaVideoCodecVc1,
    kSbMediaVideoCodecAv1,
    kSbMediaVideoCodecVp8,    kSbMediaVideoCodecVp9,
};

constexpr SbMediaType kMediaTypes[] = {
    kSbMediaTypeAudio, kSbMediaTypeVideo,
};

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
  for (auto type : kMediaTypes) {
    int alignment = SbMediaGetBufferAlignment(type);
    // TODO: This currently accepts 0 or a power of 2. We should disallow 0 here
    // when we change the default value to be 1 instead of 0.
    EXPECT_GE(alignment, 0);
    EXPECT_EQ(alignment & (alignment - 1), 0)
        << "Alignment must always be a power of 2";
  }
}

TEST(SbMediaBufferTest, AllocationUnit) {
  // TODO: impose more bounds.
  EXPECT_GE(SbMediaGetBufferAllocationUnit(), 0);
}

TEST(SbMediaBufferTest, AudioBudget) {
  // TODO: refine this lower bound.
  const int kMinAudioBudget = 1 * 1024 * 1024;
  EXPECT_GT(SbMediaGetAudioBufferBudget(), kMinAudioBudget);
}

TEST(SbMediaBufferTest, GarbageCollectionDurationThreshold) {
  // TODO: impose reasonable bounds here.
  int kMinGarbageCollectionDurationThreshold = 10 * kSbTimeSecond;
  int kMaxGarbageCollectionDurationThreshold = 240 * kSbTimeSecond;
  int threshold = SbMediaGetBufferGarbageCollectionDurationThreshold();
  EXPECT_GE(threshold, kMinGarbageCollectionDurationThreshold);
  EXPECT_LE(threshold, kMaxGarbageCollectionDurationThreshold);
}

TEST(SbMediaBufferTest, InitialCapacity) {
  EXPECT_GE(SbMediaGetInitialBufferCapacity(), 0);
}

TEST(SbMediaBufferTest, MaxCapacity) {
  // TODO: set a reasonable upper bound.
  for (auto resolution : kVideoResolutions) {
    for (auto bits_per_pixel : kBitsPerPixelValues) {
      for (auto codec : kVideoCodecs) {
        EXPECT_GT(SbMediaGetMaxBufferCapacity(codec, resolution[0],
                                              resolution[1], bits_per_pixel),
                  0);
        EXPECT_GE(SbMediaGetMaxBufferCapacity(codec, resolution[0],
                                              resolution[1], bits_per_pixel),
                  SbMediaGetInitialBufferCapacity());
      }
    }
  }
}

TEST(SbMediaBufferTest, Padding) {
  for (auto type : kMediaTypes) {
    EXPECT_GE(SbMediaGetBufferPadding(type), 0);
  }
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

TEST(SbMediaBufferTest, StorageType) {
  // Just don't crash.
  SbMediaBufferStorageType type = SbMediaGetBufferStorageType();
  switch (type) {
    case kSbMediaBufferStorageTypeMemory:
    case kSbMediaBufferStorageTypeFile:
      return;
  }
  SB_NOTREACHED();
}

TEST(SbMediaBufferTest, UsingMemoryPool) {
  // Just don't crash.
  SbMediaIsBufferUsingMemoryPool();
}

TEST(SbMediaBufferTest, VideoBudget) {
  // TODO: refine this lower bound.
  const int kMinVideoBudget = 1 * 1024 * 1024;
  for (auto codec : kVideoCodecs) {
    for (auto resolution : kVideoResolutions) {
      for (auto bits_per_pixel : kBitsPerPixelValues) {
        EXPECT_GT(SbMediaGetVideoBufferBudget(codec, resolution[0],
                                              resolution[1], bits_per_pixel),
                  kMinVideoBudget);
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
  TEST_PERF_FUNCNOARGS_DEFAULT(SbMediaGetBufferStorageType);
  TEST_PERF_FUNCNOARGS_DEFAULT(SbMediaIsBufferUsingMemoryPool);

  for (auto type : kMediaTypes) {
    TEST_PERF_FUNCWITHARGS_DEFAULT(SbMediaGetBufferAlignment, type);
    TEST_PERF_FUNCWITHARGS_DEFAULT(SbMediaGetBufferPadding, type);
  }

  for (auto resolution : kVideoResolutions) {
    for (auto bits_per_pixel : kBitsPerPixelValues) {
      for (auto codec : kVideoCodecs) {
        TEST_PERF_FUNCWITHARGS_DEFAULT(SbMediaGetMaxBufferCapacity, codec,
                                       resolution[0], resolution[1],
                                       bits_per_pixel);
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
}  // namespace starboard
