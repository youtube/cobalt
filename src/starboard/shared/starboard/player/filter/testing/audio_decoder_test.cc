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

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/stub_player_components_impl.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_HAS(PLAYER_FILTER_TESTS)
// TODO: Write test for HE-AAC

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

std::string GetTestInputDirectory() {
  const size_t kPathSize = SB_FILE_MAX_PATH + 1;

  char content_path[kPathSize];
  SB_CHECK(SbSystemGetPath(kSbSystemPathContentDirectory, content_path,
                           kPathSize));
  std::string directory_path =
      std::string(content_path) + SB_FILE_SEP_CHAR + "test" +
      SB_FILE_SEP_CHAR + "starboard" + SB_FILE_SEP_CHAR + "shared" +
      SB_FILE_SEP_CHAR + "starboard" + SB_FILE_SEP_CHAR + "player" +
      SB_FILE_SEP_CHAR + "testdata";

  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str()))
      << "Cannot open directory " << directory_path;
  return directory_path;
}

void DeallocateSampleFunc(SbPlayer player,
                          void* context,
                          const void* sample_buffer) {
  SB_UNREFERENCED_PARAMETER(player);
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(sample_buffer);
}

std::string ResolveTestFileName(const char* filename) {
  return GetTestInputDirectory() + SB_FILE_SEP_CHAR + filename;
}

class AudioDecoderTest
    : public ::testing::TestWithParam<std::tuple<const char*, bool> > {
 public:
  AudioDecoderTest()
      : test_filename_(std::get<0>(GetParam())),
        using_stub_decoder_(std::get<1>(GetParam())),
        dmp_reader_(ResolveTestFileName(test_filename_).c_str()) {
    SB_LOG(INFO) << "Testing " << test_filename_
                 << (using_stub_decoder_ ? " with stub audio decoder." : ".");
  }
  void SetUp() override {
    ASSERT_NE(dmp_reader_.audio_codec(), kSbMediaAudioCodecNone);
    ASSERT_GT(dmp_reader_.number_of_audio_buffers(), 0);

    PlayerComponents::AudioParameters audio_parameters = {
        dmp_reader_.audio_codec(), dmp_reader_.audio_sample_info(),
        kSbDrmSystemInvalid};

    scoped_ptr<PlayerComponents> components;
    if (using_stub_decoder_) {
      components = make_scoped_ptr<StubPlayerComponentsImpl>(
          new StubPlayerComponentsImpl);
    } else {
      components = PlayerComponents::Create();
    }
    components->CreateAudioComponents(audio_parameters, &audio_decoder_,
                                      &audio_renderer_sink_);
    ASSERT_TRUE(audio_decoder_);

    audio_decoder_->Initialize(std::bind(&AudioDecoderTest::OnOutput, this),
                               std::bind(&AudioDecoderTest::OnError, this));
  }

  void OnOutput() {
    ScopedLock scoped_lock(event_queue_mutex_);
    event_queue_.push_back(kOutput);
  }
  void OnError() {
    ScopedLock scoped_lock(event_queue_mutex_);
    event_queue_.push_back(kError);
  }

 protected:
  enum Event { kConsumed, kOutput, kError };

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
    FAIL();
  }

  // TODO: Add test to ensure that |consumed_cb| is not reused by the decoder.
  AudioDecoder::ConsumedCB consumed_cb() {
    return std::bind(&AudioDecoderTest::OnConsumed, this);
  }

  // This has to be called when the decoder is just initialized/reseted or when
  // OnConsumed() is called.
  void WriteSingleInput(size_t index) {
    ASSERT_TRUE(can_accept_more_input_);
    ASSERT_LT(index, dmp_reader_.number_of_audio_buffers());

    can_accept_more_input_ = false;

    last_input_buffer_ = GetAudioInputBuffer(index);

    outstanding_inputs_.insert(last_input_buffer_->timestamp());

    audio_decoder_->Decode(last_input_buffer_, consumed_cb());
  }

  // This has to be called when OnOutput() is called.
  void ReadFromDecoder(scoped_refptr<DecodedAudio>* decoded_audio) {
    ASSERT_TRUE(decoded_audio);

    scoped_refptr<DecodedAudio> local_decoded_audio = audio_decoder_->Read();
    ASSERT_TRUE(local_decoded_audio);

    if (local_decoded_audio->is_end_of_stream()) {
      *decoded_audio = local_decoded_audio;
      return;
    }
    if (last_decoded_audio_) {
      ASSERT_LT(last_decoded_audio_->timestamp(),
                local_decoded_audio->timestamp());
    }
    last_decoded_audio_ = local_decoded_audio;
    *decoded_audio = local_decoded_audio;
  }

  void WriteMultipleInputs(size_t start_index,
                           size_t number_of_inputs_to_write) {
    ASSERT_LE(start_index + number_of_inputs_to_write,
              dmp_reader_.number_of_audio_buffers());

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

  void DrainOutputs(bool* error_occurred = NULL) {
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
    last_input_buffer_ = NULL;
    last_decoded_audio_ = NULL;
    outstanding_inputs_.clear();
    eos_written_ = false;
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

  scoped_refptr<InputBuffer> GetAudioInputBuffer(size_t index) const {
    auto player_sample_info =
        dmp_reader_.GetPlayerSampleInfo(kSbMediaTypeAudio, index);
#if SB_API_VERSION >= 11
    return new InputBuffer(DeallocateSampleFunc, NULL, NULL,
                           player_sample_info);
#else   // SB_API_VERSION >= 11
    SbMediaAudioSampleInfo audio_sample_info =
        dmp_reader_.GetAudioSampleInfo(index);
    return new InputBuffer(kSbMediaTypeAudio, DeallocateSampleFunc, NULL, NULL,
                           player_sample_info, &audio_sample_info);
#endif  // SB_API_VERSION >= 11
  }

  void WriteEndOfStream() {
    SB_DCHECK(!eos_written_);
    audio_decoder_->WriteEndOfStream();
    eos_written_ = true;
  }

  void AssertInvalidOutputFormat() {
    SbMediaAudioSampleType output_sample_type = audio_decoder_->GetSampleType();
    ASSERT_TRUE(output_sample_type == kSbMediaAudioSampleTypeFloat32 ||
                output_sample_type == kSbMediaAudioSampleTypeInt16Deprecated);

    SbMediaAudioFrameStorageType output_storage_type =
        audio_decoder_->GetStorageType();
    ASSERT_TRUE(output_storage_type ==
                    kSbMediaAudioFrameStorageTypeInterleaved ||
                output_storage_type == kSbMediaAudioFrameStorageTypePlanar);

    int output_samples_per_second = audio_decoder_->GetSamplesPerSecond();
    ASSERT_TRUE(output_samples_per_second > 0 &&
                output_samples_per_second <= 480000);
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

  // List of timestamps of InputBuffers provided to the Decoder so far, sorted
  // in ascending order.
  std::set<SbTime> outstanding_inputs_;

  bool eos_written_ = false;
};

TEST_P(AudioDecoderTest, ThreeMoreDecoders) {
  const int kDecodersToCreate = 3;

  PlayerComponents::AudioParameters audio_parameters = {
      dmp_reader_.audio_codec(), dmp_reader_.audio_sample_info(),
      kSbDrmSystemInvalid};

  scoped_ptr<PlayerComponents> components = PlayerComponents::Create();
  scoped_ptr<AudioDecoder> audio_decoders[kDecodersToCreate];
  scoped_ptr<AudioRendererSink> audio_renderer_sinks[kDecodersToCreate];

  for (int i = 0; i < kDecodersToCreate; ++i) {
    components->CreateAudioComponents(audio_parameters, &audio_decoders[i],
                                      &audio_renderer_sinks[i]);
    ASSERT_TRUE(audio_decoders[i]);

    audio_decoders[i]->Initialize(std::bind(&AudioDecoderTest::OnOutput, this),
                                  std::bind(&AudioDecoderTest::OnError, this));
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
  if (last_decoded_audio_->frames() == kAacFrameSize) {
    return;
  }
  auto sample_info = dmp_reader_.audio_sample_info();
  ASSERT_EQ(sample_info.samples_per_second * 2,
            audio_decoder_->GetSamplesPerSecond());
}

TEST_P(AudioDecoderTest, SingleInvalidInput) {
  can_accept_more_input_ = false;
  last_input_buffer_ = GetAudioInputBuffer(0);
  std::vector<uint8_t> content(last_input_buffer_->size(), 0xab);
  // Replace the content with invalid data.
  last_input_buffer_->SetDecryptedContent(content.data(),
                                          static_cast<int>(content.size()));
  audio_decoder_->Decode(last_input_buffer_, consumed_cb());

  WriteEndOfStream();

  bool error_occurred = false;
  ASSERT_NO_FATAL_FAILURE(DrainOutputs(&error_occurred));
  // The decoder is allowed to either signal an error or return dummy audio
  // data but not both.
  if (error_occurred) {
    ASSERT_FALSE(last_decoded_audio_);
  } else {
    ASSERT_TRUE(last_decoded_audio_);
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

#if SB_API_VERSION >= 11

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
  ASSERT_TRUE(last_decoded_audio_);
  ASSERT_NO_FATAL_FAILURE(AssertInvalidOutputFormat());
}

#endif  // SB_API_VERSION >= 11

std::vector<const char*> GetSupportedTests() {
  const char* kFilenames[] = {"beneath_the_canopy_140_aac.dmp",
                              "beneath_the_canopy_249_opus.dmp", "heaac.dmp"};

  static std::vector<const char*> test_params;

  if (!test_params.empty()) {
    return test_params;
  }

  for (auto filename : kFilenames) {
    VideoDmpReader dmp_reader(ResolveTestFileName(filename).c_str());
    SB_DCHECK(dmp_reader.number_of_audio_buffers() > 0);
    if (SbMediaIsAudioSupported(dmp_reader.audio_codec(),
                                dmp_reader.audio_bitrate())) {
      test_params.push_back(filename);
    }
  }

  SB_DCHECK(!test_params.empty());
  return test_params;
}

INSTANTIATE_TEST_CASE_P(AudioDecoderTests,
                        AudioDecoderTest,
                        Combine(ValuesIn(GetSupportedTests()), Bool()));

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
#endif  // SB_HAS(PLAYER_FILTER_TESTS)
