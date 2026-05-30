// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/url_player_demuxer.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

class TestDemuxerHost : public DemuxerHost {
 public:
  void OnBufferedTimeRangesChanged(
      const Ranges<base::TimeDelta>& ranges) override {}
  void SetDuration(base::TimeDelta duration) override {}
  void OnDemuxerError(PipelineStatus error) override {}
};

class UrlPlayerDemuxerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(UrlPlayerDemuxerTest, ExposesUrlAndPlaceholderStreams) {
  const GURL url("https://example.com/test.m3u8");
  UrlPlayerDemuxer demuxer(task_environment_.GetMainThreadTaskRunner(), url);

  EXPECT_EQ(url, demuxer.GetMediaUrl());
  EXPECT_EQ("UrlPlayerDemuxer", demuxer.GetDisplayName());
  EXPECT_EQ(DemuxerType::kUrlPlayerDemuxer, demuxer.GetDemuxerType());

  std::vector<DemuxerStream*> streams = demuxer.GetAllStreams();
  ASSERT_EQ(2u, streams.size());
  EXPECT_EQ(DemuxerStream::AUDIO, streams[0]->type());
  EXPECT_EQ(DemuxerStream::VIDEO, streams[1]->type());
}

TEST_F(UrlPlayerDemuxerTest, InitializeCompletesAsynchronously) {
  UrlPlayerDemuxer demuxer(task_environment_.GetMainThreadTaskRunner(),
                           GURL("https://example.com/test.m3u8"));
  TestDemuxerHost host;
  base::RunLoop run_loop;

  demuxer.Initialize(&host,
                     base::BindOnce(
                         [](base::RunLoop* run_loop, PipelineStatus status) {
                           EXPECT_EQ(PIPELINE_OK, status);
                           run_loop->Quit();
                         },
                         &run_loop));

  run_loop.Run();
}

TEST_F(UrlPlayerDemuxerTest, SeekCompletesAsynchronously) {
  UrlPlayerDemuxer demuxer(task_environment_.GetMainThreadTaskRunner(),
                           GURL("https://example.com/test.m3u8"));
  base::RunLoop run_loop;

  demuxer.Seek(base::Seconds(5),
               base::BindOnce(
                   [](base::RunLoop* run_loop, PipelineStatus status) {
                     EXPECT_EQ(PIPELINE_OK, status);
                     run_loop->Quit();
                   },
                   &run_loop));

  run_loop.Run();
}

TEST_F(UrlPlayerDemuxerTest, OnTracksChangedCompletesWithNoStreams) {
  UrlPlayerDemuxer demuxer(task_environment_.GetMainThreadTaskRunner(),
                           GURL("https://example.com/test.m3u8"));
  base::RunLoop run_loop;

  demuxer.OnTracksChanged(DemuxerStream::AUDIO, {}, base::TimeDelta(),
                          base::BindOnce(
                              [](base::RunLoop* run_loop,
                                 const std::vector<DemuxerStream*>& streams) {
                                EXPECT_TRUE(streams.empty());
                                run_loop->Quit();
                              },
                              &run_loop));

  run_loop.Run();
}

TEST_F(UrlPlayerDemuxerTest, PlaceholderStreamConfigsAreValid) {
  UrlPlayerDemuxer demuxer(task_environment_.GetMainThreadTaskRunner(),
                           GURL("https://example.com/test.m3u8"));
  std::vector<DemuxerStream*> streams = demuxer.GetAllStreams();

  // Audio: minimal valid placeholder config.
  AudioDecoderConfig audio_config = streams[0]->audio_decoder_config();
  EXPECT_TRUE(audio_config.IsValidConfig());
  EXPECT_EQ(AudioCodec::kAAC, audio_config.codec());

  // Video: minimal valid placeholder config.
  VideoDecoderConfig video_config = streams[1]->video_decoder_config();
  EXPECT_TRUE(video_config.IsValidConfig());
  EXPECT_EQ(VideoCodec::kH264, video_config.codec());
}

}  // namespace
}  // namespace media
