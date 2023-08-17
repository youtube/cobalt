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

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

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
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
