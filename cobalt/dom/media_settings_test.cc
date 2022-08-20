// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/media_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

TEST(MediaSettingsImplTest, Empty) {
  MediaSettingsImpl impl;

  EXPECT_FALSE(impl.GetSourceBufferEvictExtraInBytes());
  EXPECT_FALSE(impl.GetMinimumProcessorCountToOffloadAlgorithm());
  EXPECT_FALSE(impl.IsAsynchronousReductionEnabled());
  EXPECT_FALSE(impl.IsAvoidCopyingArrayBufferEnabled());
  EXPECT_FALSE(impl.GetMaxSizeForImmediateJob());
  EXPECT_FALSE(impl.GetMaxSourceBufferAppendSizeInBytes());
  EXPECT_FALSE(impl.GetMediaElementTimeupdateEventIntervalInMilliseconds());
}

TEST(MediaSettingsImplTest, SunnyDay) {
  MediaSettingsImpl impl;

  ASSERT_TRUE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", 100));
  ASSERT_TRUE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", 101));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAsynchronousReduction", 1));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAvoidCopyingArrayBuffer", 1));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSizeForImmediateJob", 103));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSourceBufferAppendSizeInBytes", 100000));
  ASSERT_TRUE(
      impl.Set("MediaElement.TimeupdateEventIntervalInMilliseconds", 100001));

  EXPECT_EQ(impl.GetSourceBufferEvictExtraInBytes().value(), 100);
  EXPECT_EQ(impl.GetMinimumProcessorCountToOffloadAlgorithm().value(), 101);
  EXPECT_TRUE(impl.IsAsynchronousReductionEnabled().value());
  EXPECT_TRUE(impl.IsAvoidCopyingArrayBufferEnabled().value());
  EXPECT_EQ(impl.GetMaxSizeForImmediateJob().value(), 103);
  EXPECT_EQ(impl.GetMaxSourceBufferAppendSizeInBytes().value(), 100000);
  EXPECT_EQ(impl.GetMediaElementTimeupdateEventIntervalInMilliseconds().value(),
            100001);
}

TEST(MediaSettingsImplTest, RainyDay) {
  MediaSettingsImpl impl;

  ASSERT_FALSE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", -100));
  ASSERT_FALSE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", -101));
  ASSERT_FALSE(impl.Set("MediaSource.EnableAsynchronousReduction", 2));
  ASSERT_FALSE(impl.Set("MediaSource.EnableAvoidCopyingArrayBuffer", 2));
  ASSERT_FALSE(impl.Set("MediaSource.MaxSizeForImmediateJob", -103));
  ASSERT_FALSE(impl.Set("MediaSource.MaxSourceBufferAppendSizeInBytes", 0));
  ASSERT_FALSE(
      impl.Set("MediaElement.TimeupdateEventIntervalInMilliseconds", 0));

  EXPECT_FALSE(impl.GetSourceBufferEvictExtraInBytes());
  EXPECT_FALSE(impl.GetMinimumProcessorCountToOffloadAlgorithm());
  EXPECT_FALSE(impl.IsAsynchronousReductionEnabled());
  EXPECT_FALSE(impl.IsAvoidCopyingArrayBufferEnabled());
  EXPECT_FALSE(impl.GetMaxSizeForImmediateJob());
  EXPECT_FALSE(impl.GetMaxSourceBufferAppendSizeInBytes());
  EXPECT_FALSE(impl.GetMediaElementTimeupdateEventIntervalInMilliseconds());
}

TEST(MediaSettingsImplTest, ZeroValuesWork) {
  MediaSettingsImpl impl;

  ASSERT_TRUE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", 0));
  ASSERT_TRUE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", 0));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAsynchronousReduction", 0));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAvoidCopyingArrayBuffer", 0));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSizeForImmediateJob", 0));
  // O is an invalid value for "MediaSource.MaxSourceBufferAppendSizeInBytes".
  // O is an invalid value for
  // "MediaElement.TimeupdateEventIntervalInMilliseconds".

  EXPECT_EQ(impl.GetSourceBufferEvictExtraInBytes().value(), 0);
  EXPECT_EQ(impl.GetMinimumProcessorCountToOffloadAlgorithm().value(), 0);
  EXPECT_FALSE(impl.IsAsynchronousReductionEnabled().value());
  EXPECT_FALSE(impl.IsAvoidCopyingArrayBufferEnabled().value());
  EXPECT_EQ(impl.GetMaxSizeForImmediateJob().value(), 0);
}

TEST(MediaSettingsImplTest, Updatable) {
  MediaSettingsImpl impl;

  ASSERT_TRUE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", 0));
  ASSERT_TRUE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", 0));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAsynchronousReduction", 0));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAvoidCopyingArrayBuffer", 0));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSizeForImmediateJob", 0));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSourceBufferAppendSizeInBytes", 1));
  ASSERT_TRUE(
      impl.Set("MediaElement.TimeupdateEventIntervalInMilliseconds", 1));

  ASSERT_TRUE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", 1));
  ASSERT_TRUE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", 1));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAsynchronousReduction", 1));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAvoidCopyingArrayBuffer", 1));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSizeForImmediateJob", 1));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSourceBufferAppendSizeInBytes", 2));
  ASSERT_TRUE(
      impl.Set("MediaElement.TimeupdateEventIntervalInMilliseconds", 2));

  EXPECT_EQ(impl.GetSourceBufferEvictExtraInBytes().value(), 1);
  EXPECT_EQ(impl.GetMinimumProcessorCountToOffloadAlgorithm().value(), 1);
  EXPECT_TRUE(impl.IsAsynchronousReductionEnabled().value());
  EXPECT_TRUE(impl.IsAvoidCopyingArrayBufferEnabled().value());
  EXPECT_EQ(impl.GetMaxSizeForImmediateJob().value(), 1);
  EXPECT_EQ(impl.GetMaxSourceBufferAppendSizeInBytes().value(), 2);
  EXPECT_EQ(impl.GetMediaElementTimeupdateEventIntervalInMilliseconds().value(),
            2);
}

TEST(MediaSettingsImplTest, InvalidSettingNames) {
  MediaSettingsImpl impl;

  ASSERT_FALSE(impl.Set("MediaSource.Invalid", 0));
  ASSERT_FALSE(impl.Set("MediaElement.Invalid", 1));
  ASSERT_FALSE(impl.Set("Invalid.SourceBufferEvictExtraInBytes", 0));
  ASSERT_FALSE(impl.Set("Invalid.TimeupdateEventIntervalInMilliseconds", 1));
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
