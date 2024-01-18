// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/base/cval_stats.h"

#include <set>
#include <string>

#include "base/time/time.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"


const char kCValnameLatest[] = "Media.SbPlayerCreateTime.Latest";
const char kCValnameAverage[] = "Media.SbPlayerCreateTime.Average";
const char kCValnameMaximum[] = "Media.SbPlayerCreateTime.Maximum";
const char kCValnameMedian[] = "Media.SbPlayerCreateTime.Median";
const char kCValnameMinimum[] = "Media.SbPlayerCreateTime.Minimum";

const char kPipelineIdentifier[] = "test_pipeline";

constexpr base::TimeDelta kSleepTime =
    base::TimeDelta::FromMicroseconds(50);  // 50 microseconds

namespace cobalt {
namespace media {

TEST(MediaCValStatsTest, InitiallyEmpty) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  std::set<std::string> cval_names = cvm->GetOrderedCValNames();
  std::set<std::string>::size_type size = cval_names.size();
  EXPECT_EQ(size, 0);
}

TEST(MediaCValStatsTest, CValStatsCreation) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  CValStats cval_stats_;
  std::set<std::string> cval_names = cvm->GetOrderedCValNames();
  std::set<std::string>::size_type size = cval_names.size();
  EXPECT_EQ(size, 15);
}

TEST(MediaCValStatsTest, NothingRecorded) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  CValStats cval_stats_;

  cval_stats_.StartTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
  SbThreadSleep(kSleepTime.InMicroseconds());

  cval_stats_.StopTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);

  base::Optional<std::string> result = cvm->GetValueAsString(kCValnameLatest);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "0");
}

TEST(MediaCValStatsTest, EnableRecording) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  CValStats cval_stats_;

  cval_stats_.Enable(true);

  cval_stats_.StartTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
  SbThreadSleep(kSleepTime.InMicroseconds());
  cval_stats_.StopTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);

  base::Optional<std::string> result = cvm->GetValueAsString(kCValnameLatest);
  EXPECT_TRUE(result);
  EXPECT_NE(*result, "0");

  result = cvm->GetValueAsString(kCValnameMinimum);
  EXPECT_TRUE(result);
  EXPECT_NE(*result, "0");

  result = cvm->GetValueAsString(kCValnameMaximum);
  EXPECT_TRUE(result);
  EXPECT_NE(*result, "0");
}

TEST(MediaCValStatsTest, DontGenerateHistoricalData) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  CValStats cval_stats_;

  cval_stats_.Enable(true);

  for (int i = 0; i < kMediaDefaultMaxSamplesBeforeCalculation - 1; i++) {
    cval_stats_.StartTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
    SbThreadSleep(kSleepTime.InMicroseconds());
    cval_stats_.StopTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
  }

  base::Optional<std::string> result = cvm->GetValueAsString(kCValnameLatest);
  EXPECT_TRUE(result);
  EXPECT_NE(*result, "0");

  result = cvm->GetValueAsString(kCValnameAverage);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "0");

  result = cvm->GetValueAsString(kCValnameMedian);
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, "0");
}

TEST(MediaCValStatsTest, GenerateHistoricalData) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  CValStats cval_stats_;

  cval_stats_.Enable(true);

  for (int i = 0; i < kMediaDefaultMaxSamplesBeforeCalculation; i++) {
    cval_stats_.StartTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
    SbThreadSleep(kSleepTime.InMicroseconds());
    cval_stats_.StopTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
  }

  base::Optional<std::string> result = cvm->GetValueAsString(kCValnameAverage);
  EXPECT_TRUE(result);
  EXPECT_NE(*result, "0");

  result = cvm->GetValueAsString(kCValnameMedian);
  EXPECT_TRUE(result);
  EXPECT_NE(*result, "0");
}

TEST(MediaCValStatsTest, CircularBufferDontWrapIndex) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  CValStats cval_stats_;

  cval_stats_.Enable(true);

  for (int i = 0; i < kMaxSamples - 1; i++) {
    cval_stats_.StartTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
    cval_stats_.StopTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
  }

  EXPECT_EQ(cval_stats_.GetBufferIndexForTest(), 99);
}


TEST(MediaCValStatsTest, CircularBufferWrapIndex) {
  base::CValManager* cvm = base::CValManager::GetInstance();
  EXPECT_TRUE(cvm);
  CValStats cval_stats_;

  cval_stats_.Enable(true);

  for (int i = 0; i < kMaxSamples + 5; i++) {
    cval_stats_.StartTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
    cval_stats_.StopTimer(MediaTiming::SbPlayerCreate, kPipelineIdentifier);
  }

  EXPECT_EQ(cval_stats_.GetBufferIndexForTest(), 5);
}

}  // namespace media
}  // namespace cobalt
