// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/formats/webm/webm_stream_parser.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/demuxer.h"
#include "cobalt/media/base/media_tracks.h"
#include "cobalt/media/base/mock_media_log.h"
#include "cobalt/media/base/stream_parser.h"
#include "cobalt/media/base/test_data_util.h"
#include "cobalt/media/base/text_track_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::SaveArg;
using testing::_;

namespace media {

class WebMStreamParserTest : public testing::Test {
 public:
  WebMStreamParserTest()
      : media_log_(new testing::StrictMock<MockMediaLog>()) {}

 protected:
  void ParseWebMFile(const std::string& filename,
                     const StreamParser::InitParameters& expected_params) {
    scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile(filename);
    parser_.reset(new WebMStreamParser());
    Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
        base::Bind(&WebMStreamParserTest::OnEncryptedMediaInitData,
                   base::Unretained(this));

    EXPECT_CALL(*this, InitCB(_));
    EXPECT_CALL(*this, NewMediaSegmentCB()).Times(testing::AnyNumber());
    EXPECT_CALL(*this, EndMediaSegmentCB()).Times(testing::AnyNumber());
    EXPECT_CALL(*this, NewBuffersCB(_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(true));
    parser_->Init(
        base::Bind(&WebMStreamParserTest::InitF, base::Unretained(this),
                   expected_params),
        base::Bind(&WebMStreamParserTest::NewConfigCB, base::Unretained(this)),
        base::Bind(&WebMStreamParserTest::NewBuffersCB, base::Unretained(this)),
        false,  // don't ignore_text_track
        encrypted_media_init_data_cb,
        base::Bind(&WebMStreamParserTest::NewMediaSegmentCB,
                   base::Unretained(this)),
        base::Bind(&WebMStreamParserTest::EndMediaSegmentCB,
                   base::Unretained(this)),
        media_log_);
    bool result = parser_->Parse(buffer->data(), buffer->data_size());
    EXPECT_TRUE(result);
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

  bool NewConfigCB(scoped_ptr<MediaTracks> tracks,
                   const StreamParser::TextTrackConfigMap& text_track_map) {
    DCHECK(tracks.get());
    media_tracks_ = tracks.Pass();
    return true;
  }

  MOCK_METHOD1(NewBuffersCB, bool(const StreamParser::BufferQueueMap&));
  MOCK_METHOD2(OnEncryptedMediaInitData,
               void(EmeInitDataType init_data_type,
                    const std::vector<uint8_t>& init_data));
  MOCK_METHOD0(NewMediaSegmentCB, void());
  MOCK_METHOD0(EndMediaSegmentCB, void());

  scoped_refptr<testing::StrictMock<MockMediaLog>> media_log_;
  scoped_ptr<WebMStreamParser> parser_;
  scoped_ptr<MediaTracks> media_tracks_;
};

TEST_F(WebMStreamParserTest, VerifyMediaTrackMetadata) {
  EXPECT_MEDIA_LOG(testing::HasSubstr("Estimating WebM block duration"))
      .Times(testing::AnyNumber());
  StreamParser::InitParameters params(kInfiniteDuration);
  params.detected_audio_track_count = 1;
  params.detected_video_track_count = 1;
  params.detected_text_track_count = 0;
  ParseWebMFile("bear.webm", params);
  EXPECT_NE(media_tracks_.get(), NULL);

  EXPECT_EQ(media_tracks_->tracks().size(), 2u);

  const MediaTrack& video_track = *(media_tracks_->tracks()[0]);
  EXPECT_EQ(video_track.type(), MediaTrack::Video);
  EXPECT_EQ(video_track.bytestream_track_id(), 1);
  EXPECT_EQ(video_track.kind(), "main");
  EXPECT_EQ(video_track.label(), "");
  EXPECT_EQ(video_track.language(), "und");

  const MediaTrack& audio_track = *(media_tracks_->tracks()[1]);
  EXPECT_EQ(audio_track.type(), MediaTrack::Audio);
  EXPECT_EQ(audio_track.bytestream_track_id(), 2);
  EXPECT_EQ(audio_track.kind(), "main");
  EXPECT_EQ(audio_track.label(), "");
  EXPECT_EQ(audio_track.language(), "und");
}

TEST_F(WebMStreamParserTest, VerifyDetectedTrack_AudioOnly) {
  EXPECT_MEDIA_LOG(testing::HasSubstr("Estimating WebM block duration"))
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
  EXPECT_MEDIA_LOG(testing::HasSubstr("Estimating WebM block duration"))
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
  EXPECT_MEDIA_LOG(testing::HasSubstr("Estimating WebM block duration"))
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

  gfx::ColorSpace expected_color_space(gfx::ColorSpace::PrimaryID::SMPTEST428_1,
                                       gfx::ColorSpace::TransferID::LOG,
                                       gfx::ColorSpace::MatrixID::RGB,
                                       gfx::ColorSpace::RangeID::FULL);
  EXPECT_EQ(video_config.color_space_info(), expected_color_space);

  base::Optional<HDRMetadata> hdr_metadata = video_config.hdr_metadata();
  EXPECT_TRUE(hdr_metadata.has_value());
  EXPECT_EQ(hdr_metadata->max_cll, 11u);
  EXPECT_EQ(hdr_metadata->max_fall, 12u);

  const MasteringMetadata& mmdata = hdr_metadata->mastering_metadata;
  EXPECT_FLOAT_EQ(mmdata.primary_r_chromaticity_x, 0.1f);
  EXPECT_FLOAT_EQ(mmdata.primary_r_chromaticity_y, 0.2f);
  EXPECT_FLOAT_EQ(mmdata.primary_g_chromaticity_x, 0.1f);
  EXPECT_FLOAT_EQ(mmdata.primary_g_chromaticity_y, 0.2f);
  EXPECT_FLOAT_EQ(mmdata.primary_b_chromaticity_x, 0.1f);
  EXPECT_FLOAT_EQ(mmdata.primary_b_chromaticity_y, 0.2f);
  EXPECT_FLOAT_EQ(mmdata.white_point_chromaticity_x, 0.1f);
  EXPECT_FLOAT_EQ(mmdata.white_point_chromaticity_y, 0.2f);
  EXPECT_EQ(mmdata.luminance_max, 40);
  EXPECT_EQ(mmdata.luminance_min, 30);
}

}  // namespace media
