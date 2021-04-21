// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "third_party/google_benchmark/include/benchmark/benchmark.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

using video_dmp::VideoDmpReader;

const size_t kMaxNumberOfInputs = 256;

class AudioDecoderHelper {
 public:
  explicit AudioDecoderHelper(const char* filename)
      : dmp_reader_(ResolveTestFileName(filename).c_str()),
        number_of_inputs_(std::min(dmp_reader_.number_of_audio_buffers(),
                                   kMaxNumberOfInputs)) {
    const bool kUseStubDecoder = false;
    SB_CHECK(number_of_inputs_ > 0);
    SB_CHECK(CreateAudioComponents(kUseStubDecoder, dmp_reader_.audio_codec(),
                                   dmp_reader_.audio_sample_info(),
                                   &audio_decoder_, &audio_renderer_sink_));
    SB_CHECK(audio_decoder_);
    audio_decoder_->Initialize(std::bind(&AudioDecoderHelper::OnOutput, this),
                               std::bind(&AudioDecoderHelper::OnError, this));
  }

  size_t number_of_inputs() const { return number_of_inputs_; }

  void DecodeAll() {
    SB_CHECK(current_input_buffer_index_ == 0);
    OnConsumed();  // Kick off the first Decode() call
    // Note that we deliberately don't add any time out to the loop, to ensure
    // that the benchmark is accurate.
    while (!end_of_stream_decoded_) {
      job_queue_.RunUntilIdle();
    }
  }

 private:
  scoped_refptr<InputBuffer> GetAudioInputBuffer(size_t index) {
    auto player_sample_info =
        dmp_reader_.GetPlayerSampleInfo(kSbMediaTypeAudio, index);
    return new InputBuffer(StubDeallocateSampleFunc, NULL, NULL,
                           player_sample_info);
  }

  void OnOutput() {
    if (!job_queue_.BelongsToCurrentThread()) {
      job_queue_.Schedule(std::bind(&AudioDecoderHelper::OnOutput, this));
      return;
    }
    int decoded_sample_rate;
    auto decoded_audio = audio_decoder_->Read(&decoded_sample_rate);
    end_of_stream_decoded_ = decoded_audio->is_end_of_stream();
  }

  void OnError() { SB_NOTREACHED(); }

  void OnConsumed() {
    if (!job_queue_.BelongsToCurrentThread()) {
      job_queue_.Schedule(std::bind(&AudioDecoderHelper::OnConsumed, this));
      return;
    }
    if (current_input_buffer_index_ < number_of_inputs_) {
      audio_decoder_->Decode(GetAudioInputBuffer(current_input_buffer_index_),
                             std::bind(&AudioDecoderHelper::OnConsumed, this));
      ++current_input_buffer_index_;
    } else {
      SB_CHECK(current_input_buffer_index_ == number_of_inputs_);
      audio_decoder_->WriteEndOfStream();
      // Increment so we can know if WriteEndOfStream() is called twice.
      ++current_input_buffer_index_;
    }
  }

  VideoDmpReader dmp_reader_;
  JobQueue job_queue_;

  const size_t number_of_inputs_;
  size_t current_input_buffer_index_ = 0;
  bool end_of_stream_decoded_ = false;

  scoped_ptr<AudioDecoder> audio_decoder_;
  scoped_ptr<AudioRendererSink> audio_renderer_sink_;
};

}  // namespace

void RunBenchmark(::benchmark::State& state, const char* filename) {
  size_t number_of_inputs = 0;
  for (auto _ : state) {
    state.PauseTiming();
    AudioDecoderHelper helper(filename);
    number_of_inputs = helper.number_of_inputs();
    state.ResumeTiming();
    helper.DecodeAll();
  }
  state.SetItemsProcessed(state.iterations() * number_of_inputs);
}

}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

// This function has to reside in the global namespace for BENCHMARK_CAPTURE to
// pick it up.
void BM_AudioDecoder(::benchmark::State& state, const char* filename) {
  starboard::shared::starboard::player::filter::testing::RunBenchmark(state,
                                                                      filename);
}

BENCHMARK_CAPTURE(BM_AudioDecoder,
                  aac_stereo,
                  "beneath_the_canopy_aac_stereo.dmp");
BENCHMARK_CAPTURE(BM_AudioDecoder,
                  opus_stereo,
                  "beneath_the_canopy_opus_stereo.dmp");
