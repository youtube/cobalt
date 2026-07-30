// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <string>

#include "starboard/common/check_op.h"
#include "starboard/common/time.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/adaptive_audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/mock_audio_decoder.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/stub_player_components_factory.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO: Implement AudioDecoderMock and refactor the test accordingly.
namespace starboard {
namespace {

using std::deque;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::NiceMock;
using ::testing::ValuesIn;

const int64_t kWaitForNextEventTimeOut = 5'000'000;  // 5 seconds

class AdaptiveAudioDecoderTest
    : public ::testing::TestWithParam<std::tuple<vector<const char*>, bool>> {
 protected:
  enum Event { kConsumed, kOutput, kError };

  AdaptiveAudioDecoderTest()
      : test_filenames_(std::get<0>(GetParam())),
        using_stub_decoder_(std::get<1>(GetParam())) {
    for (auto filename : test_filenames_) {
      dmp_readers_.emplace_back(
          new VideoDmpReader(filename, VideoDmpReader::kEnableReadOnDemand));
    }

    auto accumulate_operation = [](string accumulated, const char* str) {
      if (!accumulated.length()) {
        return string(str);
      }
      return std::move(accumulated) + "->" + str;
    };
    string description =
        std::accumulate(test_filenames_.begin(), test_filenames_.end(),
                        string(), accumulate_operation);
    SB_LOG(INFO) << "Testing: " << description
                 << (using_stub_decoder_ ? " (with stub decoder)." : ".");
  }

  void SetUp() override {
    ASSERT_GT(dmp_readers_.size(), 0U);
    for (auto& dmp_reader : dmp_readers_) {
      ASSERT_NE(dmp_reader->audio_codec(), kSbMediaAudioCodecNone);
      ASSERT_GT(dmp_reader->number_of_audio_buffers(), 0U);
    }

    std::unique_ptr<AudioRendererSink> audio_renderer_sink;
    ASSERT_TRUE(CreateAudioComponents(using_stub_decoder_, &job_queue_,
                                      dmp_readers_[0]->audio_stream_info(),
                                      &audio_decoder_, &audio_renderer_sink));
    ASSERT_TRUE(audio_decoder_);
    audio_decoder_->Initialize(
        std::bind(&AdaptiveAudioDecoderTest::OnOutput, this),
        std::bind(&AdaptiveAudioDecoderTest::OnError, this));
  }

  void WriteSingleInput(VideoDmpReader* dmp_reader, size_t buffer_index) {
    SB_DCHECK(dmp_reader);

    ASSERT_TRUE(can_accept_more_input_);
    ASSERT_LT(buffer_index, dmp_reader->number_of_audio_buffers());

    can_accept_more_input_ = false;
    scoped_refptr<InputBuffer> input =
        GetAudioInputBuffer(dmp_reader, buffer_index);
    written_inputs_.push_back(input);
    audio_decoder_->Decode(
        {input}, std::bind(&AdaptiveAudioDecoderTest::OnConsumed, this));
  }

  void WriteMultipleInputs(VideoDmpReader* dmp_reader,
                           size_t buffer_start_index,
                           size_t number_of_inputs_to_write) {
    SB_DCHECK(dmp_reader);

    ASSERT_LT(buffer_start_index + number_of_inputs_to_write,
              dmp_reader->number_of_audio_buffers());

    while (number_of_inputs_to_write > 0) {
      ASSERT_NO_FATAL_FAILURE(WaitAndProcessUntilAcceptInput());
      ASSERT_NO_FATAL_FAILURE(WriteSingleInput(dmp_reader, buffer_start_index));
      ++buffer_start_index;
      --number_of_inputs_to_write;
    }
  }

  void WriteEndOfStream() {
    ASSERT_NO_FATAL_FAILURE(WaitAndProcessUntilAcceptInput());
    audio_decoder_->WriteEndOfStream();
  }

  void WaitAndProcessNextEvent(Event* event) {
    int64_t start = CurrentMonotonicTime();
    while (CurrentMonotonicTime() - start < kWaitForNextEventTimeOut) {
      job_queue_.RunUntilIdle();
      {
        std::lock_guard lock(event_queue_mutex_);
        if (!event_queue_.empty()) {
          *event = event_queue_.front();
          event_queue_.pop_front();
          ProcessEvent(*event);
          return;
        }
      }
      usleep(1000);
    }
    *event = kError;
    FAIL();
  }

  void WaitAndProcessUntilAcceptInput() {
    while (!can_accept_more_input_) {
      Event event = kError;
      ASSERT_NO_FATAL_FAILURE(WaitAndProcessNextEvent(&event));
      if (event != kConsumed) {
        ASSERT_EQ(kOutput, event);
        ASSERT_TRUE(last_decoded_audio_);
      }
    }
  }

  void DrainOutputs() {
    while (!last_decoded_audio_ || !last_decoded_audio_->is_end_of_stream()) {
      Event event = kError;
      ASSERT_NO_FATAL_FAILURE(WaitAndProcessNextEvent(&event));
      if (event != kConsumed) {
        ASSERT_EQ(kOutput, event);
        ASSERT_TRUE(last_decoded_audio_);
      }
    }
  }

  void AssertExpectedAndOutputFramesMatch(int expected_output_frames) {
    if (using_stub_decoder_) {
      // The number of output frames is not applicable in the case of the
      // StubAudioDecoder, because it is not actually doing any decoding work.
      return;
    }
    ASSERT_LE(static_cast<size_t>(
                  std::abs(expected_output_frames - num_of_output_frames_)),
              dmp_readers_.size());
  }

  vector<std::unique_ptr<VideoDmpReader>> dmp_readers_;
  std::optional<DecodedAudio> last_decoded_audio_;
  deque<scoped_refptr<InputBuffer>> written_inputs_;
  int num_of_output_frames_ = 0;
  int output_sample_rate_;
  bool first_output_received_ = false;

 private:
  void OnOutput() {
    std::lock_guard lock(event_queue_mutex_);
    event_queue_.push_back(kOutput);
  }
  void OnError() {
    std::lock_guard lock(event_queue_mutex_);
    event_queue_.push_back(kError);
  }

  void OnConsumed() {
    std::lock_guard lock(event_queue_mutex_);
    event_queue_.push_back(kConsumed);
  }

  void ProcessEvent(Event event) {
    switch (event) {
      case kConsumed: {
        can_accept_more_input_ = true;
        break;
      }
      case kOutput: {
        ReadFromDecoder();
        break;
      }
      case kError: {
        break;
      }
      default:
        SB_NOTREACHED();
    }
  }

  void ReadFromDecoder() {
    int samples_per_second;
    std::optional<DecodedAudio> decoded_audio =
        audio_decoder_->Read(&samples_per_second);
    ASSERT_TRUE(decoded_audio.has_value());
    if (first_output_received_) {
      ASSERT_EQ(output_sample_rate_, samples_per_second);
    } else {
      output_sample_rate_ = samples_per_second;
      first_output_received_ = true;
    }

    if (decoded_audio->is_end_of_stream()) {
      last_decoded_audio_ = std::move(decoded_audio);
      return;
    }

    // Stub decoder doesn't produce outputs with right timestamp.
    if (!using_stub_decoder_) {
      ASSERT_NEAR(decoded_audio->timestamp(),
                  written_inputs_.front()->timestamp(), 5);
      written_inputs_.pop_front();

      // TODO: The timestamps of inputs are not monotonic increasing, so
      // that we can't get monotonic increasing output timestamps here. We
      // can enable the check below once we make the input timestamps monotonic
      // increasing.
      if (last_decoded_audio_) {
        // ASSERT_LE(last_decoded_audio_->timestamp(),
        // decoded_audio->timestamp());
      }
    }

    last_decoded_audio_ = std::move(decoded_audio);
    num_of_output_frames_ += last_decoded_audio_->frames();
  }

  // Test parameter for the filenames to load with the VideoDmpReader.
  std::vector<const char*> test_filenames_;

  // Test parameter to configure whether the test is run with the
  // StubAudioDecoder, or the platform-specific AudioDecoderImpl.
  bool using_stub_decoder_;

  JobQueue job_queue_;
  std::unique_ptr<AudioDecoder> audio_decoder_;

  std::mutex event_queue_mutex_;
  std::deque<Event> event_queue_;
  bool can_accept_more_input_ = true;
};

std::string GetAdaptiveAudioDecoderTestConfigName(
    ::testing::TestParamInfo<std::tuple<vector<const char*>, bool>> info) {
  std::vector<const char*> filenames(std::get<0>(info.param));
  bool using_stub_decoder = std::get<1>(info.param);
  std::string config_name;

  for (auto name : filenames) {
    config_name += std::string(name) + "__to__";
  }
  if (!config_name.empty()) {
    // Remove trailing "__to__".
    config_name.erase(config_name.end() - 6, config_name.end());

    std::replace(config_name.begin(), config_name.end(), '.', '_');
    if (using_stub_decoder) {
      config_name += "__stub";
    }
  }

  return config_name;
}

TEST_P(AdaptiveAudioDecoderTest, SingleInput) {
  int64_t playing_duration = 0;
  // Skip buffer 0, as the difference between first and second opus buffer
  // timestamp is a little larger than it should be.
  size_t buffer_index = 1;
  const size_t kBuffersToWrite = 1;
  for (auto& dmp_reader : dmp_readers_) {
    SB_DCHECK(dmp_reader);
    ASSERT_NO_FATAL_FAILURE(
        WriteMultipleInputs(dmp_reader.get(), buffer_index, kBuffersToWrite));
    auto input_buffer = GetAudioInputBuffer(dmp_reader.get(), buffer_index);
    int64_t input_timestamp = input_buffer->timestamp();
    buffer_index += kBuffersToWrite;
    // Use next buffer here, need to make sure dmp file has enough buffers.
    SB_DCHECK_GT(dmp_reader->number_of_audio_buffers(), buffer_index);
    auto next_input_buffer =
        GetAudioInputBuffer(dmp_reader.get(), buffer_index);
    int64_t next_timestamp = next_input_buffer->timestamp();
    playing_duration += next_timestamp - input_timestamp;
  }
  ASSERT_NO_FATAL_FAILURE(WriteEndOfStream());
  ASSERT_NO_FATAL_FAILURE(DrainOutputs());

  ASSERT_EQ(true, first_output_received_);
  ASSERT_NE(0, output_sample_rate_);
  int expected_output_frames =
      playing_duration * output_sample_rate_ / static_cast<double>(1'000'000);
  // The |num_of_output_frames_| may not accurately match
  // |expected_output_frames|. Each time to switch decoder, it may have one
  // sample difference in output due to integer conversion. The total difference
  // should not exceed the length of |dmp_readers_|.
  AssertExpectedAndOutputFramesMatch(expected_output_frames);
}

TEST_P(AdaptiveAudioDecoderTest, MultipleInput) {
  int64_t playing_duration = 0;
  // Skip buffer 0, as the difference between first and second opus buffer
  // timestamp is a little larger than it should be.
  size_t buffer_index = 1;
  const size_t kBuffersToWrite = 5;
  for (auto& dmp_reader : dmp_readers_) {
    SB_DCHECK(dmp_reader);
    ASSERT_NO_FATAL_FAILURE(
        WriteMultipleInputs(dmp_reader.get(), buffer_index, kBuffersToWrite));
    auto input_buffer = GetAudioInputBuffer(dmp_reader.get(), buffer_index);
    int64_t input_timestamp = input_buffer->timestamp();
    buffer_index += kBuffersToWrite;
    // Use next buffer here, need to make sure dmp file has enough buffers.
    SB_DCHECK_GT(dmp_reader->number_of_audio_buffers(), buffer_index);
    auto next_input_buffer =
        GetAudioInputBuffer(dmp_reader.get(), buffer_index);
    int64_t next_timestamp = next_input_buffer->timestamp();
    playing_duration += next_timestamp - input_timestamp;
  }
  ASSERT_NO_FATAL_FAILURE(WriteEndOfStream());
  ASSERT_NO_FATAL_FAILURE(DrainOutputs());

  ASSERT_EQ(true, first_output_received_);
  ASSERT_NE(0, output_sample_rate_);
  int expected_output_frames =
      playing_duration * output_sample_rate_ / static_cast<double>(1'000'000);
  // The |num_of_output_frames_| may not accurately match
  // |expected_output_frames|. Each time to switch decoder, it may have one
  // sample difference in output due to integer conversion. The total difference
  // should not exceed the length of |dmp_readers_|.
  AssertExpectedAndOutputFramesMatch(expected_output_frames);
}

vector<vector<const char*>> GetSupportedTests() {
  static vector<vector<const char*>> test_params;

  if (!test_params.empty()) {
    return test_params;
  }

  vector<const char*> supported_files = GetSupportedAudioTestFiles(
      kExcludeHeaac, /*max_channels=*/6, kExcludePassthrough);

  // Generate test cases. For example, we have |supported_files| [A, B, C].
  // Add tests A->A, A->B, A->C, B->A, B->B, B->C, C->A, C->B and C->C.
  for (size_t i = 0; i < supported_files.size(); i++) {
    for (size_t j = 0; j < supported_files.size(); j++) {
      test_params.push_back({supported_files[i], supported_files[j]});
    }
  }
  // Add tests A->B->C and C->B->A.
  if (supported_files.size() > 2) {
    test_params.push_back(supported_files);
    test_params.emplace_back(supported_files.rbegin(), supported_files.rend());
  }
  // Add tests A->B->C->A->B->C and C->B->A->C->B->A.
  test_params.push_back(supported_files);
  test_params.back().insert(test_params.back().end(), supported_files.begin(),
                            supported_files.end());
  test_params.emplace_back(test_params.back().rbegin(),
                           test_params.back().rend());

  SB_LOG_IF(INFO, test_params.empty())
      << "Test params for AdaptiveAudioDecoderTests is empty.";
  return test_params;
}

INSTANTIATE_TEST_SUITE_P(AdaptiveAudioDecoderTests,
                         AdaptiveAudioDecoderTest,
                         Combine(ValuesIn(GetSupportedTests()), Bool()),
                         GetAdaptiveAudioDecoderTestConfigName);

class AdaptiveAudioDecoderResetTest : public ::testing::TestWithParam<bool> {
 protected:
  static constexpr uint8_t kHeAacConfig[] = {0x2b, 0x92, 0x08, 0x00};
  static constexpr uint8_t kAacLcConfig[] = {0x12, 0x10};

  scoped_refptr<InputBuffer> CreateAacInput(const uint8_t* config,
                                            uint16_t config_size) {
    static constexpr uint8_t kPayload[] = {0};
    SbPlayerSampleInfo sample_info = {};
    sample_info.type = kSbMediaTypeAudio;
    sample_info.buffer = kPayload;
    sample_info.buffer_size = sizeof(kPayload);
    SbMediaAudioStreamInfo& info = sample_info.audio_sample_info.stream_info;
    info.codec = kSbMediaAudioCodecAac;
    info.mime = "audio/mp4";
    info.number_of_channels = 2;
    info.samples_per_second = 44100;
    info.bits_per_sample = 16;
    info.audio_specific_config_size = config_size;
    info.audio_specific_config = config;
    return new InputBuffer(StubDeallocateSampleFunc, nullptr, nullptr,
                           sample_info);
  }

  scoped_refptr<InputBuffer> HeAacInput() {
    return CreateAacInput(kHeAacConfig, sizeof(kHeAacConfig));
  }
  scoped_refptr<InputBuffer> AacLcInput() {
    return CreateAacInput(kAacLcConfig, sizeof(kAacLcConfig));
  }

  void SetUp() override {
    // Hands out mock decoders that consume every input, and records them so
    // tests can set expectations. The mocks are owned by |decoder_|.
    auto creator = [this](const AudioStreamInfo&, SbDrmSystem) {
      auto mock = std::make_unique<NiceMock<MockAudioDecoder>>(
          kSbMediaAudioSampleTypeFloat32, 44100);
      ON_CALL(*mock, Decode(_, _))
          .WillByDefault([this](const InputBuffers&,
                                const AudioDecoder::ConsumedCB& consumed_cb) {
            job_queue_.Schedule(consumed_cb);
          });
      mock_decoders_.push_back(mock.get());
      return mock;
    };
    auto first_input = HeAacInput();
    decoder_ = std::make_unique<AdaptiveAudioDecoder>(
        &job_queue_, first_input->audio_stream_info(), kSbDrmSystemInvalid,
        creator, /*enable_reset_audio_decoder=*/enable_reset());
    decoder_->Initialize([] {},
                         [](SbPlayerError, const std::string& message) {
                           ADD_FAILURE() << message;
                         });
  }

  bool enable_reset() const { return GetParam(); }

  // Returns whether the input was consumed.
  bool Decode(scoped_refptr<InputBuffer> input) {
    bool consumed = false;
    decoder_->Decode({input}, [&consumed] { consumed = true; });
    job_queue_.RunUntilIdle();
    return consumed;
  }

  JobQueue job_queue_;  // Must outlive |decoder_|.
  // Not owned. An entry dangles once |decoder_| tears that decoder down, so
  // set expectations on it before Reset() or a config change.
  std::vector<MockAudioDecoder*> mock_decoders_;
  std::unique_ptr<AdaptiveAudioDecoder> decoder_;
};

TEST_P(AdaptiveAudioDecoderResetTest, ConfigChangeAfterResetSwapsDecoder) {
  ASSERT_TRUE(Decode(HeAacInput()));
  ASSERT_EQ(mock_decoders_.size(), 1u);
  EXPECT_CALL(*mock_decoders_[0], WriteEndOfStream()).Times(0);

  decoder_->Reset();
  EXPECT_TRUE(Decode(AacLcInput()));
  EXPECT_EQ(mock_decoders_.size(), 2u);
}

TEST_P(AdaptiveAudioDecoderResetTest, SameConfigAfterReset) {
  ASSERT_TRUE(Decode(HeAacInput()));
  ASSERT_EQ(mock_decoders_.size(), 1u);
  EXPECT_CALL(*mock_decoders_[0], WriteEndOfStream()).Times(0);

  decoder_->Reset();
  EXPECT_TRUE(Decode(HeAacInput()));
  EXPECT_EQ(mock_decoders_.size(), enable_reset() ? 1u : 2u);
}

INSTANTIATE_TEST_SUITE_P(AdaptiveAudioDecoderResetTests,
                         AdaptiveAudioDecoderResetTest,
                         Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? std::string("ResetEnabled")
                                             : std::string("ResetDisabled");
                         });

}  // namespace

}  // namespace starboard
