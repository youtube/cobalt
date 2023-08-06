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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_TESTING_TEST_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_TESTING_TEST_UTIL_H_

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_sink.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {

typedef std::tuple<const char*, SbPlayerOutputMode> VideoTestParam;

enum HeaacOption {
  kIncludeHeaac,
  kExcludeHeaac,
};

// The function doesn't free the buffer, it assumes that the lifetime of the
// buffer is actually managed by other code.  It can be used in the places where
// SbPlayerDeallocateSampleFunc is expected.
void StubDeallocateSampleFunc(SbPlayer player,
                              void* context,
                              const void* sample_buffer);

std::vector<const char*> GetSupportedAudioTestFiles(
    HeaacOption heaac_option,
    int max_channels,
    const char* extra_mime_attributes = "");
std::vector<VideoTestParam> GetSupportedVideoTests();

bool CreateAudioComponents(bool using_stub_decoder,
                           const media::AudioStreamInfo& audio_stream_info,
                           scoped_ptr<AudioDecoder>* audio_decoder,
                           scoped_ptr<AudioRendererSink>* audio_renderer_sink);

::testing::AssertionResult AlmostEqualTime(SbTime time1, SbTime time2);

media::VideoStreamInfo CreateVideoStreamInfo(SbMediaVideoCodec codec);

bool IsPartialAudioSupported();

scoped_refptr<InputBuffer> GetAudioInputBuffer(
    video_dmp::VideoDmpReader* dmp_reader,
    size_t index);

scoped_refptr<InputBuffer> GetAudioInputBuffer(
    video_dmp::VideoDmpReader* dmp_reader,
    size_t index,
    SbTime discarded_duration_from_front,
    SbTime discarded_duration_from_back);

}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_TESTING_TEST_UTIL_H_
