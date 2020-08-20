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

#ifndef COBALT_SPEECH_ENDPOINTER_DELEGATE_H_
#define COBALT_SPEECH_ENDPOINTER_DELEGATE_H_

#include "cobalt/media/base/audio_bus.h"
#include "content/browser/speech/endpointer/endpointer.h"

namespace cobalt {
namespace speech {

// Delegate of endpointer for detecting the first time sound started in one
// speech session (from start speaking to end of speaking).
class EndPointerDelegate {
 public:
  typedef media::AudioBus AudioBus;

  explicit EndPointerDelegate(int sample_rate);
  ~EndPointerDelegate();

  // Start endpointer session for sound processing.
  void Start();
  // Stop endpointer session.
  void Stop();

  // Return true if it is the first time that the sound started.
  bool IsFirstTimeSoundStarted(const AudioBus& audio_bus);

 private:
  // Used for detecting sound start event.
  content::Endpointer endpointer_;
  // Used for recording the number of samples before notifying that it is the
  // first time the sound started. The |endpointer_| should be started and run
  // in the EnvironmentEstimation mode if used in noisy conditions for at least
  // 200ms before switching to UserInputMode.
  int num_samples_recorded_;

  // Record if it is the first time that the sound started.
  bool is_first_time_sound_started_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_ENDPOINTER_DELEGATE_H_
