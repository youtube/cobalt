// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/speech/endpointer_delegate.h"

#include "base/time/time.h"

namespace cobalt {
namespace speech {

namespace {
const int kEndPointerEstimationTimeInMillisecond = 300;
// If used in noisy conditions, the endpointer should be started and run in the
// EnvironmentEstimation mode, for at least 200ms, before switching to
// UserInputMode.
COMPILE_ASSERT(kEndPointerEstimationTimeInMillisecond >= 200,
               in_environment_estimation_mode_for_at_least_200ms);
}  // namespace

EndPointerDelegate::EndPointerDelegate(int sample_rate)
    : endpointer_(sample_rate),
      num_samples_recorded_(0),
      is_first_time_sound_started_(false) {}

EndPointerDelegate::~EndPointerDelegate() {}

void EndPointerDelegate::Start() {
  num_samples_recorded_ = 0;
  is_first_time_sound_started_ = false;

  endpointer_.StartSession();
  endpointer_.SetEnvironmentEstimationMode();
}

void EndPointerDelegate::Stop() { endpointer_.EndSession(); }

bool EndPointerDelegate::IsFirstTimeSoundStarted(const AudioBus& audio_bus) {
  if (is_first_time_sound_started_) {
    return false;
  }

  num_samples_recorded_ += static_cast<int>(audio_bus.frames());
  if (endpointer_.IsEstimatingEnvironment() &&
      num_samples_recorded_ * 1000 / endpointer_.sample_rate() >
          kEndPointerEstimationTimeInMillisecond) {
    // Switch to user input mode.
    endpointer_.SetUserInputMode();
  }

  endpointer_.ProcessAudio(audio_bus, NULL);
  int64_t ep_time;
  content::EpStatus status = endpointer_.Status(&ep_time);
  if (status == content::EP_POSSIBLE_ONSET) {
    is_first_time_sound_started_ = true;
  }

  return is_first_time_sound_started_;
}

}  // namespace speech
}  // namespace cobalt
