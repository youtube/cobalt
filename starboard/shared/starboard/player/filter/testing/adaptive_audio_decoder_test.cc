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

#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <numeric>
#include <queue>

#include "starboard/common/mutex.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/player_components.h"
#include "starboard/shared/starboard/player/filter/stub_player_components_factory.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO: Implement AudioDecoderMock and refactor the test accordingly.
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
using std::vector;
using std::string;
using video_dmp::VideoDmpReader;

const SbTimeMonotonic kWaitForNextEventTimeOut = 5 * kSbTimeSecond;

scoped_refptr<InputBuffer> GetAudioInputBuffer(VideoDmpReader* dmp_reader,
                                               size_t index) {
  SB_DCHECK(dmp_reader);

  auto player_sample_info =
      dmp_reader->GetPlayerSampleInfo(kSbMediaTypeAudio, index);
  return new InputBuffer(StubDeallocateSampleFunc, NULL, NULL,
                         player_sample_info);
}

string GetTestInputDirectory() {
  const size_t kPathSize = kSbFileMaxPath + 1;

  std::vector<char> content_path(kPathSize);
  SB_CHECK(SbSystemGetPath(kSbSystemPathContentDirectory, content_path.data(),
                           kPathSize));
  string directory_path =
      string(content_path.data()) + kSbFileSepChar + "test" + kSbFileSepChar +
      "starboard" + kSbFileSepChar + "shared" + kSbFileSepChar + "starboard" +
      kSbFileSepChar + "player" + kSbFileSepChar + "testdata";

  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str()))
      << "Cannot open directory " << directory_path;
  return directory_path;
}

string ResolveTestFileName(const char* filename) {
  return GetTestInputDirectory() + kSbFileSepChar + filename;
}

class AdaptiveAudioDecoderTest
    : public ::testing::TestWithParam<std::tuple<vector<const char*>, bool>> {
 protected:
  enum Event { kConsumed, kOutput, kError };

  AdaptiveAudioDecoderTest()
      : test_filenames_(std::get<0>(GetParam())),
        using_stub_decoder_(std::get<1>(GetParam())) {
    for (auto filename : test_filenames_) {
      dmp_readers_.emplace_back(
          new VideoDmpReader(ResolveTestFileName(filename).c_str(),
                             VideoDmpReader::kEnableReadOnDemand));
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
    ASSERT_GT(dmp_readers_.size(), 0);
    for (auto& dmp_reader : dmp_readers_) {
      ASSERT_NE(dmp_reader->audio_codec(), kSbMediaAudioCodecNone);
      ASSERT_GT(dmp_reader->number_of_audio_buffers(), 0);
    }

    scoped_ptr<AudioRendererSink> audio_renderer_sink;
    ASSERT_TRUE(CreateAudioComponents(using_stub_decoder_,
                                      dmp_readers_[0]->audio_codec(),
                                      dmp_readers_[0]->audio_sample_info(),
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
    audio_decoder_->Decode(
        GetAudioInputBuffer(dmp_reader, buffer_index),
        std::bind(&AdaptiveAudioDecoderTest::OnConsumed, this));
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
    SbTimeMonotonic start = SbTimeGetMonotonicNow();
    while (SbTimeGetMonotonicNow() - start < kWaitForNextEventTimeOut) {
      job_queue_.RunUntilIdle();
      {
        ScopedLock scoped_lock(event_queue_mutex_);
        if (!event_queue_.empty()) {
          *event = event_queue_.front();
          event_queue_.pop_front();
          ProcessEvent(*event);
          return;
        }
      }
      SbThreadSleep(kSbTimeMillisecond);
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
    ASSERT_LE(abs(expected_output_frames - num_of_output_frames_),
              dmp_readers_.size());
  }

  vector<std::unique_ptr<VideoDmpReader>> dmp_readers_;
  scoped_refptr<DecodedAudio> last_decoded_audio_;
  int num_of_output_frames_ = 0;
  int output_sample_rate_;
  bool first_output_received_ = false;

 private:
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
    scoped_refptr<DecodedAudio> decoded_audio =
        audio_decoder_->Read(&samples_per_second);
    ASSERT_TRUE(decoded_audio);
    if (first_output_received_) {
      ASSERT_EQ(output_sample_rate_, samples_per_second);
    } else {
      output_sample_rate_ = samples_per_second;
      first_output_received_ = true;
    }

    if (decoded_audio->is_end_of_stream()) {
      last_decoded_audio_ = decoded_audio;
      return;
    }
    // TODO: fix resampler timestamp issue.
    // if (last_decoded_audio_) {
    //   ASSERT_LT(last_decoded_audio_->timestamp(),
    //             decoded_audio->timestamp());
    // }
    last_decoded_audio_ = decoded_audio;
    num_of_output_frames_ += last_decoded_audio_->frames();
  }

  // Test parameter for the filenames to load with the VideoDmpReader.
  std::vector<const char*> test_filenames_;

  // Test parameter to configure whether the test is run with the
  // StubAudioDecoder, or the platform-specific AudioDecoderImpl.
  bool using_stub_decoder_;

  JobQueue job_queue_;
  scoped_ptr<AudioDecoder> audio_decoder_;

  Mutex event_queue_mutex_;
  std::deque<Event> event_queue_;
  bool can_accept_more_input_ = true;
};

TEST_P(AdaptiveAudioDecoderTest, SingleInput) {
  SbTime playing_duration = 0;
  // Skip buffer 0, as the difference between first and second opus buffer
  // timestamp is a little larger than it should be.
  size_t buffer_index = 1;
  const size_t kBuffersToWrite = 1;
  for (auto& dmp_reader : dmp_readers_) {
    SB_DCHECK(dmp_reader);
    ASSERT_NO_FATAL_FAILURE(
        WriteMultipleInputs(dmp_reader.get(), buffer_index, kBuffersToWrite));
    auto input_buffer = GetAudioInputBuffer(dmp_reader.get(), buffer_index);
    SbTime input_timestamp = input_buffer->timestamp();
    buffer_index += kBuffersToWrite;
    // Use next buffer here, need to make sure dmp file has enough buffers.
    SB_DCHECK(dmp_reader->number_of_audio_buffers() > buffer_index);
    auto next_input_buffer =
        GetAudioInputBuffer(dmp_reader.get(), buffer_index);
    SbTime next_timestamp = next_input_buffer->timestamp();
    playing_duration += next_timestamp - input_timestamp;
  }
  ASSERT_NO_FATAL_FAILURE(WriteEndOfStream());
  ASSERT_NO_FATAL_FAILURE(DrainOutputs());

  ASSERT_EQ(true, first_output_received_);
  ASSERT_NE(0, output_sample_rate_);
  int expected_output_frames = playing_duration * output_sample_rate_ /
                               static_cast<double>(kSbTimeSecond);
  // The |num_of_output_frames_| may not accurately match
  // |expected_output_frames|. Each time to switch decoder, it may have one
  // sample difference in output due to integer conversion. The total difference
  // should not exceed the length of |dmp_readers_|.
  AssertExpectedAndOutputFramesMatch(expected_output_frames);
}

TEST_P(AdaptiveAudioDecoderTest, MultipleInput) {
  SbTime playing_duration = 0;
  // Skip buffer 0, as the difference between first and second opus buffer
  // timestamp is a little larger than it should be.
  size_t buffer_index = 1;
  const size_t kBuffersToWrite = 5;
  for (auto& dmp_reader : dmp_readers_) {
    SB_DCHECK(dmp_reader);
    ASSERT_NO_FATAL_FAILURE(
        WriteMultipleInputs(dmp_reader.get(), buffer_index, kBuffersToWrite));
    auto input_buffer = GetAudioInputBuffer(dmp_reader.get(), buffer_index);
    SbTime input_timestamp = input_buffer->timestamp();
    buffer_index += kBuffersToWrite;
    // Use next buffer here, need to make sure dmp file has enough buffers.
    SB_DCHECK(dmp_reader->number_of_audio_buffers() > buffer_index);
    auto next_input_buffer =
        GetAudioInputBuffer(dmp_reader.get(), buffer_index);
    SbTime next_timestamp = next_input_buffer->timestamp();
    playing_duration += next_timestamp - input_timestamp;
  }
  ASSERT_NO_FATAL_FAILURE(WriteEndOfStream());
  ASSERT_NO_FATAL_FAILURE(DrainOutputs());

  ASSERT_EQ(true, first_output_received_);
  ASSERT_NE(0, output_sample_rate_);
  int expected_output_frames = playing_duration * output_sample_rate_ /
                               static_cast<double>(kSbTimeSecond);
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

  vector<const char*> supported_files =
      GetSupportedAudioTestFiles(kExcludeHeaac, 6, "audiopassthrough=false");

  // Generate test cases. For example, we have |supported_files| [A, B, C].
  // Add tests A->A, A->B, A->C, B->A, B->B, B->C, C->A, C->B and C->C.
  for (int i = 0; i < supported_files.size(); i++) {
    for (int j = 0; j < supported_files.size(); j++) {
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
      << "Test params for AdaptiveAudioDecodeTests is empty.";
  return test_params;
}

INSTANTIATE_TEST_CASE_P(AdaptiveAudioDecoderTests,
                        AdaptiveAudioDecoderTest,
                        Combine(ValuesIn(GetSupportedTests()), Bool()));

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
