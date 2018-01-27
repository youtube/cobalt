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

#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"

#include <functional>
#include <set>

#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink_impl.h"
#include "starboard/shared/starboard/player/filter/mock_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/mock_audio_renderer_sink.h"
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
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;

class AudioRendererTest : public ::testing::Test {
 protected:
  static const int kDefaultNumberOfChannels = 2;
  static const int kDefaultSamplesPerSecond = 100000;
  static const SbMediaAudioSampleType kDefaultAudioSampleType =
      kSbMediaAudioSampleTypeFloat32;
  static const SbMediaAudioFrameStorageType kDefaultAudioFrameStorageType =
      kSbMediaAudioFrameStorageTypeInterleaved;

  AudioRendererTest() {
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
    audio_renderer_sink_ = new ::testing::StrictMock<MockAudioRendererSink>;
    audio_decoder_ = new MockAudioDecoder(sample_type_, storage_type_,
                                          kDefaultSamplesPerSecond);

    ON_CALL(*audio_renderer_sink_, Start(_, _, _, _, _, _, _))
        .WillByDefault(DoAll(InvokeWithoutArgs([this]() {
                               audio_renderer_sink_->SetHasStarted(true);
                             }),
                             SaveArg<6>(&renderer_callback_)));
    ON_CALL(*audio_renderer_sink_, Stop())
        .WillByDefault(InvokeWithoutArgs(
            [this]() { audio_renderer_sink_->SetHasStarted(false); }));

    ON_CALL(*audio_renderer_sink_, HasStarted())
        .WillByDefault(::testing::ReturnPointee(
            audio_renderer_sink_->HasStartedPointer()));
    EXPECT_CALL(*audio_renderer_sink_, HasStarted()).Times(AnyNumber());

    // This allows audio renderers to query different sample types, and only
    // same type of float32 will be returned as supported.
    ON_CALL(*audio_renderer_sink_,
            IsAudioSampleTypeSupported(kSbMediaAudioSampleTypeFloat32))
        .WillByDefault(Return(true));
    EXPECT_CALL(*audio_renderer_sink_, IsAudioSampleTypeSupported(_))
        .Times(AnyNumber());

    // This allows audio renderers to query different sample types, and only
    // sample frequency of 100Khz will be supported.
    const int kSupportedSampleFrequency = 100000;
    ON_CALL(*audio_renderer_sink_,
            GetNearestSupportedSampleFrequency(kSupportedSampleFrequency))
        .WillByDefault(Return(kSupportedSampleFrequency));
    EXPECT_CALL(*audio_renderer_sink_, GetNearestSupportedSampleFrequency(_))
        .Times(AnyNumber());

    EXPECT_CALL(*audio_decoder_, Initialize(_, _))
        .WillOnce(SaveArg<0>(&output_cb_));

    audio_renderer_.reset(new AudioRenderer(
        make_scoped_ptr<AudioDecoder>(audio_decoder_),
        make_scoped_ptr<AudioRendererSink>(audio_renderer_sink_),
        GetDefaultAudioHeader()));
    audio_renderer_->Initialize(std::bind(&AudioRendererTest::OnError, this));
  }

  // Creates audio buffers, decodes them, and passes them onto the renderer,
  // until the renderer reaches its preroll threshold.
  // Once the renderer is "full", an EndOfStream is written.
  // Returns the number of frames written.
  int FillRendererWithDecodedAudioAndWriteEOS() {
    const int kFramesPerBuffer = 1024;

    int frames_written = 0;

    while (audio_renderer_->IsSeekingInProgress()) {
      SbMediaTime pts =
          frames_written * kSbMediaTimeSecond / kDefaultSamplesPerSecond;
      scoped_refptr<InputBuffer> input_buffer = CreateInputBuffer(pts);
      WriteSample(input_buffer);
      CallConsumedCB();
      scoped_refptr<DecodedAudio> decoded_audio =
          CreateDecodedAudio(pts, kFramesPerBuffer);
      SendDecoderOutput(decoded_audio);
      frames_written += kFramesPerBuffer;
    }

    WriteEndOfStream();

    return frames_written;
  }

  void WriteSample(const scoped_refptr<InputBuffer>& input_buffer) {
    ASSERT_TRUE(input_buffer != NULL);
    ASSERT_FALSE(consumed_cb_);

    buffers_in_decoder_.insert(input_buffer->data());
    EXPECT_CALL(*audio_decoder_, Decode(input_buffer, _))
        .WillOnce(SaveArg<1>(&consumed_cb_));
    audio_renderer_->WriteSample(input_buffer);
    job_queue_.RunUntilIdle();

    ASSERT_TRUE(consumed_cb_);
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
    ASSERT_TRUE(consumed_cb_);
    consumed_cb_();
    consumed_cb_ = nullptr;
    job_queue_.RunUntilIdle();
  }

  void SendDecoderOutput(const scoped_refptr<DecodedAudio>& decoded_audio) {
    ASSERT_TRUE(output_cb_);

    EXPECT_CALL(*audio_decoder_, Read()).WillOnce(Return(decoded_audio));
    output_cb_();
    job_queue_.RunUntilIdle();
  }

  scoped_refptr<InputBuffer> CreateInputBuffer(SbMediaTime pts) {
    const int kInputBufferSize = 4;
    return new InputBuffer(kSbMediaTypeAudio, DeallocateSampleCB, NULL, this,
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

  void OnError() {}

  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType storage_type_;

  JobQueue job_queue_;
  std::set<const void*> buffers_in_decoder_;

  AudioDecoder::OutputCB output_cb_;
  AudioDecoder::ConsumedCB consumed_cb_;

  scoped_ptr<AudioRenderer> audio_renderer_;
  MockAudioDecoder* audio_decoder_;
  MockAudioRendererSink* audio_renderer_sink_;
  AudioRendererSink::RenderCallback* renderer_callback_;

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
    AudioRendererTest* test = static_cast<AudioRendererTest*>(context);
    EXPECT_TRUE(test != NULL);
    test->OnDeallocateSample(sample_buffer);
  }
};

TEST_F(AudioRendererTest, StateAfterConstructed) {
  EXPECT_FALSE(audio_renderer_->IsEndOfStreamWritten());
  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());
  EXPECT_TRUE(audio_renderer_->CanAcceptMoreData());
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());
  bool is_playing = true;
  bool is_eos_played = true;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
}

TEST_F(AudioRendererTest, SunnyDay) {
  {
    InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  int frames_written = FillRendererWithDecodedAudioAndWriteEOS();

  bool is_playing = true;
  bool is_eos_played = true;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);

  int frames_in_buffer;
  int offset_in_frames;
  bool is_eos_reached;
  renderer_callback_->GetSourceStatus(&frames_in_buffer, &offset_in_frames,
                                      &is_playing, &is_eos_reached);
  EXPECT_GT(frames_in_buffer, 0);
  EXPECT_GE(offset_in_frames, 0);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_reached);

  // Consume frames in two batches, so we can test if |GetCurrentMediaTime()|
  // is incrementing in an expected manner.
  const int frames_to_consume = frames_in_buffer / 4;
  SbMediaTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_GT(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_played);
  EXPECT_GT(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererTest, SunnyDayWithDoublePlaybackRateAndInt16Samples) {
  const int kPlaybackRate = 2;

  // Resets |audio_renderer_sink_|, so all the gtest codes need to be below
  // this line.
  ResetToFormat(kSbMediaAudioSampleTypeInt16,
                kSbMediaAudioFrameStorageTypeInterleaved);

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  // It is OK to set the rate to 1.0 any number of times.
  EXPECT_CALL(*audio_renderer_sink_, SetPlaybackRate(1.0)).Times(AnyNumber());
  audio_renderer_->SetPlaybackRate(static_cast<float>(kPlaybackRate));

  Seek(0);

  int frames_written = FillRendererWithDecodedAudioAndWriteEOS();
  bool is_playing = false;
  bool is_eos_played = true;

  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);

  int frames_in_buffer;
  int offset_in_frames;
  bool is_eos_reached;
  renderer_callback_->GetSourceStatus(&frames_in_buffer, &offset_in_frames,
                                      &is_playing, &is_eos_reached);

  EXPECT_GT(frames_in_buffer, 0);
  EXPECT_GE(offset_in_frames, 0);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_reached);

  // Consume frames in two batches, so we can test if |GetCurrentMediaTime()|
  // is incrementing in an expected manner.
  const int frames_to_consume = frames_in_buffer / 4;
  SbMediaTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_GT(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_GT(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererTest, StartPlayBeforePreroll) {
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  audio_renderer_->Play();

  int frames_written = FillRendererWithDecodedAudioAndWriteEOS();

  SendDecoderOutput(new DecodedAudio);

  bool is_playing = false;
  bool is_eos_played = true;
  SbMediaTime media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);

  int frames_in_buffer;
  int offset_in_frames;
  bool is_eos_reached;
  renderer_callback_->GetSourceStatus(&frames_in_buffer, &offset_in_frames,
                                      &is_playing, &is_eos_reached);
  EXPECT_GT(frames_in_buffer, 0);
  EXPECT_GE(offset_in_frames, 0);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_reached);

  // Consume frames in two batches, so we can test if |GetCurrentMediaTime()|
  // is incrementing in an expected manner.
  const int frames_to_consume = frames_in_buffer / 4;
  SbMediaTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_GE(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_played);
  EXPECT_GE(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererTest, DecoderReturnsEOSWithoutAnyData) {
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

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
  bool is_playing = true;
  bool is_eos_played = false;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_TRUE(is_eos_played);
}

// Test decoders that take many input samples before returning any output.
TEST_F(AudioRendererTest, DecoderConsumeAllInputBeforeReturningData) {
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

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
  bool is_playing = true;
  bool is_eos_played = false;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_TRUE(is_eos_played);
}

TEST_F(AudioRendererTest, MoreNumberOfOuputBuffersThanInputBuffers) {
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;

  while (audio_renderer_->IsSeekingInProgress()) {
    SbMediaTime pts =
        frames_written * kSbMediaTimeSecond / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(pts));
    CallConsumedCB();
    SendDecoderOutput(CreateDecodedAudio(pts, kFramesPerBuffer / 2));
    frames_written += kFramesPerBuffer / 2;
    pts = frames_written * kSbMediaTimeSecond / kDefaultSamplesPerSecond;
    SendDecoderOutput(CreateDecodedAudio(pts, kFramesPerBuffer / 2));
    frames_written += kFramesPerBuffer / 2;
  }

  WriteEndOfStream();

  bool is_playing = true;
  bool is_eos_played = true;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);

  int frames_in_buffer;
  int offset_in_frames;
  bool is_eos_reached;
  renderer_callback_->GetSourceStatus(&frames_in_buffer, &offset_in_frames,
                                      &is_playing, &is_eos_reached);
  EXPECT_GT(frames_in_buffer, 0);
  EXPECT_GE(offset_in_frames, 0);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_TRUE(is_eos_reached);

  // Consume frames in two batches, so we can test if |GetCurrentMediaTime()|
  // is incrementing in an expected manner.
  const int frames_to_consume = frames_in_buffer / 4;
  SbMediaTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_GE(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_played);
  EXPECT_GE(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererTest, LessNumberOfOuputBuffersThanInputBuffers) {
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(*audio_renderer_sink_, HasStarted())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _))
        .WillOnce(SaveArg<6>(&renderer_callback_));
    EXPECT_CALL(*audio_renderer_sink_, HasStarted())
        .WillRepeatedly(Return(true));
  }

  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;

  while (audio_renderer_->IsSeekingInProgress()) {
    SbMediaTime pts =
        frames_written * kSbMediaTimeSecond / kDefaultSamplesPerSecond;
    SbMediaTime output_pts = pts;
    WriteSample(CreateInputBuffer(pts));
    CallConsumedCB();
    frames_written += kFramesPerBuffer / 2;
    pts = frames_written * kSbMediaTimeSecond / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(pts));
    CallConsumedCB();
    frames_written += kFramesPerBuffer / 2;
    SendDecoderOutput(CreateDecodedAudio(output_pts, kFramesPerBuffer));
  }

  WriteEndOfStream();

  bool is_playing;
  bool is_eos_played;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            0);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);

  int frames_in_buffer;
  int offset_in_frames;
  bool is_eos_reached;
  renderer_callback_->GetSourceStatus(&frames_in_buffer, &offset_in_frames,
                                      &is_playing, &is_eos_reached);
  EXPECT_GT(frames_in_buffer, 0);
  EXPECT_GE(offset_in_frames, 0);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_reached);

  // Consume frames in two batches, so we can test if |GetCurrentMediaTime()|
  // is incrementing in an expected manner.
  const int frames_to_consume = frames_in_buffer / 4;
  SbMediaTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_GE(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_GE(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererTest, Seek) {
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
    EXPECT_CALL(*audio_renderer_sink_, Stop());
    EXPECT_CALL(*audio_decoder_, Reset());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  int frames_written = FillRendererWithDecodedAudioAndWriteEOS();

  bool is_playing;
  bool is_eos_played;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            0);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbMediaTime media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);

  int frames_in_buffer;
  int offset_in_frames;
  bool is_eos_reached;
  renderer_callback_->GetSourceStatus(&frames_in_buffer, &offset_in_frames,
                                      &is_playing, &is_eos_reached);
  EXPECT_GT(frames_in_buffer, 0);
  EXPECT_GE(offset_in_frames, 0);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_reached);

  // Consume frames in multiple batches, so we can test if
  // |GetCurrentMediaTime()| is incrementing in an expected manner.
  const double seek_time = 0.5 * kSbMediaTimeSecond;
  const int frames_to_consume = frames_in_buffer / 10;
  SbMediaTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_GE(new_media_time, media_time);
  Seek(seek_time);

  frames_written = FillRendererWithDecodedAudioAndWriteEOS();

  EXPECT_GE(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played),
            seek_time);
  EXPECT_FALSE(audio_renderer_->IsSeekingInProgress());

  audio_renderer_->Play();
  SendDecoderOutput(new DecodedAudio);

  renderer_callback_->GetSourceStatus(&frames_in_buffer, &offset_in_frames,
                                      &is_playing, &is_eos_reached);
  EXPECT_GT(frames_in_buffer, 0);
  EXPECT_GE(offset_in_frames, 0);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_reached);
  const int remaining_frames = frames_in_buffer - offset_in_frames;
  renderer_callback_->ConsumeFrames(remaining_frames);
  new_media_time =
      audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played);
  EXPECT_GE(new_media_time, seek_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

// TODO: Add more Seek tests.

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
