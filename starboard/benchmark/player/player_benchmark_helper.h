// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_BENCHMARK_PLAYER_PLAYER_BENCHMARK_HELPER_H_
#define STARBOARD_BENCHMARK_PLAYER_PLAYER_BENCHMARK_HELPER_H_

#include <memory>

#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/testing/fake_graphics_context_provider.h"

namespace starboard {
class QueueApplication;

namespace benchmark {

class SbPlayerBenchmarkHelper {
 public:
  SbPlayerBenchmarkHelper();
  ~SbPlayerBenchmarkHelper();

  // Generate creation parameters for standard AVC and AAC playback
  void GetCreationParam(SbPlayerCreationParam* out_creation_param,
                        SbPlayerOutputMode output_mode);

  SbWindow GetWindow() { return fake_graphics_context_provider_.window(); }
  SbDecodeTargetGraphicsContextProvider* GetDecoderTargetProvider() {
    return fake_graphics_context_provider_.decoder_target_provider();
  }

  // Dummy callbacks required by SbPlayerCreate
  static void DummyDeallocateSampleFunc(SbPlayer player,
                                        void* context,
                                        const void* sample_buffer);
  static void DummyDecoderStatusFunc(SbPlayer player,
                                     void* context,
                                     SbMediaType type,
                                     SbPlayerDecoderState state,
                                     int ticket);
  static void DummyPlayerStatusFunc(SbPlayer player,
                                    void* context,
                                    SbPlayerState state,
                                    int ticket);
  static void DummyPlayerErrorFunc(SbPlayer player,
                                   void* context,
                                   SbPlayerError error,
                                   const char* message);

 private:
  std::unique_ptr<starboard::QueueApplication> mock_application_;
  starboard::FakeGraphicsContextProvider fake_graphics_context_provider_;
};

}  // namespace benchmark
}  // namespace starboard

#endif  // STARBOARD_BENCHMARK_PLAYER_PLAYER_BENCHMARK_HELPER_H_
