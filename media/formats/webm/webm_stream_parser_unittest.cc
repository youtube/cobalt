// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_stream_parser.h"

#include <memory>

#include "base/functional/bind.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_media_log.h"
#include "media/base/stream_parser.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/text_track_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::SaveArg;
using testing::_;

namespace media {

class WebMStreamParserTest : public testing::Test {
 public:
  WebMStreamParserTest() = default;

 protected:
  void ParseWebMFile(const std::string& filename,
                     const StreamParser::InitParameters& expected_params) {
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    parser_ = std::make_unique<WebMStreamParser>();
    Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
        base::BindRepeating(&WebMStreamParserTest::OnEncryptedMediaInitData,
                            base::Unretained(this));

    EXPECT_CALL(*this, InitCB(_));
    EXPECT_CALL(*this, NewMediaSegmentCB()).Times(testing::AnyNumber());
    EXPECT_CALL(*this, EndMediaSegmentCB()).Times(testing::AnyNumber());
    EXPECT_CALL(*this, NewBuffersCB(_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(true));
    parser_->Init(base::BindOnce(&WebMStreamParserTest::InitF,
                                 base::Unretained(this), expected_params),
                  base::BindRepeating(&WebMStreamParserTest::NewConfigCB,
                                      base::Unretained(this)),
                  base::BindRepeating(&WebMStreamParserTest::NewBuffersCB,
                                      base::Unretained(this)),
                  false,  // don't ignore_text_track
                  encrypted_media_init_data_cb,
                  base::BindRepeating(&WebMStreamParserTest::NewMediaSegmentCB,
                                      base::Unretained(this)),
                  base::BindRepeating(&WebMStreamParserTest::EndMediaSegmentCB,
                                      base::Unretained(this)),
                  &media_log_);

    // Note this portion is a simplified version of
    // StreamParserTestBase::AppendAllDataThenParseInPieces(). Consider unifying
    // via inheritance or utility method.
    EXPECT_TRUE(
        parser_->AppendToParseBuffer(buffer->data(), buffer->data_size()));
    bool has_more_data = true;
    size_t iterations = 0;
    while (has_more_data) {
      StreamParser::ParseStatus parse_result = parser_->Parse(1);
      EXPECT_NE(StreamParser::ParseStatus::kFailed, parse_result);
      has_more_data =
          parse_result == StreamParser::ParseStatus::kSuccessHasMoreData;
      iterations++;
      EXPECT_EQ(iterations < buffer->data_size(), has_more_data);
    }
  }

  // Verifies only the detected track counts by track type, then chains to the
  // InitCB mock.
  void InitF(const StreamParser::InitParameters& expected_params,
             const StreamParser::InitParameters& params) {
    EXPECT_EQ(expected_params.detected_audio_track_count,
              params.detected_audio_track_count);
    EXPECT_EQ(expected_params.detected_video_track_count,
              params.detected_video_track_count);
    EXPECT_EQ(expected_params.detected_text_track_count,
              params.detected_text_track_count);
    InitCB(params);
  }

  MOCK_METHOD1(InitCB, void(const StreamParser::InitParameters& params));

  bool NewConfigCB(std::unique_ptr<MediaTracks> tracks,
                   const StreamParser::TextTrackConfigMap& text_track_map) {
    size_t audio_config_count = 0;
    size_t video_config_count = 0;
    DCHECK(tracks.get());
    for (const auto& track : tracks->tracks()) {
      if (track->type() == MediaTrack::Audio) {
        audio_config_count++;
        continue;
      }
      if (track->type() == MediaTrack::Video) {
        video_config_count++;
      }
    }
    EXPECT_EQ(tracks->GetAudioConfigs().size(), audio_config_count);
    EXPECT_EQ(tracks->GetVideoConfigs().size(), video_config_count);
    media_tracks_ = std::move(tracks);
    return true;
  }

  MOCK_METHOD1(NewBuffersCB, bool(const StreamParser::BufferQueueMap&));
  MOCK_METHOD2(OnEncryptedMediaInitData,
               void(EmeInitDataType init_data_type,
                    const std::vector<uint8_t>& init_data));
  MOCK_METHOD0(NewMediaSegmentCB, void());
  MOCK_METHOD0(EndMediaSegmentCB, void());

  testing::StrictMock<MockMediaLog> media_log_;
  std::unique_ptr<WebMStreamParser> parser_;
  std::unique_ptr<MediaTracks> media_tracks_;
};

TEST_F(WebMStreamParserTest, VerifyMediaTrackMetadata) {
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimatedAny())
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count = 1;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 0;
  ParseWebMFile("bear.webm", params);
  EXPECT_NE(media_tracks_.get(), nullptr);

  EXPECT_EQ(media_tracks_->tracks().size(), 2u);

  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Video);
  EXPECT_EQ(video_track.bytestream_track_id(), 1);
  EXPECT_EQ(video_track.kind().value(), "main");
  EXPECT_EQ(video_track.label().value(), "");
  EXPECT_EQ(video_track.language().value(), "und");

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track.bytestream_track_id(), 2);
  EXPECT_EQ(audio_track.kind().value(), "main");
  EXPECT_EQ(audio_track.label().value(), "");
  EXPECT_EQ(audio_track.language().value(), "und");
}

TEST_F(WebMStreamParserTest, VerifyDetectedTrack_AudioOnly) {
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimatedAny())
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count = 1;
  params.detected_video_track_count = 0;
  params.detected_text_track_count = 0;
  ParseWebMFile("bear-320x240-audio-only.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 1u);
  EXPECT_EQ(media_tracks_->tracks()[0]->type(), MediaTrack::Audio);
}

TEST_F(WebMStreamParserTest, VerifyDetectedTrack_VideoOnly) {
  StreamParser::InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count = 0;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 0;
  ParseWebMFile("bear-320x240-video-only.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 1u);
  EXPECT_EQ(media_tracks_->tracks()[0]->type(), MediaTrack::Video);
}

TEST_F(WebMStreamParserTest, VerifyDetectedTracks_AVText) {
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimatedAny())
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count = 1;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 1;
  ParseWebMFile("bear-vp8-webvtt.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 2u);
  EXPECT_EQ(media_tracks_->tracks()[0]->type(), MediaTrack::Video);
  EXPECT_EQ(media_tracks_->tracks()[1]->type(), MediaTrack::Audio);
}

TEST_F(WebMStreamParserTest, ColourElement) {
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimatedAny())
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count = 0;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 0;
  ParseWebMFile("colour.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 1u);

  const auto& video_track = media_tracks_->tracks()[0];
  EXPECT_EQ(video_track->type(), MediaTrack::Video);

  const VideoDecoderConfig& video_config =
      media_tracks_->getVideoConfig(video_track->bytestream_track_id());

  VideoColorSpace expected_color_space(VideoColorSpace::PrimaryID::SMPTEST428_1,
                                       VideoColorSpace::TransferID::LOG,
                                       VideoColorSpace::MatrixID::RGB,
                                       gfx::ColorSpace::RangeID::FULL);
  EXPECT_EQ(video_config.color_space_info(), expected_color_space);

  absl::optional<gfx::HDRMetadata> hdr_metadata = video_config.hdr_metadata();
  EXPECT_TRUE(hdr_metadata.has_value());
  EXPECT_EQ(hdr_metadata->max_content_light_level, 11u);
  EXPECT_EQ(hdr_metadata->max_frame_average_light_level, 12u);

  const gfx::ColorVolumeMetadata& mmdata = hdr_metadata->color_volume_metadata;
  EXPECT_FLOAT_EQ(mmdata.primaries.fRX, 0.1f);
  EXPECT_FLOAT_EQ(mmdata.primaries.fRY, 0.2f);
  EXPECT_FLOAT_EQ(mmdata.primaries.fGX, 0.1f);
  EXPECT_FLOAT_EQ(mmdata.primaries.fGY, 0.2f);
  EXPECT_FLOAT_EQ(mmdata.primaries.fBX, 0.1f);
  EXPECT_FLOAT_EQ(mmdata.primaries.fBY, 0.2f);
  EXPECT_FLOAT_EQ(mmdata.primaries.fWX, 0.1f);
  EXPECT_FLOAT_EQ(mmdata.primaries.fWY, 0.2f);
  EXPECT_EQ(mmdata.luminance_max, 40);
  EXPECT_EQ(mmdata.luminance_min, 30);
}

TEST_F(WebMStreamParserTest, ColourElementWithUnspecifiedRange) {
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimatedAny())
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count = 0;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 0;
  ParseWebMFile("colour_unspecified_range.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 1u);

  const auto& video_track = media_tracks_->tracks()[0];
  EXPECT_EQ(video_track->type(), MediaTrack::Video);

  const VideoDecoderConfig& video_config =
      media_tracks_->getVideoConfig(video_track->bytestream_track_id());

  VideoColorSpace expected_color_space(VideoColorSpace::PrimaryID::SMPTEST428_1,
                                       VideoColorSpace::TransferID::LOG,
                                       VideoColorSpace::MatrixID::RGB,
                                       gfx::ColorSpace::RangeID::INVALID);
  EXPECT_EQ(video_config.color_space_info(), expected_color_space);
}

TEST_F(WebMStreamParserTest, ParseVideoWithSphericalMetadata) {
  EXPECT_MEDIA_LOG(WebMSimpleBlockDurationEstimatedAny())
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count = 0;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 0;
  ParseWebMFile("bear-spherical-metadata.webm", params);
  EXPECT_EQ(media_tracks_->tracks().size(), 1u);

  const auto& video_track = media_tracks_->tracks()[0];
  EXPECT_EQ(video_track->type(), MediaTrack::Video);
}
}  // namespace media
