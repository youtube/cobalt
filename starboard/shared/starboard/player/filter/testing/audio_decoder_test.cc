// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"

#include <deque>
#include <functional>
#include <map>

#include "starboard/common/condition_variable.h"
#include "starboard/common/media.h"
#include "starboard/common/mutex.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/stub_player_components_factory.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::ValuesIn;
using video_dmp::VideoDmpReader;

const SbTimeMonotonic kWaitForNextEventTimeOut = 5 * kSbTimeSecond;

class AudioDecoderTest
    : public ::testing::TestWithParam<std::tuple<const char*, bool> > {
 public:
  AudioDecoderTest()
      : test_filename_(std::get<0>(GetParam())),
        using_stub_decoder_(std::get<1>(GetParam())),
        dmp_reader_(ResolveTestFileName(test_filename_).c_str(),
                    VideoDmpReader::kEnableReadOnDemand) {
    SB_LOG(INFO) << "Testing " << test_filename_
                 << (using_stub_decoder_ ? " with stub audio decoder." : ".");
  }
  void SetUp() override {
    ASSERT_NE(dmp_reader_.audio_codec(), kSbMediaAudioCodecNone);
    ASSERT_GT(dmp_reader_.number_of_audio_buffers(), 0);

    CreateComponents(dmp_reader_.audio_codec(), dmp_reader_.audio_sample_info(),
                     &audio_decoder_, &audio_renderer_sink_);
  }

 protected:
  enum Event { kConsumed, kOutput, kError };

  void CreateComponents(SbMediaAudioCodec codec,
                        const SbMediaAudioSampleInfo& audio_sample_info,
                        scoped_ptr<AudioDecoder>* audio_decoder,
                        scoped_ptr<AudioRendererSink>* audio_renderer_sink) {
    if (CreateAudioComponents(using_stub_decoder_, codec, audio_sample_info,
                              audio_decoder, audio_renderer_sink)) {
      SB_CHECK(*audio_decoder);
      (*audio_decoder)
          ->Initialize(std::bind(&AudioDecoderTest::OnOutput, this),
                       std::bind(&AudioDecoderTest::OnError, this));
    }
  }

  void OnOutput() {
    ScopedLock scoped_lock(event_queue_mutex_);
    event_queue_.push_back(kOutput);
  }

  void OnError() {
    ScopedLock scoped_lock(event_queue_mutex_);
    event_queue_.push_back(kError);
  }

  void OnConsumed() {
    ScopedLock scoped_lock(event_queue_mutex_);
    event_queue_.push_back(kConsumed);
  }

  void WaitForNextEvent(Event* event) {
    SbTimeMonotonic start = SbTimeGetMonotonicNow();
    while (SbTimeGetMonotonicNow() - start < kWaitForNextEventTimeOut) {
      job_queue_.RunUntilIdle();
      {
        ScopedLock scoped_lock(event_queue_mutex_);
        if (!event_queue_.empty()) {
          *event = event_queue_.front();
          event_queue_.pop_front();

          if (*event == kConsumed) {
            ASSERT_FALSE(can_accept_more_input_);
            can_accept_more_input_ = true;
          }
          return;
        }
      }
      SbThreadSleep(kSbTimeMillisecond);
    }
    *event = kError;
  }

  // TODO: Add test to ensure that |consumed_cb| is not reused by the decoder.
  AudioDecoder::ConsumedCB consumed_cb() {
    return std::bind(&AudioDecoderTest::OnConsumed, this);
  }

  // This has to be called when the decoder is just initialized/reset or when
  // OnConsumed() is called.
  void WriteSingleInput(size_t index) {
    ASSERT_TRUE(can_accept_more_input_);
    ASSERT_LT(index, dmp_reader_.number_of_audio_buffers());

    can_accept_more_input_ = false;

    last_input_buffer_ = GetAudioInputBuffer(index);

    audio_decoder_->Decode(last_input_buffer_, consumed_cb());
  }

  // This has to be called when OnOutput() is called.
  void ReadFromDecoder(scoped_refptr<DecodedAudio>* decoded_audio) {
    ASSERT_TRUE(decoded_audio);

    int decoded_sample_rate;
    scoped_refptr<DecodedAudio> local_decoded_audio =
        audio_decoder_->Read(&decoded_sample_rate);
    ASSERT_TRUE(local_decoded_audio);
    if (!first_output_received_) {
      first_output_received_ = true;
      decoded_audio_sample_type_ = local_decoded_audio->sample_type();
      decoded_audio_storage_type_ = local_decoded_audio->storage_type();
      decoded_audio_samples_per_second_ = decoded_sample_rate;
    }

    if (local_decoded_audio->is_end_of_stream()) {
      *decoded_audio = local_decoded_audio;
      return;
    }
    ASSERT_EQ(decoded_audio_sample_type_, local_decoded_audio->sample_type());
    ASSERT_EQ(decoded_audio_storage_type_, local_decoded_audio->storage_type());
    ASSERT_EQ(decoded_audio_samples_per_second_, decoded_sample_rate);

    // TODO: Adaptive audio decoder outputs may don't have timestamp info.
    // Currently, we skip timestamp check if the outputs don't have timestamp
    // info. Enable it after we fix timestamp issues.
    if (local_decoded_audio->timestamp() && last_decoded_audio_) {
      ASSERT_LT(last_decoded_audio_->timestamp(),
                local_decoded_audio->timestamp());
    }
    last_decoded_audio_ = local_decoded_audio;
    num_of_output_frames_ += last_decoded_audio_->frames();
    *decoded_audio = local_decoded_audio;
  }

  void WriteMultipleInputs(size_t start_index,
                           size_t number_of_inputs_to_write,
                           bool* error_occurred = nullptr) {
    ASSERT_LE(start_index + number_of_inputs_to_write,
              dmp_reader_.number_of_audio_buffers());

    if (error_occurred) {
      *error_occurred = false;
    }

    ASSERT_NO_FATAL_FAILURE(WriteSingleInput(start_index));
    ++start_index;
    --number_of_inputs_to_write;

    while (number_of_inputs_to_write > 0) {
      Event event = kError;
      ASSERT_NO_FATAL_FAILURE(WaitForNextEvent(&event));
      if (event == kConsumed) {
        ASSERT_NO_FATAL_FAILURE(WriteSingleInput(start_index));
        ++start_index;
        --number_of_inputs_to_write;
        continue;
      }
      if (event == kError) {
        ASSERT_TRUE(error_occurred);
        *error_occurred = true;
        return;
      }
      ASSERT_EQ(kOutput, event);
      scoped_refptr<DecodedAudio> decoded_audio;
      ASSERT_NO_FATAL_FAILURE(ReadFromDecoder(&decoded_audio));
      ASSERT_TRUE(decoded_audio);
      ASSERT_FALSE(decoded_audio->is_end_of_stream());
    }
  }

  // The start_index will be updated to the new position.
  void WriteTimeLimitedInputs(int* start_index, SbTime time_limit) {
    SB_DCHECK(start_index);
    SB_DCHECK(*start_index >= 0);
    SB_DCHECK(*start_index < dmp_reader_.number_of_audio_buffers());
    ASSERT_NO_FATAL_FAILURE(
        WriteSingleInput(static_cast<size_t>(*start_index)));
    SB_DCHECK(last_input_buffer_);
    SbTime last_timestamp = last_input_buffer_->timestamp();
    SbTime first_timestamp = last_timestamp;
    ++(*start_index);

    while (last_timestamp - first_timestamp < time_limit &&
           *start_index < dmp_reader_.number_of_audio_buffers()) {
      Event event = kError;
      ASSERT_NO_FATAL_FAILURE(WaitForNextEvent(&event));
      if (event == kConsumed) {
        ASSERT_NO_FATAL_FAILURE(
            WriteSingleInput(static_cast<size_t>(*start_index)));
        SB_DCHECK(last_input_buffer_);
        last_timestamp = last_input_buffer_->timestamp();
        ++(*start_index);
        continue;
      }
      ASSERT_EQ(kOutput, event);
      scoped_refptr<DecodedAudio> decoded_audio;
      ASSERT_NO_FATAL_FAILURE(ReadFromDecoder(&decoded_audio));
      ASSERT_TRUE(decoded_audio);
      ASSERT_FALSE(decoded_audio->is_end_of_stream());
    }
  }

  void DrainOutputs(bool* error_occurred = nullptr) {
    if (error_occurred) {
      *error_occurred = false;
    }

    for (;;) {
      Event event = kError;
      ASSERT_NO_FATAL_FAILURE(WaitForNextEvent(&event));
      if (event == kError) {
        if (error_occurred) {
          *error_occurred = true;
        } else {
          FAIL();
        }
        return;
      }
      if (event == kConsumed) {
        continue;
      }
      ASSERT_EQ(kOutput, event);
      scoped_refptr<DecodedAudio> decoded_audio;
      ASSERT_NO_FATAL_FAILURE(ReadFromDecoder(&decoded_audio));
      ASSERT_TRUE(decoded_audio);
      if (decoded_audio->is_end_of_stream()) {
        break;
      }
    }
  }

  void ResetDecoder() {
    audio_decoder_->Reset();
    can_accept_more_input_ = true;
    last_input_buffer_ = nullptr;
    last_decoded_audio_ = nullptr;
    eos_written_ = false;
    decoded_audio_samples_per_second_ = 0;
    first_output_received_ = false;
  }

  void WaitForDecodedAudio() {
    Event event;
    while (!last_decoded_audio_) {
      ASSERT_NO_FATAL_FAILURE(WaitForNextEvent(&event));
      if (event == kConsumed) {
        continue;
      }
      ASSERT_EQ(kOutput, event);
      scoped_refptr<DecodedAudio> decoded_audio;
      ASSERT_NO_FATAL_FAILURE(ReadFromDecoder(&decoded_audio));
      ASSERT_TRUE(decoded_audio);
      ASSERT_FALSE(decoded_audio->is_end_of_stream());
    }
  }

  scoped_refptr<InputBuffer> GetAudioInputBuffer(size_t index) {
    auto player_sample_info =
        dmp_reader_.GetPlayerSampleInfo(kSbMediaTypeAudio, index);
    auto input_buffer = new InputBuffer(StubDeallocateSampleFunc, nullptr,
                                        nullptr, player_sample_info);
    auto iter = invalid_inputs_.find(index);
    if (iter != invalid_inputs_.end()) {
      std::vector<uint8_t> content(input_buffer->size(), iter->second);
      // Replace the content with invalid data.
      input_buffer->SetDecryptedContent(content.data(),
                                        static_cast<int>(content.size()));
    }
    return input_buffer;
  }

  void UseInvalidDataForInput(size_t index, uint8_t byte_to_fill) {
    invalid_inputs_[index] = byte_to_fill;
  }

  void WriteEndOfStream() {
    SB_DCHECK(!eos_written_);
    audio_decoder_->WriteEndOfStream();
    eos_written_ = true;
  }

  void AssertInvalidOutputFormat() {
    ASSERT_TRUE(decoded_audio_sample_type_ == kSbMediaAudioSampleTypeFloat32 ||
                decoded_audio_sample_type_ ==
                    kSbMediaAudioSampleTypeInt16Deprecated);

    ASSERT_TRUE(decoded_audio_storage_type_ ==
                    kSbMediaAudioFrameStorageTypeInterleaved ||
                decoded_audio_storage_type_ ==
                    kSbMediaAudioFrameStorageTypePlanar);

    ASSERT_TRUE(decoded_audio_samples_per_second_ > 0 &&
                decoded_audio_samples_per_second_ <= 480000);
  }

  void AssertExpectedAndOutputFramesMatch(int expected_output_frames) {
    if (using_stub_decoder_) {
      // The number of output frames is not applicable in the case of the
      // StubAudioDecoder, because it is not actually doing any decoding work.
      return;
    }
    ASSERT_LE(abs(expected_output_frames - num_of_output_frames_), 1);
  }

  Mutex event_queue_mutex_;
  std::deque<Event> event_queue_;

  // Test parameter for the filename to load with the VideoDmpReader.
  const char* test_filename_;

  // Test parameter to configure whether the test is run with the
  // StubAudioDecoder, or the platform-specific AudioDecoderImpl
  bool using_stub_decoder_;

  JobQueue job_queue_;
  VideoDmpReader dmp_reader_;
  scoped_ptr<AudioDecoder> audio_decoder_;
  scoped_ptr<AudioRendererSink> audio_renderer_sink_;

  bool can_accept_more_input_ = true;
  scoped_refptr<InputBuffer> last_input_buffer_;
  scoped_refptr<DecodedAudio> last_decoded_audio_;

  bool eos_written_ = false;

  std::map<size_t, uint8_t> invalid_inputs_;

  int num_of_output_frames_ = 0;

  SbMediaAudioSampleType decoded_audio_sample_type_ =
      kSbMediaAudioSampleTypeInt16Deprecated;
  SbMediaAudioFrameStorageType decoded_audio_storage_type_ =
      kSbMediaAudioFrameStorageTypeInterleaved;
  int decoded_audio_samples_per_second_ = 0;

  bool first_output_received_ = false;
};

TEST_P(AudioDecoderTest, MultiDecoders) {
  const int kDecodersToCreate = 100;
  const int kMinimumNumberOfExtraDecodersRequired = 3;

  scoped_ptr<AudioDecoder> audio_decoders[kDecodersToCreate];
  scoped_ptr<AudioRendererSink> audio_renderer_sinks[kDecodersToCreate];

  for (int i = 0; i < kDecodersToCreate; ++i) {
    CreateComponents(dmp_reader_.audio_codec(), dmp_reader_.audio_sample_info(),
                     &audio_decoders[i], &audio_renderer_sinks[i]);
    if (!audio_decoders[i]) {
      ASSERT_GE(i, kMinimumNumberOfExtraDecodersRequired);
    }
  }
}

TEST_P(AudioDecoderTest, SingleInput) {
  ASSERT_NO_FATAL_FAILURE(WriteSingleInput(0));
  WriteEndOfStream();

  ASSERT_NO_FATAL_FAILURE(DrainOutputs());
  ASSERT_TRUE(last_decoded_audio_);
  ASSERT_NO_FATAL_FAILURE(AssertInvalidOutputFormat());
}

TEST_P(AudioDecoderTest, SingleInputHEAAC) {
  static const int kAacFrameSize = 1024;

  if (dmp_reader_.audio_codec() != kSbMediaAudioCodecAac) {
    return;
  }

  ASSERT_NO_FATAL_FAILURE(WriteSingleInput(0));
  WriteEndOfStream();

  ASSERT_NO_FATAL_FAILURE(DrainOutputs());
  ASSERT_TRUE(last_decoded_audio_);
  ASSERT_NO_FATAL_FAILURE(AssertInvalidOutputFormat());

  int input_sample_rate =
      last_input_buffer_->audio_sample_info().samples_per_second;
  ASSERT_NE(0, decoded_audio_samples_per_second_);
  int expected_output_frames =
      kAacFrameSize * decoded_audio_samples_per_second_ / input_sample_rate;
  AssertExpectedAndOutputFramesMatch(expected_output_frames);
}

TEST_P(AudioDecoderTest, InvalidCodec) {
  auto invalid_codec = dmp_reader_.audio_codec() == kSbMediaAudioCodecAac
                           ? kSbMediaAudioCodecOpus
                           : kSbMediaAudioCodecAac;
  auto audio_sample_info = dmp_reader_.audio_sample_info();

  audio_sample_info.codec = invalid_codec;

  CreateComponents(invalid_codec, audio_sample_info, &audio_decoder_,
                   &audio_renderer_sink_);
  if (!audio_decoder_) {
    return;
  }

  WriteSingleInput(0);
  WriteEndOfStream();

  bool error_occurred = true;
  ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
}

TEST_P(AudioDecoderTest, InvalidConfig) {
  auto original_audio_sample_info = dmp_reader_.audio_sample_info();

  for (uint16_t i = 0;
       i < original_audio_sample_info.audio_specific_config_size; ++i) {
    std::vector<uint8_t> config(
        original_audio_sample_info.audio_specific_config_size);
    memcpy(config.data(), original_audio_sample_info.audio_specific_config,
           original_audio_sample_info.audio_specific_config_size);
    auto audio_sample_info = original_audio_sample_info;
    config[i] = ~config[i];
    audio_sample_info.audio_specific_config = config.data();

    CreateComponents(dmp_reader_.audio_codec(), audio_sample_info,
                     &audio_decoder_, &audio_renderer_sink_);
    if (!audio_decoder_) {
      return;
    }
    WriteSingleInput(0);
    WriteEndOfStream();

    bool error_occurred = true;
    ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));

    ResetDecoder();
  }

  for (uint16_t i = 0;
       i < original_audio_sample_info.audio_specific_config_size; ++i) {
    std::vector<uint8_t> config(i);
    memcpy(config.data(), original_audio_sample_info.audio_specific_config, i);
    auto audio_sample_info = original_audio_sample_info;
    audio_sample_info.audio_specific_config = config.data();
    audio_sample_info.audio_specific_config_size = i;

    CreateComponents(dmp_reader_.audio_codec(), audio_sample_info,
                     &audio_decoder_, &audio_renderer_sink_);
    if (!audio_decoder_) {
      return;
    }
    WriteSingleInput(0);
    WriteEndOfStream();

    bool error_occurred = true;
    ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));

    ResetDecoder();
  }
}

TEST_P(AudioDecoderTest, SingleInvalidInput) {
  UseInvalidDataForInput(0, 0xab);

  WriteSingleInput(0);
  WriteEndOfStream();

  bool error_occurred = true;
  ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
}

TEST_P(AudioDecoderTest, MultipleValidInputsAfterInvalidInput) {
  const size_t kMaxNumberOfInputToWrite = 10;
  const size_t number_of_input_to_write =
      std::min(kMaxNumberOfInputToWrite, dmp_reader_.number_of_audio_buffers());

  UseInvalidDataForInput(0, 0xab);

  bool error_occurred = true;
  // Write first few frames.  The first one is invalid and the rest are valid.
  WriteMultipleInputs(0, number_of_input_to_write, &error_occurred);

  if (!error_occurred) {
    WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
  }
}

TEST_P(AudioDecoderTest, MultipleInvalidInput) {
  const size_t kMaxNumberOfInputToWrite = 128;
  const size_t number_of_input_to_write =
      std::min(kMaxNumberOfInputToWrite, dmp_reader_.number_of_audio_buffers());
  // Replace the content of the first few input buffers with invalid data.
  // Every test instance loads its own copy of data so this won't affect other
  // tests.
  for (size_t i = 0; i < number_of_input_to_write; ++i) {
    UseInvalidDataForInput(i, static_cast<uint8_t>(0xab + i));
  }

  bool error_occurred = true;
  WriteMultipleInputs(0, number_of_input_to_write, &error_occurred);
  if (!error_occurred) {
    WriteEndOfStream();
    ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
  }
}

TEST_P(AudioDecoderTest, EndOfStreamWithoutAnyInput) {
  WriteEndOfStream();

  ASSERT_NO_FATAL_FAILURE(DrainOutputs());
  ASSERT_FALSE(last_decoded_audio_);
  ASSERT_NO_FATAL_FAILURE(AssertInvalidOutputFormat());
}

TEST_P(AudioDecoderTest, ResetBeforeInput) {
  ResetDecoder();

  ASSERT_NO_FATAL_FAILURE(WriteSingleInput(0));
  WriteEndOfStream();

  ASSERT_NO_FATAL_FAILURE(DrainOutputs());
  ASSERT_TRUE(last_decoded_audio_);
  ASSERT_NO_FATAL_FAILURE(AssertInvalidOutputFormat());
}

TEST_P(AudioDecoderTest, MultipleInputs) {
  const size_t kMaxNumberOfInputsToWrite = 5;
  const size_t number_of_inputs_to_write = std::min(
      kMaxNumberOfInputsToWrite, dmp_reader_.number_of_audio_buffers());

  ASSERT_NO_FATAL_FAILURE(WriteMultipleInputs(0, number_of_inputs_to_write));

  WriteEndOfStream();

  ASSERT_NO_FATAL_FAILURE(DrainOutputs());
  ASSERT_TRUE(last_decoded_audio_);
  ASSERT_NO_FATAL_FAILURE(AssertInvalidOutputFormat());
}

TEST_P(AudioDecoderTest, LimitedInput) {
  SbTime duration = kSbTimeSecond / 2;
  SbMediaSetAudioWriteDuration(duration);

  ASSERT_FALSE(last_decoded_audio_);
  int start_index = 0;
  ASSERT_NO_FATAL_FAILURE(WriteTimeLimitedInputs(&start_index, duration));

  if (start_index >= dmp_reader_.number_of_audio_buffers()) {
    WriteEndOfStream();
  }

  // Wait for decoded audio.
  WaitForDecodedAudio();
}

TEST_P(AudioDecoderTest, ContinuedLimitedInput) {
  SbTime duration = kSbTimeSecond / 2;
  SbMediaSetAudioWriteDuration(duration);

  SbTime start = SbTimeGetMonotonicNow();
  int start_index = 0;
  Event event;
  while (true) {
    ASSERT_NO_FATAL_FAILURE(WriteTimeLimitedInputs(&start_index, duration));
    if (start_index >= dmp_reader_.number_of_audio_buffers()) {
      break;
    }
    SB_DCHECK(last_input_buffer_);
    WaitForDecodedAudio();
    ASSERT_TRUE(last_decoded_audio_);
    while ((last_input_buffer_->timestamp() -
            last_decoded_audio_->timestamp()) > duration ||
           !can_accept_more_input_) {
      ASSERT_NO_FATAL_FAILURE(WaitForNextEvent(&event));
      if (event == kConsumed) {
        continue;
      }
      ASSERT_EQ(kOutput, event);
      scoped_refptr<DecodedAudio> decoded_audio;
      ASSERT_NO_FATAL_FAILURE(ReadFromDecoder(&decoded_audio));
      ASSERT_TRUE(decoded_audio);
      ASSERT_FALSE(decoded_audio->is_end_of_stream());
    }
  }
  WriteEndOfStream();
  ASSERT_NO_FATAL_FAILURE(DrainOutputs());
  SbTime elapsed = SbTimeGetMonotonicNow() - start;
  SB_LOG(INFO) << "Decoding " << dmp_reader_.number_of_audio_buffers() << " au "
               << " of " << GetMediaAudioCodecName(dmp_reader_.audio_codec())
               << " takes " << elapsed << " microseconds.";

  ASSERT_TRUE(last_decoded_audio_);
  ASSERT_NO_FATAL_FAILURE(AssertInvalidOutputFormat());
}

INSTANTIATE_TEST_CASE_P(
    AudioDecoderTests,
    AudioDecoderTest,
    Combine(ValuesIn(GetSupportedAudioTestFiles(kIncludeHeaac,
                                                6,
                                                "audiopassthrough=false")),
            Bool()));

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
