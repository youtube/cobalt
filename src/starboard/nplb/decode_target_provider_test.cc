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

#if SB_VERSION(3) && SB_HAS(GRAPHICS)

namespace starboard {
namespace nplb {
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

template <typename T>
SbDecodeTargetProvider MakeProvider() {
  return {&T::Acquire, &T::Release};
}

void AcquireFalse() {
  SbDecodeTarget target = SbDecodeTargetAcquireFromProvider(
      kSbDecodeTargetFormat1PlaneRGBA, 16, 16);
  bool valid = SbDecodeTargetIsValid(target);
  EXPECT_FALSE(valid);
  if (valid) {
    SbDecodeTargetDestroy(target);
  }
}

void ReleaseInvalid() {
  SbDecodeTargetReleaseToProvider(kSbDecodeTargetInvalid);
}

TEST(SbDecodeTargetProviderTest, SunnyDayRegisterUnregister) {
  ProviderContext context = {};
  SbDecodeTargetProvider provider = MakeProvider<ProviderContext>();
  ASSERT_TRUE(SbDecodeTargetRegisterProvider(&provider, &context));
  SbDecodeTargetUnregisterProvider(&provider, &context);
}

TEST(SbDecodeTargetProviderTest, SunnyDayCallsThroughToProvider) {
  ProviderContext context = {};
  SbDecodeTargetProvider provider = MakeProvider<ProviderContext>();
  ASSERT_TRUE(SbDecodeTargetRegisterProvider(&provider, &context));
  EXPECT_EQ(0, context.called_acquire);
  EXPECT_EQ(0, context.called_release);

  AcquireFalse();
  EXPECT_EQ(1, context.called_acquire);
  AcquireFalse();
  EXPECT_EQ(2, context.called_acquire);

  ReleaseInvalid();
  EXPECT_EQ(1, context.called_release);
  ReleaseInvalid();
  EXPECT_EQ(2, context.called_release);

  SbDecodeTargetUnregisterProvider(&provider, &context);
}

TEST(SbDecodeTargetProviderTest, SunnyDayRegisterOver) {
  ProviderContext context = {};
  SbDecodeTargetProvider provider = MakeProvider<ProviderContext>();
  ASSERT_TRUE(SbDecodeTargetRegisterProvider(&provider, &context));

  ProviderContext context2 = {};
  SbDecodeTargetProvider provider2 = MakeProvider<ProviderContext>();
  ASSERT_TRUE(SbDecodeTargetRegisterProvider(&provider2, &context2));

  AcquireFalse();
  EXPECT_EQ(1, context2.called_acquire);
  EXPECT_EQ(0, context.called_acquire);
  AcquireFalse();
  EXPECT_EQ(2, context2.called_acquire);
  EXPECT_EQ(0, context.called_acquire);

  ReleaseInvalid();
  EXPECT_EQ(1, context2.called_release);
  EXPECT_EQ(0, context.called_release);
  ReleaseInvalid();
  EXPECT_EQ(2, context2.called_release);
  EXPECT_EQ(0, context.called_release);

  SbDecodeTargetUnregisterProvider(&provider2, &context2);
}

TEST(SbDecodeTargetProviderTest, RainyDayAcquireNoProvider) {
  AcquireFalse();
}

TEST(SbDecodeTargetProviderTest, RainyDayReleaseNoProvider) {
  ReleaseInvalid();
}

TEST(SbDecodeTargetProviderTest, RainyDayUnregisterUnregistered) {
  ProviderContext context = {};
  SbDecodeTargetProvider provider = MakeProvider<ProviderContext>();
  SbDecodeTargetUnregisterProvider(&provider, &context);

  ProviderContext context2 = {};
  SbDecodeTargetProvider provider2 = MakeProvider<ProviderContext>();
  ASSERT_TRUE(SbDecodeTargetRegisterProvider(&provider2, &context2));
  SbDecodeTargetUnregisterProvider(&provider, &context);
  SbDecodeTargetUnregisterProvider(&provider, &context2);
  SbDecodeTargetUnregisterProvider(&provider2, &context);

  AcquireFalse();
  EXPECT_EQ(1, context2.called_acquire);
  AcquireFalse();
  EXPECT_EQ(2, context2.called_acquire);

  ReleaseInvalid();
  EXPECT_EQ(1, context2.called_release);
  ReleaseInvalid();
  EXPECT_EQ(2, context2.called_release);

  SbDecodeTargetUnregisterProvider(&provider2, &context2);

  AcquireFalse();
  EXPECT_EQ(2, context2.called_acquire);
  AcquireFalse();
  EXPECT_EQ(2, context2.called_acquire);

  ReleaseInvalid();
  EXPECT_EQ(2, context2.called_release);
  ReleaseInvalid();
  EXPECT_EQ(2, context2.called_release);
}

TEST(SbDecodeTargetProviderTest, RainyDayUnregisterNull) {
  ProviderContext context = {};
  SbDecodeTargetUnregisterProvider(NULL, &context);
}

TEST(SbDecodeTargetProviderTest, RainyDayUnregisterNullNull) {
  SbDecodeTargetUnregisterProvider(NULL, NULL);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_VERSION(3) && SB_HAS(GRAPHICS)
