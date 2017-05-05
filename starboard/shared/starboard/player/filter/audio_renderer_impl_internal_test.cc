// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_renderer_impl_internal.h"

#include <set>

#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/mock_audio_decoder.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

// TODO: Inject and test the renderer using SbAudioSink mock.  Otherwise the
// tests rely on a correctly implemented audio sink and may fail on other audio
// sinks.

class AudioRendererImplTest : public ::testing::Test {
 protected:
  static const int kDefaultNumberOfChannels = 2;
  static const int kDefaultSamplesPerSecond = 100000;
  static const SbMediaAudioSampleType kDefaultAudioSampleType =
      kSbMediaAudioSampleTypeFloat32;
  static const SbMediaAudioFrameStorageType kDefaultAudioFrameStorageType =
      kSbMediaAudioFrameStorageTypeInterleaved;

  AudioRendererImplTest() {
    ResetToFormat(kSbMediaAudioSampleTypeFloat32,
                  kSbMediaAudioFrameStorageTypeInterleaved);
  }

  // This function should be called in the fixture before any other functions
  // to set the desired format of the decoder.
  void ResetToFormat(SbMediaAudioSampleType sample_type,
                     SbMediaAudioFrameStorageType storage_type) {
    audio_renderer_.reset(NULL);
    sample_type_ = sample_type;
    storage_type_ = storage_type;
    audio_decoder_ = new MockAudioDecoder(sample_type_, storage_type_,
                                          kDefaultSamplesPerSecond);
    EXPECT_CALL(*audio_decoder_, Initialize(_))
        .WillOnce(SaveArg<0>(&output_cb_));
    audio_renderer_.reset(
        new AudioRendererImpl(make_scoped_ptr<AudioDecoder>(audio_decoder_),
                              GetDefaultAudioHeader()));
  }

  void WriteSample(InputBuffer input_buffer) {
    ASSERT_TRUE(input_buffer.is_valid());
    ASSERT_FALSE(consumed_cb_.is_valid());

    buffers_in_decoder_.insert(input_buffer.data());
    EXPECT_CALL(*audio_decoder_, Decode(input_buffer, _))
        .WillOnce(SaveArg<1>(&consumed_cb_));
    audio_renderer_->WriteSample(input_buffer);
    job_queue_.RunUntilIdle();

    ASSERT_TRUE(consumed_cb_.is_valid());
  }

  void WriteEndOfStream() {
    EXPECT_CALL(*audio_decoder_, WriteEndOfStream());
    audio_renderer_->WriteEndOfStream();
    job_queue_.RunUntilIdle();
  }

  void Seek(SbMediaTime seek_to_pts) {
    audio_renderer_->Seek(seek_to_pts);
    job_queue_.RunUntilIdle();
    EXPECT_TRUE(audio_renderer_->IsSeekingInProgress());
  }

  void CallConsumedCB() {
    ASSERT_TRUE(consumed_cb_.is_valid());
    consumed_cb_.Run();
    consumed_cb_.reset();
    job_queue_.RunUntilIdle();
  }

  void SendDecoderOutput(const scoped_refptr<DecodedAudio>& decoded_audio) {
    ASSERT_TRUE(output_cb_.is_valid());

    EXPECT_CALL(*audio_decoder_, Read()).WillOnce(Return(decoded_audio));
    output_cb_.Run();
    job_queue_.RunUntilIdle();
  }

  InputBuffer CreateInputBuffer(SbMediaTime pts) {
    const int kInputBufferSize = 4;
    return InputBuffer(kSbMediaTypeAudio, DeallocateSampleCB, NULL, this,
                       SbMemoryAllocate(kInputBufferSize), kInputBufferSize,
                       pts, NULL, NULL);
  }

  scoped_refptr<DecodedAudio> CreateDecodedAudio(SbMediaTime pts, int frames) {
    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        kDefaultNumberOfChannels, sample_type_, storage_type_, pts,
        frames * kDefaultNumberOfChannels *
            media::GetBytesPerSample(sample_type_));
    SbMemorySet(decoded_audio->buffer(), 0, decoded_audio->size());
    return decoded_audio;
  }

  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType storage_type_;

  JobQueue job_queue_;
  std::set<const void*> buffers_in_decoder_;

  Closure output_cb_;
  Closure consumed_cb_;

  scoped_ptr<AudioRenderer> audio_renderer_;
  MockAudioDecoder* audio_decoder_;

 private:
  void OnDeallocateSample(const void* sample_buffer) {
    ASSERT_TRUE(buffers_in_decoder_.find(sample_buffer) !=
                buffers_in_decoder_.end());
    buffers_in_decoder_.erase(buffers_in_decoder_.find(sample_buffer));
    SbMemoryDeallocate(const_cast<void*>(sample_buffer));
  }

  static SbMediaAudioHeader GetDefaultAudioHeader() {
    SbMediaAudioHeader audio_header = {};

    audio_header.number_of_channels = kDefaultNumberOfChannels;
    audio_header.samples_per_second = kDefaultSamplesPerSecond;
    audio_header.bits_per_sample = 32;
    audio_header.average_bytes_per_second = audio_header.samples_per_second *
                                            audio_header.number_of_channels *
                                            audio_header.bits_per_sample / 8;
    audio_header.block_alignment = 4;
    audio_header.audio_specific_config_size = 0;

    return audio_header;
  }

  static void DeallocateSampleCB(SbPlayer player,
                                 void* context,
                                 const void* sample_buffer) {
    AudioRendererImplTest* test = static_cast<AudioRendererImplTest*>(context);
    EXPECT_TRUE(test != NULL);
    test->OnDeallocateSample(sample_buffer);
  }
};

TEST_F(AudioRendererImplTest, StateAfterConstructed) {
  EXPECT_FALSE(audio_renderer_->IsEndOfStreamWritten());
  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());
  EXPECT_TRUE(audio_renderer_->CanAcceptMoreData());
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());
  EXPECT_EQ(audio_renderer_->GetCurrentTime(), 0);
}

TEST_F(AudioRendererImplTest, SunnyDay) {
  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;
  int preroll_frames = kDefaultSamplesPerSecond *
                       AudioRendererImpl::kPrerollTime / kSbTimeSecond;

  while (frames_written <= preroll_frames) {
    SbMediaTime pts = frames_written / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(pts));
    CallConsumedCB();
    SendDecoderOutput(CreateDecodedAudio(pts, kFramesPerBuffer));
    frames_written += kFramesPerBuffer;
  }

  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  WriteEndOfStream();

  EXPECT_EQ(audio_renderer_->GetCurrentTime(), 0);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time = audio_renderer_->GetCurrentTime();

  while (!audio_renderer_->IsEndOfStreamPlayed()) {
    SbThreadSleep(AudioRendererImpl::kPrerollTime);
    SbMediaTime new_media_time = audio_renderer_->GetCurrentTime();
    EXPECT_GT(new_media_time, media_time);
    media_time = new_media_time;
  }
}

TEST_F(AudioRendererImplTest, SunnyDayWithDoublePlaybackRateAndInt16Samples) {
  const int kPlaybackRate = 2;

  ResetToFormat(kSbMediaAudioSampleTypeInt16,
                kSbMediaAudioFrameStorageTypeInterleaved);
  audio_renderer_->SetPlaybackRate(static_cast<float>(kPlaybackRate));

  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;
  int preroll_frames = kDefaultSamplesPerSecond *
                       AudioRendererImpl::kPrerollTime / kSbTimeSecond *
                       kPlaybackRate * 2;

  while (frames_written <= preroll_frames) {
    SbMediaTime pts = frames_written / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(pts));
    CallConsumedCB();
    SendDecoderOutput(CreateDecodedAudio(pts, kFramesPerBuffer));
    frames_written += kFramesPerBuffer;

    if (!audio_renderer_->IsSeekingInProgress()) {
      break;
    }
  }

  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  WriteEndOfStream();

  EXPECT_EQ(audio_renderer_->GetCurrentTime(), 0);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time = audio_renderer_->GetCurrentTime();

  while (!audio_renderer_->IsEndOfStreamPlayed()) {
    SbThreadSleep(AudioRendererImpl::kPrerollTime);
    SbMediaTime new_media_time = audio_renderer_->GetCurrentTime();
    EXPECT_GT(new_media_time, media_time);
    media_time = new_media_time;
  }
}

TEST_F(AudioRendererImplTest, StartPlayBeforePreroll) {
  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;
  int preroll_frames = kDefaultSamplesPerSecond *
                       AudioRendererImpl::kPrerollTime / kSbTimeSecond;

  audio_renderer_->Play();

  while (frames_written <= preroll_frames) {
    SbMediaTime pts = frames_written / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(pts));
    CallConsumedCB();
    SendDecoderOutput(CreateDecodedAudio(pts, kFramesPerBuffer));
    frames_written += kFramesPerBuffer;
  }

  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  WriteEndOfStream();
  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time = audio_renderer_->GetCurrentTime();

  while (!audio_renderer_->IsEndOfStreamPlayed()) {
    SbThreadSleep(AudioRendererImpl::kPrerollTime);
    SbMediaTime new_media_time = audio_renderer_->GetCurrentTime();
    EXPECT_GT(new_media_time, media_time);
    media_time = new_media_time;
  }
}

TEST_F(AudioRendererImplTest, DecoderReturnsEOSWithoutAnyData) {
  Seek(0);

  WriteSample(CreateInputBuffer(0));
  CallConsumedCB();

  WriteEndOfStream();

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamWritten());
  EXPECT_FALSE(audio_renderer_->CanAcceptMoreData());
  EXPECT_TRUE(audio_renderer_->IsSeekingInProgress());

  // Return EOS from decoder without sending any audio data, which is valid.
  SendDecoderOutput(new DecodedAudio);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());
  EXPECT_EQ(audio_renderer_->GetCurrentTime(), 0);
}

// Test decoders that take many input samples before returning any output.
TEST_F(AudioRendererImplTest, DecoderConsumeAllInputBeforeReturningData) {
  Seek(0);

  for (int i = 0; i < 128; ++i) {
    WriteSample(CreateInputBuffer(i));
    CallConsumedCB();

    if (!audio_renderer_->CanAcceptMoreData()) {
      break;
    }
  }

  // Send an EOS to "force" the decoder return data.
  WriteEndOfStream();

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamWritten());
  EXPECT_FALSE(audio_renderer_->CanAcceptMoreData());
  EXPECT_TRUE(audio_renderer_->IsSeekingInProgress());

  // Return EOS from decoder without sending any audio data, which is valid.
  SendDecoderOutput(new DecodedAudio);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());
  EXPECT_EQ(audio_renderer_->GetCurrentTime(), 0);
}

TEST_F(AudioRendererImplTest, MoreNumberOfOuputBuffersThanInputBuffers) {
  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;
  int preroll_frames = kDefaultSamplesPerSecond *
                       AudioRendererImpl::kPrerollTime / kSbTimeSecond;

  while (frames_written <= preroll_frames) {
    SbMediaTime pts = frames_written / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(pts));
    CallConsumedCB();
    SendDecoderOutput(CreateDecodedAudio(pts, kFramesPerBuffer / 2));
    frames_written += kFramesPerBuffer / 2;
    pts = frames_written / kDefaultSamplesPerSecond;
    SendDecoderOutput(CreateDecodedAudio(pts, kFramesPerBuffer / 2));
    frames_written += kFramesPerBuffer / 2;
  }

  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  WriteEndOfStream();

  EXPECT_EQ(audio_renderer_->GetCurrentTime(), 0);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time = audio_renderer_->GetCurrentTime();

  while (!audio_renderer_->IsEndOfStreamPlayed()) {
    SbThreadSleep(AudioRendererImpl::kPrerollTime);
    SbMediaTime new_media_time = audio_renderer_->GetCurrentTime();
    EXPECT_GT(new_media_time, media_time);
    media_time = new_media_time;
  }
}

TEST_F(AudioRendererImplTest, LessNumberOfOuputBuffersThanInputBuffers) {
  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;
  int preroll_frames = kDefaultSamplesPerSecond *
                       AudioRendererImpl::kPrerollTime / kSbTimeSecond;

  while (frames_written <= preroll_frames) {
    SbMediaTime pts = frames_written / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(pts));
    CallConsumedCB();
    frames_written += kFramesPerBuffer;
  }

  SendDecoderOutput(CreateDecodedAudio(0, frames_written));

  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  WriteEndOfStream();

  EXPECT_EQ(audio_renderer_->GetCurrentTime(), 0);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time = audio_renderer_->GetCurrentTime();

  while (!audio_renderer_->IsEndOfStreamPlayed()) {
    SbThreadSleep(AudioRendererImpl::kPrerollTime);
    SbMediaTime new_media_time = audio_renderer_->GetCurrentTime();
    EXPECT_GT(new_media_time, media_time);
    media_time = new_media_time;
  }
}

TEST_F(AudioRendererImplTest, Seek) {}

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
