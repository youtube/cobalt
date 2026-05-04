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

// clang-format off
#include "media/filters/source_buffer_state.h"
// clang-format on

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/frame_processor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace {

VideoDecoderConfig CreateVideoConfig(VideoCodec codec) {
  gfx::Size size(16, 16);
  return VideoDecoderConfig(codec, VIDEO_CODEC_PROFILE_UNKNOWN,
                            VideoDecoderConfig::AlphaMode::kIsOpaque,
                            VideoColorSpace::REC709(), kNoTransformation, size,
                            gfx::Rect(size), size, {},
                            EncryptionScheme::kUnencrypted);
}

void AddVideoTrack(std::unique_ptr<MediaTracks>& t, VideoCodec codec, int id) {
  t->AddVideoTrack(CreateVideoConfig(codec), true, id, MediaTrack::Kind(),
                   MediaTrack::Label(), MediaTrack::Language());
}

}  // namespace

class StarboardSourceBufferStateTest : public ::testing::Test {
 public:
  StarboardSourceBufferStateTest() : mock_stream_parser_(nullptr) {}

  void TearDown() override { SourceBufferState::SetVideoBufferSizeClampMb(0); }

  std::unique_ptr<SourceBufferState> CreateSourceBufferState() {
    std::unique_ptr<FrameProcessor> frame_processor =
        std::make_unique<FrameProcessor>(base::DoNothing(), &media_log_);
    mock_stream_parser_ = new testing::StrictMock<MockStreamParser>();
    return base::WrapUnique(new SourceBufferState(
        base::WrapUnique(mock_stream_parser_.get()), std::move(frame_processor),
        base::BindRepeating(
            &StarboardSourceBufferStateTest::CreateDemuxerStream,
            base::Unretained(this)),
        &media_log_));
  }

  std::unique_ptr<SourceBufferState> CreateAndInitSourceBufferState(
      const std::string& expected_codecs) {
    std::unique_ptr<SourceBufferState> sbs = CreateSourceBufferState();
    EXPECT_CALL(*mock_stream_parser_, Init(_, _, _, _, _, _, _))
        .WillOnce([&](auto init_cb, auto config_cb, auto new_buffers_cb,
                      auto encrypted_media_init_data_cb, auto new_segment_cb,
                      auto end_of_segment_cb,
                      auto media_log) { new_config_cb_ = config_cb; });
    sbs->Init(base::DoNothing(), expected_codecs, base::DoNothing());

    sbs->SetTracksWatcher(base::BindRepeating(
        &StarboardSourceBufferStateTest::OnMediaTracksUpdated,
        base::Unretained(this)));

    EXPECT_CALL(*this, OnParseWarningMock(_)).Times(0);

    sbs->SetParseWarningCallback(
        base::BindRepeating(&StarboardSourceBufferStateTest::OnParseWarningMock,
                            base::Unretained(this)));

    return sbs;
  }

  void AppendDataAndReportTracks(const std::unique_ptr<SourceBufferState>& sbs,
                                 std::unique_ptr<MediaTracks> tracks) {
    base::TimeDelta t;

    EXPECT_CALL(*mock_stream_parser_, AppendToParseBuffer(_))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_stream_parser_, Parse(_))
        .WillOnce(DoAll(
            InvokeWithoutArgs([&] { new_config_cb_.Run(std::move(tracks)); }),
            Return(StreamParser::ParseStatus::kSuccess)));

    EXPECT_TRUE(sbs->AppendToParseBuffer(base::span<const uint8_t>()));
    EXPECT_EQ(StreamParser::ParseStatus::kSuccess,
              sbs->RunSegmentParserLoop(t, t, &t));
  }

  MOCK_METHOD1(OnParseWarningMock, void(const SourceBufferParseWarning));
  MOCK_METHOD1(MediaTracksUpdatedMock, void(std::unique_ptr<MediaTracks>&));
  void OnMediaTracksUpdated(std::unique_ptr<MediaTracks> tracks) {
    MediaTracksUpdatedMock(tracks);
  }

  ChunkDemuxerStream* CreateDemuxerStream(DemuxerStream::Type type) {
    static unsigned track_id = 0;
    demuxer_streams_.push_back(base::WrapUnique(new ChunkDemuxerStream(
        type, MediaTrack::Id(base::NumberToString(++track_id)))));
    return demuxer_streams_.back().get();
  }

  testing::StrictMock<MockMediaLog> media_log_;
  std::vector<std::unique_ptr<ChunkDemuxerStream>> demuxer_streams_;
  raw_ptr<MockStreamParser, DanglingUntriaged> mock_stream_parser_;
  StreamParser::NewConfigCB new_config_cb_;
};

TEST_F(StarboardSourceBufferStateTest, SetVideoBufferSizeClampMb) {
  std::unique_ptr<SourceBufferState> sbs =
      CreateAndInitSourceBufferState("vp9");

  auto tracks = std::make_unique<MediaTracks>();
  AddVideoTrack(tracks, VideoCodec::kVP9, 1);
  EXPECT_FOUND_CODEC_NAME(Video, "vp9");
  EXPECT_CALL(*this, MediaTracksUpdatedMock(_));

  SourceBufferState::SetVideoBufferSizeClampMb(10);

  EXPECT_MEDIA_LOG(
      "{\"info\":\"Custom video per-track SourceBuffer size clamp "
      "limit=10485760\"}");
  AppendDataAndReportTracks(sbs, std::move(tracks));
}

}  // namespace media
