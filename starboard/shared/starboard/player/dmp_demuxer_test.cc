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

#include "starboard/shared/starboard/player/dmp_demuxer.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace video_dmp {

namespace {

typedef std::map<SbMediaType, std::string> MediaTypeToTestFileNameMap;

std::vector<std::string> kAudioTestFiles = {
    "beneath_the_canopy_aac_stereo.dmp",
    "beneath_the_canopy_aac_5_1.dmp",
    "beneath_the_canopy_aac_mono.dmp",
    "beneath_the_canopy_opus_5_1.dmp",
    "beneath_the_canopy_opus_stereo.dmp",
    "beneath_the_canopy_opus_mono.dmp",
    "sintel_329_ec3.dmp",
    "sintel_381_ac3.dmp"};

std::vector<std::string> kVideoTestFiles = {"beneath_the_canopy_137_avc.dmp",
                                            "beneath_the_canopy_248_vp9.dmp",
                                            "sintel_399_av1.dmp"};

std::vector<MediaTypeToTestFileNameMap> GetDmpDemuxerTestConfigs() {
  std::vector<MediaTypeToTestFileNameMap> dmp_demuxer_test_configs;
  MediaTypeToTestFileNameMap test_config;

  for (const auto& audio_fname : kAudioTestFiles) {
    test_config[kSbMediaTypeAudio] = audio_fname;
    for (const auto& video_fname : kVideoTestFiles) {
      test_config[kSbMediaTypeVideo] = video_fname;
      dmp_demuxer_test_configs.emplace_back(test_config);
    }
  }
  return dmp_demuxer_test_configs;
}

std::string GetDmpDemuxerTestConfigName(
    ::testing::TestParamInfo<MediaTypeToTestFileNameMap> info) {
  const MediaTypeToTestFileNameMap test_file_names = info.param;

  std::string name;
  for (const auto& it : test_file_names) {
    name += FormatString("%s_%s_",
                         (it.first == kSbMediaTypeAudio) ? "audio" : "video",
                         it.second.c_str());
  }
  name += "test";
  std::replace(name.begin(), name.end(), '.', '_');
  std::replace(name.begin(), name.end(), '(', '_');
  std::replace(name.begin(), name.end(), ')', '_');
  return name;
}
}  // namespace

class DmpDemuxerTest
    : public ::testing::TestWithParam<MediaTypeToTestFileNameMap> {
 protected:
  DmpDemuxerTest();
  ~DmpDemuxerTest() {}
  void SetUp() override;

  void ReadUtilDone();

  std::unique_ptr<DmpDemuxer> demuxer_;
  std::vector<std::unique_ptr<VideoDmpReader>> dmp_readers_;
  MediaTypeToTestFileNameMap test_file_names_;
};

DmpDemuxerTest::DmpDemuxerTest() : test_file_names_(GetParam()) {}

void DmpDemuxerTest::SetUp() {
  ASSERT_FALSE(test_file_names_.empty());

  DmpDemuxer::MediaTypeToDmpReaderMap dmp_demuxer_config;
  for (const auto& test_file_name : test_file_names_) {
    dmp_readers_.emplace_back(
        new VideoDmpReader(test_file_name.second.c_str()));
    dmp_demuxer_config[test_file_name.first] = dmp_readers_.back().get();
  }
  demuxer_.reset(new DmpDemuxer(dmp_demuxer_config));
  ASSERT_TRUE(demuxer_->GetDuration() > 0);
}

void DmpDemuxerTest::ReadUtilDone() {
  SbPlayerSampleInfo sample_info;
  SbTime start, end;
  demuxer_->GetReadRange(&start, &end);

  bool audio_eos = demuxer_->HasMediaTrack(kSbMediaTypeAudio) ? false : true;
  bool video_eos = demuxer_->HasMediaTrack(kSbMediaTypeVideo) ? false : true;
  SbTime audio_time = 0;
  SbTime video_time = 0;
  int audio_sample_count = 0;
  while (!audio_eos || !video_eos) {
    if ((audio_time <= video_time && !audio_eos) || video_eos) {
      if (demuxer_->ReadSample(kSbMediaTypeAudio, &sample_info)) {
        audio_time = sample_info.timestamp;
        if (audio_sample_count == 0) {
          EXPECT_GE(start, audio_time);
        } else {
          EXPECT_GE(audio_time, start);
        }
        EXPECT_LE(audio_time, end);
        ++audio_sample_count;
      } else {
        audio_eos = true;
      }
    } else {
      if (demuxer_->ReadSample(kSbMediaTypeVideo, &sample_info)) {
        video_time = sample_info.timestamp;
        EXPECT_LE(video_time, end);
      } else {
        video_eos = true;
      }
    }
  }
}

TEST_P(DmpDemuxerTest, GetDuration) {
  DmpDemuxer::MediaTypeToDmpReaderMap dmp_demuxer_config;
  dmp_demuxer_config[kSbMediaTypeAudio] =
      demuxer_->GetDmpReader(kSbMediaTypeAudio);
  DmpDemuxer audio_demuxer(dmp_demuxer_config);
  dmp_demuxer_config.clear();

  dmp_demuxer_config[kSbMediaTypeVideo] =
      demuxer_->GetDmpReader(kSbMediaTypeVideo);
  DmpDemuxer video_demuxer(dmp_demuxer_config);
  SbTime shortest_track_duration =
      std::min(audio_demuxer.GetDuration(), video_demuxer.GetDuration());

  EXPECT_EQ(demuxer_->GetDuration(), shortest_track_duration);
}

TEST_P(DmpDemuxerTest, Seek) {
  EXPECT_EQ(demuxer_->Seek(0), true);
  EXPECT_EQ(demuxer_->Seek(demuxer_->GetDuration() - 1), true);
  EXPECT_EQ(demuxer_->Seek(demuxer_->GetDuration()), false);
}

TEST_P(DmpDemuxerTest, SetReadRange) {
  SbTime start, end;
  EXPECT_TRUE(demuxer_->SetReadRange(0, demuxer_->GetDuration() - 1));
  demuxer_->GetReadRange(&start, &end);
  EXPECT_EQ(start, 0);
  EXPECT_EQ(end, demuxer_->GetDuration() - 1);
}

TEST_P(DmpDemuxerTest, ReadSample) {
  SbTime start_time, end_time;
  SbTime duration = demuxer_->GetDuration();

  // Read all samples.
  ReadUtilDone();

  // Seek and read to the end of file.
  start_time = (4 * duration / 5);
  ASSERT_EQ(demuxer_->Seek(start_time), true);
  ReadUtilDone();

  // Set read range and read to the end of range.
  start_time = (6 * duration / 7);
  end_time = (7 * duration / 8);
  ASSERT_EQ(demuxer_->SetReadRange(start_time, end_time), true);
  ReadUtilDone();
}

INSTANTIATE_TEST_CASE_P(DmpDemuxerTests,
                        DmpDemuxerTest,
                        testing::ValuesIn(GetDmpDemuxerTestConfigs()),
                        GetDmpDemuxerTestConfigName);

// Only beneath_the_canopy_137_avc.dmp has bidirectional prediction frames.
TEST(VideoDmpDemuxerTest, SeekInGopWithBidirectionalPredictionFrames) {
  DmpDemuxer::MediaTypeToDmpReaderMap dmp_demuxer_config;
  VideoDmpReader dmp_reader("beneath_the_canopy_137_avc.dmp");
  dmp_demuxer_config[kSbMediaTypeVideo] = &dmp_reader;
  DmpDemuxer video_demuxer(dmp_demuxer_config);
  ASSERT_TRUE(video_demuxer.GetDuration() > 0);

  SbTime seek_time = 98765;  // us
  EXPECT_EQ(video_demuxer.SetReadRange(0, seek_time), true);
  SbPlayerSampleInfo sample_info;
  std::set<SbTime> timestamps;
  // The first GOP is:
  // I, P1, B1, B2,               P2  ...(decoding order)
  // I, B1, B2, P1, *(seek here), P2  ...(presentation order)
  // After reading, the set of timestamps should include T(I), T(B1), T(B2), and
  // T(P1).
  while (video_demuxer.ReadSample(kSbMediaTypeVideo, &sample_info)) {
    timestamps.insert(sample_info.timestamp);
  }

  // Extend the end of the original reading range by 100 milliseconds.
  EXPECT_EQ(video_demuxer.SetReadRange(0, seek_time + 100 * kSbTimeMillisecond),
            true);
  // After enlarging the range, check all samples that do not belong to
  // |timestamps| to ensure that their timestamps are greater than the seek
  // time.
  while (video_demuxer.ReadSample(kSbMediaTypeVideo, &sample_info)) {
    if (timestamps.find(sample_info.timestamp) == timestamps.end()) {
      EXPECT_GE(sample_info.timestamp, seek_time);
    }
  }
}

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
