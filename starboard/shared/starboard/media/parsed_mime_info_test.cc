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

#include "starboard/shared/starboard/media/parsed_mime_info.h"

#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::shared::starboard::media {
namespace {

const bool kCheckAc3Audio = true;

TEST(ParsedMimeInfoTest, ParsesAacLowComplexityCodec) {
  ParsedMimeInfo mime_info("audio/mp4; codecs=\"mp4a.40.2\"");
  ASSERT_TRUE(mime_info.has_audio_info());
  EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecAac);
}

TEST(ParsedMimeInfoTest, ParsesAacHighEfficiencyCodec) {
  ParsedMimeInfo mime_info("audio/mp4; codecs=\"mp4a.40.5\"");
  ASSERT_TRUE(mime_info.has_audio_info());
  EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecAac);
}

TEST(ParsedMimeInfoTest, ParsesAc3Codec) {
  ParsedMimeInfo mime_info("audio/mp4; codecs=\"ac-3\"");
  ASSERT_EQ(mime_info.has_audio_info(), kCheckAc3Audio);

  if (kCheckAc3Audio) {
    EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecAc3);
  }
}

TEST(ParsedMimeInfoTest, ParsesEac3Codec) {
  ParsedMimeInfo mime_info("audio/mp4; codecs=\"ec-3\"");
  ASSERT_EQ(mime_info.has_audio_info(), kCheckAc3Audio);

  if (kCheckAc3Audio) {
    EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecEac3);
  }
}

TEST(ParsedMimeInfoTest, ParsesOpusCodec) {
  ParsedMimeInfo mime_info("audio/webm; codecs=\"opus\"");
  ASSERT_TRUE(mime_info.has_audio_info());
  EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecOpus);
}

TEST(ParsedMimeInfoTest, ParsesVorbisCodec) {
  ParsedMimeInfo mime_info("audio/webm; codecs=\"vorbis\"");
  ASSERT_TRUE(mime_info.has_audio_info());
  EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecVorbis);
}

TEST(ParsedMimeInfoTest, ParsesMp3Codec) {
  ParsedMimeInfo mime_info("audio/mpeg; codecs=\"mp3\"");
  ASSERT_TRUE(mime_info.has_audio_info());
  EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecMp3);
}

TEST(ParsedMimeInfoTest, ParsesFlacCodec) {
  ParsedMimeInfo mime_info("audio/ogg; codecs=\"flac\"");
  ASSERT_TRUE(mime_info.has_audio_info());
  EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecFlac);
}

TEST(ParsedMimeInfoTest, ParsesPcmCodec) {
  ParsedMimeInfo mime_info("audio/wav; codecs=\"1\"");
  ASSERT_TRUE(mime_info.has_audio_info());
  EXPECT_EQ(mime_info.audio_info().codec, kSbMediaAudioCodecPcm);
}

}  // namespace

}  // namespace starboard::shared::starboard::media
