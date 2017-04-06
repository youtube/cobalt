// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "testing/gtest/include/gtest/gtest.h"

// This must come after gtest, because it includes GL, which can include X11,
// which will define None to be 0L, which conflicts with gtest.
#include "starboard/decode_target.h"  // NOLINT(build/include_order)

namespace starboard {
namespace nplb {

#if SB_HAS(GRAPHICS)
#if SB_API_VERSION >= 3 && \
    SB_API_VERSION < SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

namespace {

struct ProviderContext {
  static SbDecodeTarget Acquire(void* raw_context,
                                SbDecodeTargetFormat format,
                                int width,
                                int height) {
    ProviderContext* context = reinterpret_cast<ProviderContext*>(raw_context);
    ++context->called_acquire;
    return kSbDecodeTargetInvalid;
  }

  static void Release(void* raw_context, SbDecodeTarget target) {
    ProviderContext* context = reinterpret_cast<ProviderContext*>(raw_context);
    ++context->called_release;
  }

  int called_acquire;
  int called_release;
};

void AcquireFalse(SbDecodeTargetProvider* provider) {
  if (provider) {
    SbDecodeTarget target = SbDecodeTargetAcquireFromProvider(
        provider, kSbDecodeTargetFormat1PlaneRGBA, 16, 16);
    bool valid = SbDecodeTargetIsValid(target);
    EXPECT_FALSE(valid);
    if (valid) {
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
      SbDecodeTargetRelease(target);
#else   // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
      SbDecodeTargetDestroy(target);
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
    }
  }
}

void ReleaseInvalid(SbDecodeTargetProvider* provider) {
  if (provider) {
    SbDecodeTargetReleaseToProvider(provider, kSbDecodeTargetInvalid);
  }
}

TEST(SbDecodeTargetProviderTest, SunnyDayCallsThroughToProvider) {
  ProviderContext context = {};
  SbDecodeTargetProvider provider = {&ProviderContext::Acquire,
                                     &ProviderContext::Release,
                                     reinterpret_cast<void*>(&context)};

  EXPECT_EQ(0, context.called_acquire);
  EXPECT_EQ(0, context.called_release);

  AcquireFalse(&provider);
  EXPECT_EQ(1, context.called_acquire);
  AcquireFalse(&provider);
  EXPECT_EQ(2, context.called_acquire);

  ReleaseInvalid(&provider);
  EXPECT_EQ(1, context.called_release);
  ReleaseInvalid(&provider);
  EXPECT_EQ(2, context.called_release);
}

TEST(SbDecodeTargetProviderTest, SunnyDayMultipleProviders) {
  ProviderContext context = {};
  SbDecodeTargetProvider provider = {&ProviderContext::Acquire,
                                     &ProviderContext::Release,
                                     reinterpret_cast<void*>(&context)};

  ProviderContext context2 = {};
  SbDecodeTargetProvider provider2 = {&ProviderContext::Acquire,
                                      &ProviderContext::Release,
                                      reinterpret_cast<void*>(&context2)};

  AcquireFalse(&provider2);
  EXPECT_EQ(1, context2.called_acquire);
  EXPECT_EQ(0, context.called_acquire);
  AcquireFalse(&provider2);
  EXPECT_EQ(2, context2.called_acquire);
  EXPECT_EQ(0, context.called_acquire);

  ReleaseInvalid(&provider2);
  EXPECT_EQ(1, context2.called_release);
  EXPECT_EQ(0, context.called_release);
  ReleaseInvalid(&provider2);
  EXPECT_EQ(2, context2.called_release);
  EXPECT_EQ(0, context.called_release);
}

TEST(SbDecodeTargetProviderTest, RainyDayAcquireNoProvider) {
  AcquireFalse(NULL);
}

TEST(SbDecodeTargetProviderTest, RainyDayReleaseNoProvider) {
  ReleaseInvalid(NULL);
}

TEST(SbDecodeTargetProviderTest, RainyDayMultipleProviders) {
  ProviderContext context = {};
  SbDecodeTargetProvider provider = {&ProviderContext::Acquire,
                                     &ProviderContext::Release,
                                     reinterpret_cast<void*>(&context)};

  ProviderContext context2 = {};
  SbDecodeTargetProvider provider2 = {&ProviderContext::Acquire,
                                      &ProviderContext::Release,
                                      reinterpret_cast<void*>(&context2)};

  AcquireFalse(&provider2);
  EXPECT_EQ(1, context2.called_acquire);
  AcquireFalse(&provider2);
  EXPECT_EQ(2, context2.called_acquire);

  ReleaseInvalid(&provider2);
  EXPECT_EQ(1, context2.called_release);
  ReleaseInvalid(&provider2);
  EXPECT_EQ(2, context2.called_release);

  AcquireFalse(&provider);
  EXPECT_EQ(2, context2.called_acquire);
  AcquireFalse(&provider);
  EXPECT_EQ(2, context2.called_acquire);

  ReleaseInvalid(&provider);
  EXPECT_EQ(2, context2.called_release);
  ReleaseInvalid(&provider);
  EXPECT_EQ(2, context2.called_release);
}

}  // namespace

#endif  // SB_API_VERSION >= 3 && \
        // SB_API_VERSION < SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
#endif  // SB_HAS(GRAPHICS)

}  // namespace nplb
}  // namespace starboard
