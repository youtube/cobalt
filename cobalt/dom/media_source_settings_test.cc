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

#include "cobalt/dom/media_source_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {
namespace {

TEST(MediaSourceSettingsImplTest, Empty) {
  MediaSourceSettingsImpl impl;

  EXPECT_FALSE(impl.GetSourceBufferEvictExtraInBytes());
  EXPECT_FALSE(impl.GetMinimumProcessorCountToOffloadAlgorithm());
  EXPECT_FALSE(impl.IsAsynchronousReductionEnabled());
  EXPECT_FALSE(impl.GetMaxSizeForImmediateJob());
  EXPECT_FALSE(impl.GetMaxSourceBufferAppendSizeInBytes());
}

TEST(MediaSourceSettingsImplTest, SunnyDay) {
  MediaSourceSettingsImpl impl;

  ASSERT_TRUE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", 100));
  ASSERT_TRUE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", 101));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAsynchronousReduction", 1));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSizeForImmediateJob", 103));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSourceBufferAppendSizeInBytes", 100000));

  EXPECT_EQ(impl.GetSourceBufferEvictExtraInBytes().value(), 100);
  EXPECT_EQ(impl.GetMinimumProcessorCountToOffloadAlgorithm().value(), 101);
  EXPECT_TRUE(impl.IsAsynchronousReductionEnabled().value());
  EXPECT_EQ(impl.GetMaxSizeForImmediateJob().value(), 103);
  EXPECT_EQ(impl.GetMaxSourceBufferAppendSizeInBytes().value(), 100000);
}

TEST(MediaSourceSettingsImplTest, RainyDay) {
  MediaSourceSettingsImpl impl;

  ASSERT_FALSE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", -100));
  ASSERT_FALSE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", -101));
  ASSERT_FALSE(impl.Set("MediaSource.EnableAsynchronousReduction", 2));
  ASSERT_FALSE(impl.Set("MediaSource.MaxSizeForImmediateJob", -103));
  ASSERT_FALSE(impl.Set("MediaSource.MaxSourceBufferAppendSizeInBytes", 0));

  EXPECT_FALSE(impl.GetSourceBufferEvictExtraInBytes());
  EXPECT_FALSE(impl.GetMinimumProcessorCountToOffloadAlgorithm());
  EXPECT_FALSE(impl.IsAsynchronousReductionEnabled());
  EXPECT_FALSE(impl.GetMaxSizeForImmediateJob());
  EXPECT_FALSE(impl.GetMaxSourceBufferAppendSizeInBytes());
}

TEST(MediaSourceSettingsImplTest, ZeroValuesWork) {
  MediaSourceSettingsImpl impl;

  ASSERT_TRUE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", 0));
  ASSERT_TRUE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", 0));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAsynchronousReduction", 0));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSizeForImmediateJob", 0));
  // O is an invalid value for "MediaSource.MaxSourceBufferAppendSizeInBytes".

  EXPECT_EQ(impl.GetSourceBufferEvictExtraInBytes().value(), 0);
  EXPECT_EQ(impl.GetMinimumProcessorCountToOffloadAlgorithm().value(), 0);
  EXPECT_FALSE(impl.IsAsynchronousReductionEnabled().value());
  EXPECT_EQ(impl.GetMaxSizeForImmediateJob().value(), 0);
}

TEST(MediaSourceSettingsImplTest, Updatable) {
  MediaSourceSettingsImpl impl;

  ASSERT_TRUE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", 0));
  ASSERT_TRUE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", 0));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAsynchronousReduction", 0));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSizeForImmediateJob", 0));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSourceBufferAppendSizeInBytes", 1));

  ASSERT_TRUE(impl.Set("MediaSource.SourceBufferEvictExtraInBytes", 1));
  ASSERT_TRUE(
      impl.Set("MediaSource.MinimumProcessorCountToOffloadAlgorithm", 1));
  ASSERT_TRUE(impl.Set("MediaSource.EnableAsynchronousReduction", 1));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSizeForImmediateJob", 1));
  ASSERT_TRUE(impl.Set("MediaSource.MaxSourceBufferAppendSizeInBytes", 2));

  EXPECT_EQ(impl.GetSourceBufferEvictExtraInBytes().value(), 1);
  EXPECT_EQ(impl.GetMinimumProcessorCountToOffloadAlgorithm().value(), 1);
  EXPECT_TRUE(impl.IsAsynchronousReductionEnabled().value());
  EXPECT_EQ(impl.GetMaxSizeForImmediateJob().value(), 1);
  EXPECT_EQ(impl.GetMaxSourceBufferAppendSizeInBytes().value(), 2);
}

TEST(MediaSourceSettingsImplTest, InvalidSettingNames) {
  MediaSourceSettingsImpl impl;

  ASSERT_FALSE(impl.Set("MediaSource.Invalid", 0));
  ASSERT_FALSE(impl.Set("Invalid.SourceBufferEvictExtraInBytes", 0));
}

}  // namespace
}  // namespace dom
}  // namespace cobalt
