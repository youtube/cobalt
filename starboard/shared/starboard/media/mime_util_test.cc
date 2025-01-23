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

#include "starboard/shared/starboard/media/mime_util.h"

#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

// The codecs tested by these tests were introduced in SB_API_VERSION 14.
constexpr char kEmptyKeySystem[] = "";
constexpr int64_t kBitrate = 44100;

TEST(MimeUtilTest, ChecksSupportedMp3Containers) {
  const std::string valid_mp3_mime_str_1 =
      "audio/mpeg; codecs=\"mp3\"; bitrate=44100";
  const MimeType valid_mp3_mime_1(valid_mp3_mime_str_1);
  EXPECT_EQ(
      CanPlayMimeAndKeySystem(valid_mp3_mime_str_1.c_str(), kEmptyKeySystem),
      SbMediaIsAudioSupported(kSbMediaAudioCodecMp3, &valid_mp3_mime_1,
                              kBitrate)
          ? kSbMediaSupportTypeProbably
          : kSbMediaSupportTypeNotSupported);

  const std::string valid_mp3_mime_str_2 =
      "audio/mp3; codecs=\"mp3\"; bitrate=44100";
  const MimeType valid_mp3_mime_2(valid_mp3_mime_str_2);
  EXPECT_EQ(
      CanPlayMimeAndKeySystem(valid_mp3_mime_str_2.c_str(), kEmptyKeySystem),
      SbMediaIsAudioSupported(kSbMediaAudioCodecMp3, &valid_mp3_mime_2,
                              kBitrate)
          ? kSbMediaSupportTypeProbably
          : kSbMediaSupportTypeNotSupported);
}

TEST(MimeUtilTest, ChecksUnsupportedMp3Containers) {
  // Invalid container for MP3 codec.
  const std::string invalid_mp3_mime_str =
      "audio/mp4; codecs=\"mp3\"; bitrate=44100";
  const MimeType invalid_mp3_mime(invalid_mp3_mime_str);
  EXPECT_EQ(
      CanPlayMimeAndKeySystem(invalid_mp3_mime_str.c_str(), kEmptyKeySystem),
      kSbMediaSupportTypeNotSupported);
}

TEST(MimeUtilTest, ChecksSupportedFlacContainers) {
  const std::string valid_flac_mime_str_1 =
      "audio/ogg; codecs=\"flac\"; bitrate=44100";
  const MimeType valid_flac_mime_1(valid_flac_mime_str_1);
  EXPECT_EQ(
      CanPlayMimeAndKeySystem(valid_flac_mime_str_1.c_str(), kEmptyKeySystem),
      SbMediaIsAudioSupported(kSbMediaAudioCodecMp3, &valid_flac_mime_1,
                              kBitrate)
          ? kSbMediaSupportTypeProbably
          : kSbMediaSupportTypeNotSupported);

  const std::string valid_flac_mime_str_2 =
      "audio/flac; codecs=\"flac\"; bitrate=44100";
  const MimeType valid_flac_mime_2(valid_flac_mime_str_2);
  EXPECT_EQ(
      CanPlayMimeAndKeySystem(valid_flac_mime_str_2.c_str(), kEmptyKeySystem),
      SbMediaIsAudioSupported(kSbMediaAudioCodecMp3, &valid_flac_mime_2,
                              kBitrate)
          ? kSbMediaSupportTypeProbably
          : kSbMediaSupportTypeNotSupported);
}

TEST(MimeUtilTest, ChecksUnsupportedFlacContainers) {
  // Invalid container for FLAC codec.
  const std::string invalid_flac_mime_str =
      "audio/mp4; codecs=\"flac\"; bitrate=44100";
  const MimeType invalid_flac_mime(invalid_flac_mime_str);
  EXPECT_EQ(
      CanPlayMimeAndKeySystem(invalid_flac_mime_str.c_str(), kEmptyKeySystem),
      kSbMediaSupportTypeNotSupported);
}

TEST(MimeUtilTest, ChecksSupportedPcmContainers) {
  const std::string valid_pcm_mime_str =
      "audio/wav; codecs=\"1\"; bitrate=44100";
  const MimeType valid_pcm_mime(valid_pcm_mime_str);
  EXPECT_EQ(
      CanPlayMimeAndKeySystem(valid_pcm_mime_str.c_str(), kEmptyKeySystem),
      SbMediaIsAudioSupported(kSbMediaAudioCodecPcm, &valid_pcm_mime, kBitrate)
          ? kSbMediaSupportTypeProbably
          : kSbMediaSupportTypeNotSupported);
}

TEST(MimeUtilTest, ChecksUnsupportedWavCodecs) {
  const std::string invalid_wav_mime_str =
      "audio/wav; codecs=\"aac\"; bitrate=44100";
  const MimeType invalid_wav_mime(invalid_wav_mime_str);
  EXPECT_EQ(
      CanPlayMimeAndKeySystem(invalid_wav_mime_str.c_str(), kEmptyKeySystem),
      kSbMediaSupportTypeNotSupported);
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
