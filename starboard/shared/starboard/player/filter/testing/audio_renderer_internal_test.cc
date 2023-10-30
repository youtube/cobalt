// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_renderer_internal_pcm.h"

#include <functional>
#include <set>

#include "starboard/common/log.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
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
using ::testing::SetArgPointee;

// TODO: Write tests to cover callbacks.
class AudioRendererTest : public ::testing::Test {
 protected:
  static const int kDefaultNumberOfChannels = 2;
  static const int kDefaultSamplesPerSecond;
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

    ON_CALL(*audio_decoder_, Read(_))
        .WillByDefault(
            DoAll(SetArgPointee<0>(kDefaultSamplesPerSecond),
                  Return(scoped_refptr<DecodedAudio>(new DecodedAudio()))));
    ON_CALL(*audio_renderer_sink_, Start(_, _, _, _, _, _, _, _))
        .WillByDefault(DoAll(InvokeWithoutArgs([this]() {
                               audio_renderer_sink_->SetHasStarted(true);
                             }),
                             SaveArg<7>(&renderer_callback_)));
    ON_CALL(*audio_renderer_sink_, Stop())
        .WillByDefault(InvokeWithoutArgs([this]() {
          audio_renderer_sink_->SetHasStarted(false);
        }));  // NOLINT

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

    const int kMaxCachedFrames = 256 * 1024;
    const int kMaxFramesPerAppend = 16384;
    audio_renderer_.reset(new AudioRendererPcm(
        make_scoped_ptr<AudioDecoder>(audio_decoder_),
        make_scoped_ptr<AudioRendererSink>(audio_renderer_sink_),
        GetDefaultAudioStreamInfo(), kMaxCachedFrames, kMaxFramesPerAppend));
    audio_renderer_->Initialize(
        std::bind(&AudioRendererTest::OnError, this),
        std::bind(&AudioRendererTest::OnPrerolled, this),
        std::bind(&AudioRendererTest::OnEnded, this));
  }

  // Creates audio buffers, decodes them, and passes them onto the renderer,
  // until the renderer reaches its preroll threshold.
  // Once the renderer is "full", an EndOfStream is written.
  // Returns the number of frames written.
  int FillRendererWithDecodedAudioAndWriteEOS(SbTime start_timestamp) {
    const int kFramesPerBuffer = 1024;

    int frames_written = 0;

    while (!prerolled_) {
      SbTime timestamp = start_timestamp + frames_written * kSbTimeSecond /
                                               kDefaultSamplesPerSecond;
      scoped_refptr<InputBuffer> input_buffer = CreateInputBuffer(timestamp);
      WriteSample(input_buffer);
      CallConsumedCB();
      scoped_refptr<DecodedAudio> decoded_audio =
          CreateDecodedAudio(timestamp, kFramesPerBuffer);
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
    InputBuffers input_buffers;
    input_buffers.push_back(input_buffer);
    EXPECT_CALL(*audio_decoder_, Decode(input_buffers, _))
        .WillOnce(SaveArg<1>(&consumed_cb_));
    audio_renderer_->WriteSamples(input_buffers);
    job_queue_.RunUntilIdle();

    ASSERT_TRUE(consumed_cb_);
  }

  void WriteEndOfStream() {
    EXPECT_CALL(*audio_decoder_, WriteEndOfStream());
    audio_renderer_->WriteEndOfStream();
    job_queue_.RunUntilIdle();
  }
  void Seek(SbTime seek_to_time) {
    EXPECT_TRUE(prerolled_);
    prerolled_ = false;
    audio_renderer_->Seek(seek_to_time);
    job_queue_.RunUntilIdle();
    EXPECT_FALSE(prerolled_);
  }

  void CallConsumedCB() {
    ASSERT_TRUE(consumed_cb_);
    consumed_cb_();
    consumed_cb_ = nullptr;
    job_queue_.RunUntilIdle();
  }

  void SendDecoderOutput(const scoped_refptr<DecodedAudio>& decoded_audio) {
    ASSERT_TRUE(output_cb_);

    EXPECT_CALL(*audio_decoder_, Read(_))
        .WillOnce(DoAll(SetArgPointee<0>(kDefaultSamplesPerSecond),
                        Return(decoded_audio)));
    output_cb_();
    job_queue_.RunUntilIdle();
  }

  scoped_refptr<InputBuffer> CreateInputBuffer(SbTime timestamp) {
    const int kInputBufferSize = 4;
    SbPlayerSampleInfo sample_info = {};
    sample_info.buffer = malloc(kInputBufferSize);
    sample_info.buffer_size = kInputBufferSize;
    sample_info.timestamp = timestamp;
    sample_info.drm_info = NULL;
    sample_info.type = kSbMediaTypeAudio;
    GetDefaultAudioSampleInfo().ConvertTo(&sample_info.audio_sample_info);
    return new InputBuffer(DeallocateSampleCB, NULL, this, sample_info);
  }

  scoped_refptr<DecodedAudio> CreateDecodedAudio(SbTime timestamp, int frames) {
    scoped_refptr<DecodedAudio> decoded_audio = new DecodedAudio(
        kDefaultNumberOfChannels, sample_type_, storage_type_, timestamp,
        frames * kDefaultNumberOfChannels *
            media::GetBytesPerSample(sample_type_));
    memset(decoded_audio->data(), 0, decoded_audio->size_in_bytes());
    return decoded_audio;
  }

  void OnError() {}
  void OnPrerolled() {
    SB_DCHECK(job_queue_.BelongsToCurrentThread());
    prerolled_ = true;
  }
  void OnEnded() {}

  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType storage_type_;

  JobQueue job_queue_;
  std::set<const void*> buffers_in_decoder_;

  AudioDecoder::OutputCB output_cb_;
  AudioDecoder::ConsumedCB consumed_cb_;
  bool prerolled_ = true;

  scoped_ptr<AudioRendererPcm> audio_renderer_;
  MockAudioDecoder* audio_decoder_;
  MockAudioRendererSink* audio_renderer_sink_;
  AudioRendererSink::RenderCallback* renderer_callback_;

  void OnDeallocateSample(const void* sample_buffer) {
    ASSERT_TRUE(buffers_in_decoder_.find(sample_buffer) !=
                buffers_in_decoder_.end());
    buffers_in_decoder_.erase(buffers_in_decoder_.find(sample_buffer));
    free(const_cast<void*>(sample_buffer));
  }

  static const media::AudioStreamInfo& GetDefaultAudioStreamInfo() {
    static starboard::media::AudioStreamInfo audio_stream_info;

    audio_stream_info.codec = kSbMediaAudioCodecAac;
    audio_stream_info.mime = "";
    audio_stream_info.number_of_channels = kDefaultNumberOfChannels;
    audio_stream_info.samples_per_second = kDefaultSamplesPerSecond;
    audio_stream_info.bits_per_sample = 32;

    return audio_stream_info;
  }

  static const media::AudioSampleInfo& GetDefaultAudioSampleInfo() {
    static starboard::media::AudioSampleInfo audio_sample_info;

    audio_sample_info.stream_info = GetDefaultAudioStreamInfo();

    return audio_sample_info;
  }

  static void DeallocateSampleCB(SbPlayer player,
                                 void* context,
                                 const void* sample_buffer) {
    AudioRendererTest* test = static_cast<AudioRendererTest*>(context);
    EXPECT_TRUE(test != NULL);
    test->OnDeallocateSample(sample_buffer);
  }
};

bool HasAsyncAudioFramesReporting() {
  // TODO: When deprecating Starboard API versions less than
  // 12 it is safe to assume that all tests can be run regardless of
  // whether the platform has asynchronous audio frames reporting.
  // This function can be removed then.
  return false;
}

// static
const int AudioRendererTest::kDefaultSamplesPerSecond = 100000;

TEST_F(AudioRendererTest, StateAfterConstructed) {
  EXPECT_FALSE(audio_renderer_->IsEndOfStreamWritten());
  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());
  EXPECT_TRUE(audio_renderer_->CanAcceptMoreData());
  bool is_playing = true;
  bool is_eos_played = true;
  bool is_underflow = true;
  double playback_rate = -1.0;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
}

TEST_F(AudioRendererTest, SunnyDay) {
  if (HasAsyncAudioFramesReporting()) {
    SB_LOG(INFO) << "Platform has async audio frames reporting. Test skipped.";
    return;
  }

  {
    InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(0, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  int frames_written = FillRendererWithDecodedAudioAndWriteEOS(0);

  bool is_playing = true;
  bool is_eos_played = true;
  bool is_underflow = true;
  double playback_rate = -1.0;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
  EXPECT_TRUE(prerolled_);

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbTime media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);

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
  const int frames_to_consume = std::min(frames_written, frames_in_buffer) / 2;
  SbTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
  EXPECT_GT(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
  EXPECT_GT(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

#if SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)
TEST_F(AudioRendererTest, SunnyDayWithDoublePlaybackRateAndInt16Samples) {
  if (HasAsyncAudioFramesReporting()) {
    SB_LOG(INFO) << "Platform has async audio frames reporting. Test skipped.";
    return;
  }

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
        Start(0, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  // It is OK to set the rate to 1.0 any number of times.
  EXPECT_CALL(*audio_renderer_sink_, SetPlaybackRate(1.0)).Times(AnyNumber());
  audio_renderer_->SetPlaybackRate(static_cast<float>(kPlaybackRate));

  Seek(0);

  int frames_written = FillRendererWithDecodedAudioAndWriteEOS(0);
  bool is_playing = false;
  bool is_eos_played = true;
  bool is_underflow = true;
  double playback_rate = -1.0;

  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_EQ(playback_rate, kPlaybackRate);
  EXPECT_TRUE(prerolled_);

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbTime media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);

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
  const int frames_to_consume =
      std::min(frames_written / kPlaybackRate, frames_in_buffer) / 2;
  SbTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_GT(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_GT(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}
#endif  // SB_HAS_QUIRK(SUPPORT_INT16_AUDIO_SAMPLES)

TEST_F(AudioRendererTest, StartPlayBeforePreroll) {
  if (HasAsyncAudioFramesReporting()) {
    SB_LOG(INFO) << "Platform has async audio frames reporting. Test skipped.";
    return;
  }

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(0, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  audio_renderer_->Play();

  int frames_written = FillRendererWithDecodedAudioAndWriteEOS(0);

  SendDecoderOutput(new DecodedAudio);

  bool is_playing = false;
  bool is_eos_played = true;
  bool is_underflow = true;
  double playback_rate = -1.0;
  SbTime media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);

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
  const int frames_to_consume = std::min(frames_written, frames_in_buffer) / 2;
  SbTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
  EXPECT_GE(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
  EXPECT_GE(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererTest, DecoderReturnsEOSWithoutAnyData) {
  if (HasAsyncAudioFramesReporting()) {
    SB_LOG(INFO) << "Platform has async audio frames reporting. Test skipped.";
    return;
  }

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(0, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  WriteSample(CreateInputBuffer(0));
  CallConsumedCB();

  WriteEndOfStream();

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamWritten());
  EXPECT_FALSE(audio_renderer_->CanAcceptMoreData());
  EXPECT_FALSE(prerolled_);

  // Return EOS from decoder without sending any audio data, which is valid.
  SendDecoderOutput(new DecodedAudio);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
  EXPECT_TRUE(prerolled_);
  bool is_playing = true;
  bool is_eos_played = false;
  bool is_underflow = true;
  double playback_rate = -1.0;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_TRUE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
}

// Test decoders that take many input samples before returning any output.
TEST_F(AudioRendererTest, DecoderConsumeAllInputBeforeReturningData) {
  if (HasAsyncAudioFramesReporting()) {
    SB_LOG(INFO) << "Platform has async audio frames reporting. Test skipped.";
    return;
  }

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(0, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
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
  EXPECT_FALSE(prerolled_);

  // Return EOS from decoder without sending any audio data, which is valid.
  SendDecoderOutput(new DecodedAudio);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
  EXPECT_TRUE(prerolled_);
  bool is_playing = true;
  bool is_eos_played = false;
  bool is_underflow = true;
  double playback_rate = -1.0;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_TRUE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
}

TEST_F(AudioRendererTest, MoreNumberOfOutputBuffersThanInputBuffers) {
  if (HasAsyncAudioFramesReporting()) {
    SB_LOG(INFO) << "Platform has async audio frames reporting. Test skipped.";
    return;
  }

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(0, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;

  while (!prerolled_) {
    SbTime timestamp =
        frames_written * kSbTimeSecond / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(timestamp));
    CallConsumedCB();
    SendDecoderOutput(CreateDecodedAudio(timestamp, kFramesPerBuffer / 2));
    frames_written += kFramesPerBuffer / 2;
    timestamp = frames_written * kSbTimeSecond / kDefaultSamplesPerSecond;
    SendDecoderOutput(CreateDecodedAudio(timestamp, kFramesPerBuffer / 2));
    frames_written += kFramesPerBuffer / 2;
  }

  WriteEndOfStream();

  bool is_playing = true;
  bool is_eos_played = true;
  bool is_underflow = true;
  double playback_rate = -1.0;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            0);
  EXPECT_FALSE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
  EXPECT_TRUE(prerolled_);

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbTime media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);

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
  const int frames_to_consume = std::min(frames_written, frames_in_buffer) / 2;
  SbTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_TRUE(is_playing);
  EXPECT_FALSE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
  EXPECT_GE(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_played);
  EXPECT_EQ(playback_rate, 1.0);
  EXPECT_GE(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererTest, LessNumberOfOutputBuffersThanInputBuffers) {
  if (HasAsyncAudioFramesReporting()) {
    SB_LOG(INFO) << "Platform has async audio frames reporting. Test skipped.";
    return;
  }

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(*audio_renderer_sink_, HasStarted())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(0, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _))
        .WillOnce(SaveArg<7>(&renderer_callback_));
    EXPECT_CALL(*audio_renderer_sink_, HasStarted())
        .WillRepeatedly(Return(true));
  }

  Seek(0);

  const int kFramesPerBuffer = 1024;

  int frames_written = 0;

  while (!prerolled_) {
    SbTime timestamp =
        frames_written * kSbTimeSecond / kDefaultSamplesPerSecond;
    SbTime output_time = timestamp;
    WriteSample(CreateInputBuffer(timestamp));
    CallConsumedCB();
    frames_written += kFramesPerBuffer / 2;
    timestamp = frames_written * kSbTimeSecond / kDefaultSamplesPerSecond;
    WriteSample(CreateInputBuffer(timestamp));
    CallConsumedCB();
    frames_written += kFramesPerBuffer / 2;
    SendDecoderOutput(CreateDecodedAudio(output_time, kFramesPerBuffer));
  }

  WriteEndOfStream();

  bool is_playing;
  bool is_eos_played;
  bool is_underflow;
  double playback_rate = -1.0;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            0);
  EXPECT_TRUE(prerolled_);

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbTime media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);

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
  const int frames_to_consume = std::min(frames_written, frames_in_buffer) / 2;
  SbTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_GE(new_media_time, media_time);
  media_time = new_media_time;

  const int remaining_frames = frames_in_buffer - frames_to_consume;
  renderer_callback_->ConsumeFrames(remaining_frames, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_GE(new_media_time, media_time);

  EXPECT_TRUE(audio_renderer_->IsEndOfStreamPlayed());
}

TEST_F(AudioRendererTest, Seek) {
  if (HasAsyncAudioFramesReporting()) {
    SB_LOG(INFO) << "Platform has async audio frames reporting. Test skipped.";
    return;
  }

  const double kSeekTime = 0.5 * kSbTimeSecond;

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*audio_renderer_sink_, Stop()).Times(AnyNumber());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(0, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
    EXPECT_CALL(*audio_renderer_sink_, Stop());
    EXPECT_CALL(*audio_decoder_, Reset());
    EXPECT_CALL(
        *audio_renderer_sink_,
        Start(kSeekTime, kDefaultNumberOfChannels, kDefaultSamplesPerSecond,
              kDefaultAudioSampleType, kDefaultAudioFrameStorageType, _, _, _));
  }

  Seek(0);

  int frames_written = FillRendererWithDecodedAudioAndWriteEOS(0);

  bool is_playing;
  bool is_eos_played;
  bool is_underflow;
  double playback_rate = -1.0;
  EXPECT_EQ(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            0);
  EXPECT_TRUE(prerolled_);

  audio_renderer_->Play();

  SendDecoderOutput(new DecodedAudio);

  SbTime media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);

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
  const int frames_to_consume = std::min(frames_written, frames_in_buffer) / 10;
  SbTime new_media_time;

  EXPECT_FALSE(audio_renderer_->IsEndOfStreamPlayed());

  renderer_callback_->ConsumeFrames(frames_to_consume, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_GE(new_media_time, media_time);
  Seek(kSeekTime);

  frames_written += FillRendererWithDecodedAudioAndWriteEOS(kSeekTime);

  EXPECT_GE(audio_renderer_->GetCurrentMediaTime(&is_playing, &is_eos_played,
                                                 &is_underflow, &playback_rate),
            kSeekTime);
  EXPECT_TRUE(prerolled_);

  audio_renderer_->Play();
  SendDecoderOutput(new DecodedAudio);

  renderer_callback_->GetSourceStatus(&frames_in_buffer, &offset_in_frames,
                                      &is_playing, &is_eos_reached);
  EXPECT_GT(frames_in_buffer, 0);
  EXPECT_GE(offset_in_frames, 0);
  EXPECT_TRUE(is_playing);
  EXPECT_TRUE(is_eos_reached);
  renderer_callback_->ConsumeFrames(frames_in_buffer, SbTimeGetMonotonicNow());
  new_media_time = audio_renderer_->GetCurrentMediaTime(
      &is_playing, &is_eos_played, &is_underflow, &playback_rate);
  EXPECT_GE(new_media_time, kSeekTime);

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
