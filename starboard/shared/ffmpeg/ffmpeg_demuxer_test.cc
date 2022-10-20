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

#include "starboard/shared/ffmpeg/ffmpeg_demuxer.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/log.h"
#include "starboard/system.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace ffmpeg {
namespace {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAreArray;
using ::testing::ExplainMatchResult;
using ::testing::Ge;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::SizeIs;

// Streaming is not supported.
constexpr bool kIsStreaming = false;
constexpr char kInputWebm[] = "bear-320x240.webm";

std::string ResolveTestFilename(const char* filename) {
  std::vector<char> content_path(kSbFileMaxPath);
  SB_CHECK(SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                           kSbFileMaxPath));
  const std::string directory_path =
      std::string(content_path.data()) + kSbFileSepChar + "third_party" +
      kSbFileSepChar + "chromium" + kSbFileSepChar + "media" + kSbFileSepChar +
      "test" + kSbFileSepChar + "data";
  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str()))
      << "Cannot open directory " << directory_path;
  return directory_path + kSbFileSepChar + filename;
}

// Used to convert a MockFn to a pure C function.
template <typename T, typename U>
void CallMockCB(U* u, void* user_data) {
  static_cast<T*>(user_data)->AsStdFunction()(u);
}

// These functions forward calls to a FstreamDataSource.
int FstreamBlockingRead(uint8_t* data, int bytes_requested, void* user_data) {
  auto* input = static_cast<std::ifstream*>(user_data);
  input->read(reinterpret_cast<char*>(data), bytes_requested);
  return input->gcount();
}

void FstreamSeekTo(int position, void* user_data) {
  auto* input = static_cast<std::ifstream*>(user_data);
  input->seekg(position, input->beg);
}

int64_t FstreamGetPosition(void* user_data) {
  return static_cast<std::ifstream*>(user_data)->tellg();
}

int64_t FstreamGetSize(void* user_data) {
  // Seek to the end of the file to get the size, then seek back to the current
  // position.
  auto* input = static_cast<std::ifstream*>(user_data);
  const auto current_pos = input->tellg();
  input->seekg(0, input->end);
  const int64_t filesize = input->tellg();
  input->seekg(current_pos);
  return filesize;
}

TEST(FFmpegDemuxerTest, InitializeSucceeds) {
  const std::string filename = ResolveTestFilename(kInputWebm);
  std::ifstream input(filename, std::ios_base::binary);
  SB_CHECK(input.good()) << "Unable to load file " << filename;

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());
  CobaltExtensionDemuxerDataSource c_data_source{
      &FstreamBlockingRead, &FstreamSeekTo, &FstreamGetPosition,
      &FstreamGetSize,      kIsStreaming,   &input};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(demuxer, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  api->DestroyDemuxer(demuxer);
}

TEST(FFmpegDemuxerTest, ReadsAudioData) {
  const std::string filename = ResolveTestFilename(kInputWebm);
  std::ifstream input(filename, std::ios_base::binary);
  SB_CHECK(input.good()) << "Unable to load file " << filename;

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());
  CobaltExtensionDemuxerDataSource c_data_source{
      &FstreamBlockingRead, &FstreamSeekTo, &FstreamGetPosition,
      &FstreamGetSize,      kIsStreaming,   &input};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(demuxer, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  const std::vector<uint8_t> expected_audio = {
      0xbc, 0xd6, 0xf2, 0xda, 0x7b, 0xad, 0xe5, 0xb5,
      0x87, 0x29, 0x05, 0x50, 0x6b, 0xfc, 0x6f, 0xd6,
  };

  MockFunction<void(CobaltExtensionDemuxerBuffer*)> read_cb;
  std::vector<uint8_t> actual_audio;
  bool read_eos = false;

  EXPECT_CALL(read_cb, Call(_))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke([&](CobaltExtensionDemuxerBuffer* buffer) {
        SB_CHECK(buffer);
        if (buffer->end_of_stream || buffer->data_size <= 0) {
          read_eos = buffer->end_of_stream;
          return;
        }
        actual_audio.insert(actual_audio.end(), buffer->data,
                            buffer->data + buffer->data_size);
      }));

  // Calling Read may return less -- or more -- data than we need. Keep reading
  // until we have at least as much data as the amount we're checking.
  while (!read_eos && actual_audio.size() < expected_audio.size()) {
    demuxer->Read(kCobaltExtensionDemuxerStreamTypeAudio,
                  &CallMockCB<decltype(read_cb), CobaltExtensionDemuxerBuffer>,
                  &read_cb, demuxer->user_data);
  }

  ASSERT_THAT(actual_audio, SizeIs(Ge(expected_audio.size())));
  actual_audio.resize(expected_audio.size());
  EXPECT_THAT(actual_audio, ElementsAreArray(expected_audio));

  api->DestroyDemuxer(demuxer);
}

TEST(FFmpegDemuxerTest, ReadsVideoData) {
  const std::string filename = ResolveTestFilename(kInputWebm);
  std::ifstream input(filename, std::ios_base::binary);
  SB_CHECK(input.good()) << "Unable to load file " << filename;

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());
  CobaltExtensionDemuxerDataSource c_data_source{
      &FstreamBlockingRead, &FstreamSeekTo, &FstreamGetPosition,
      &FstreamGetSize,      kIsStreaming,   &input};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(demuxer, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  const std::vector<uint8_t> expected_video = {
      0x50, 0x04, 0x01, 0x9d, 0x01, 0x2a, 0x40, 0x01,
      0xf0, 0x00, 0x00, 0x47, 0x08, 0x85, 0x85, 0x88,
  };

  MockFunction<void(CobaltExtensionDemuxerBuffer*)> read_cb;
  std::vector<uint8_t> actual_video;
  bool read_eos = false;

  EXPECT_CALL(read_cb, Call(_))
      .Times(AtLeast(1))
      .WillRepeatedly(Invoke([&](CobaltExtensionDemuxerBuffer* buffer) {
        SB_CHECK(buffer);
        if (buffer->end_of_stream || buffer->data_size <= 0) {
          read_eos = buffer->end_of_stream;
          return;
        }
        actual_video.insert(actual_video.end(), buffer->data,
                            buffer->data + buffer->data_size);
      }));

  // Calling Read may return less -- or more -- data than we need. Keep reading
  // until we have at least as much data as the amount we're checking.
  while (!read_eos && actual_video.size() < expected_video.size()) {
    demuxer->Read(kCobaltExtensionDemuxerStreamTypeVideo,
                  &CallMockCB<decltype(read_cb), CobaltExtensionDemuxerBuffer>,
                  &read_cb, demuxer->user_data);
  }

  ASSERT_THAT(actual_video, SizeIs(Ge(expected_video.size())));
  actual_video.resize(expected_video.size());
  EXPECT_THAT(actual_video, ElementsAreArray(expected_video));

  api->DestroyDemuxer(demuxer);
}

TEST(FFmpegDemuxerTest, PopulatesAudioConfig) {
  const std::string filename = ResolveTestFilename(kInputWebm);
  std::ifstream input(filename, std::ios_base::binary);
  SB_CHECK(input.good()) << "Unable to load file " << filename;

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());
  CobaltExtensionDemuxerDataSource c_data_source{
      &FstreamBlockingRead, &FstreamSeekTo, &FstreamGetPosition,
      &FstreamGetSize,      kIsStreaming,   &input};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(demuxer, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  CobaltExtensionDemuxerAudioDecoderConfig actual_config = {};
  demuxer->GetAudioConfig(&actual_config, demuxer->user_data);

  EXPECT_EQ(actual_config.codec, kCobaltExtensionDemuxerCodecVorbis);
  EXPECT_EQ(actual_config.sample_format,
            kCobaltExtensionDemuxerSampleFormatPlanarF32);
  EXPECT_EQ(actual_config.channel_layout,
            kCobaltExtensionDemuxerChannelLayoutStereo);
  EXPECT_EQ(actual_config.encryption_scheme,
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted);
  EXPECT_EQ(actual_config.samples_per_second, 44100);

  api->DestroyDemuxer(demuxer);
}

TEST(FFmpegDemuxerTest, PopulatesVideoConfig) {
  const std::string filename = ResolveTestFilename(kInputWebm);
  std::ifstream input(filename, std::ios_base::binary);
  SB_CHECK(input.good()) << "Unable to load file " << filename;

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());
  CobaltExtensionDemuxerDataSource c_data_source{
      &FstreamBlockingRead, &FstreamSeekTo, &FstreamGetPosition,
      &FstreamGetSize,      kIsStreaming,   &input};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(demuxer, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);

  CobaltExtensionDemuxerVideoDecoderConfig actual_config = {};
  demuxer->GetVideoConfig(&actual_config, demuxer->user_data);

  EXPECT_EQ(actual_config.codec, kCobaltExtensionDemuxerCodecVP8);
  EXPECT_EQ(actual_config.profile, kCobaltExtensionDemuxerVp8ProfileMin);
  EXPECT_EQ(actual_config.color_space_primaries, 2);
  EXPECT_EQ(actual_config.color_space_transfer, 2);
  EXPECT_EQ(actual_config.color_space_matrix, 2);
  EXPECT_EQ(actual_config.color_space_range_id,
            kCobaltExtensionDemuxerColorSpaceRangeIdLimited);
  EXPECT_EQ(actual_config.alpha_mode, kCobaltExtensionDemuxerHasAlpha);
  EXPECT_EQ(actual_config.coded_width, 320);
  EXPECT_EQ(actual_config.coded_height, 240);
  EXPECT_EQ(actual_config.visible_rect_x, 0);
  EXPECT_EQ(actual_config.visible_rect_y, 0);
  EXPECT_EQ(actual_config.visible_rect_width, 320);
  EXPECT_EQ(actual_config.visible_rect_height, 240);
  EXPECT_EQ(actual_config.natural_width, 320);
  EXPECT_EQ(actual_config.natural_height, 240);
  EXPECT_EQ(actual_config.encryption_scheme,
            kCobaltExtensionDemuxerEncryptionSchemeUnencrypted);
  EXPECT_THAT(actual_config.extra_data, IsNull());
  EXPECT_EQ(actual_config.extra_data_size, 0);

  api->DestroyDemuxer(demuxer);
}

TEST(FFmpegDemuxerTest, ReturnsVideoDuration) {
  const std::string filename = ResolveTestFilename(kInputWebm);
  std::ifstream input(filename, std::ios_base::binary);
  SB_CHECK(input.good()) << "Unable to load file " << filename;

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());
  CobaltExtensionDemuxerDataSource c_data_source{
      &FstreamBlockingRead, &FstreamSeekTo, &FstreamGetPosition,
      &FstreamGetSize,      kIsStreaming,   &input};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(demuxer, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);
  EXPECT_EQ(demuxer->GetDuration(demuxer->user_data), 2744000);

  api->DestroyDemuxer(demuxer);
}

TEST(FFmpegDemuxerTest, TimelineOffsetReturnsZero) {
  const std::string filename = ResolveTestFilename(kInputWebm);
  std::ifstream input(filename, std::ios_base::binary);
  SB_CHECK(input.good()) << "Unable to load file " << filename;

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());
  CobaltExtensionDemuxerDataSource c_data_source{
      &FstreamBlockingRead, &FstreamSeekTo, &FstreamGetPosition,
      &FstreamGetSize,      kIsStreaming,   &input};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(demuxer, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);
  EXPECT_EQ(demuxer->GetTimelineOffset(demuxer->user_data), 0);

  api->DestroyDemuxer(demuxer);
}

TEST(FFmpegDemuxerTest, StartTimeReturnsZero) {
  const std::string filename = ResolveTestFilename(kInputWebm);
  std::ifstream input(filename, std::ios_base::binary);
  SB_CHECK(input.good()) << "Unable to load file " << filename;

  const CobaltExtensionDemuxerApi* api = GetFFmpegDemuxerApi();
  ASSERT_THAT(api, NotNull());
  CobaltExtensionDemuxerDataSource c_data_source{
      &FstreamBlockingRead, &FstreamSeekTo, &FstreamGetPosition,
      &FstreamGetSize,      kIsStreaming,   &input};
  std::vector<CobaltExtensionDemuxerAudioCodec> supported_audio_codecs;
  std::vector<CobaltExtensionDemuxerVideoCodec> supported_video_codecs;

  CobaltExtensionDemuxer* demuxer = api->CreateDemuxer(
      &c_data_source, supported_audio_codecs.data(),
      supported_audio_codecs.size(), supported_video_codecs.data(),
      supported_video_codecs.size());

  ASSERT_THAT(demuxer, NotNull());
  EXPECT_EQ(demuxer->Initialize(demuxer->user_data), kCobaltExtensionDemuxerOk);
  EXPECT_EQ(demuxer->GetStartTime(demuxer->user_data), 0);

  api->DestroyDemuxer(demuxer);
}

}  // namespace
}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
