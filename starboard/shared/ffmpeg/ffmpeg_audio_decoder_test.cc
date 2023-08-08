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

#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"

#include <memory>

#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace ffmpeg {
namespace {

// The codecs tested by these tests were introduced in SB_API_VERSION 14.
#if SB_API_VERSION >= 14
using ::starboard::shared::starboard::media::AudioStreamInfo;
using ::testing::NotNull;

AudioStreamInfo CreateStreamInfoForCodec(SbMediaAudioCodec codec) {
  AudioStreamInfo stream_info;
  stream_info.codec = codec;
  stream_info.number_of_channels = 2;
  stream_info.samples_per_second = 44100;
  stream_info.bits_per_sample = 8;
  return stream_info;
}

class FFmpegAudioDecoderTest
    : public ::testing::Test,
      public ::starboard::shared::starboard::player::JobQueue::JobOwner {
 protected:
  FFmpegAudioDecoderTest() : JobOwner(kDetached) { AttachToCurrentThread(); }

  ~FFmpegAudioDecoderTest() override = default;

  // Create a JobQueue for use on the current thread.
  ::starboard::shared::starboard::player::JobQueue job_queue_;
};

TEST_F(FFmpegAudioDecoderTest, SupportsMp3Codec) {
  AudioStreamInfo stream_info = CreateStreamInfoForCodec(kSbMediaAudioCodecMp3);
  std::unique_ptr<AudioDecoder> decoder(AudioDecoder::Create(stream_info));
  ASSERT_THAT(decoder, NotNull());
  EXPECT_TRUE(decoder->is_valid());
}

TEST_F(FFmpegAudioDecoderTest, SupportsFlacCodecFor16BitAudio) {
  AudioStreamInfo stream_info =
      CreateStreamInfoForCodec(kSbMediaAudioCodecFlac);
  stream_info.bits_per_sample = 16;
  std::unique_ptr<AudioDecoder> decoder(AudioDecoder::Create(stream_info));
  ASSERT_THAT(decoder, NotNull());
  EXPECT_TRUE(decoder->is_valid());
}

TEST_F(FFmpegAudioDecoderTest, SupportsPcmCodecFor16BitAudio) {
  AudioStreamInfo stream_info = CreateStreamInfoForCodec(kSbMediaAudioCodecPcm);
  stream_info.bits_per_sample = 16;
  std::unique_ptr<AudioDecoder> decoder(AudioDecoder::Create(stream_info));
  ASSERT_THAT(decoder, NotNull());
  EXPECT_TRUE(decoder->is_valid());
}
#endif  // SB_API_VERSION >= 14

}  // namespace
}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
