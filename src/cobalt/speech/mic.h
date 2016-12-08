/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_SPEECH_MIC_H_
#define COBALT_SPEECH_MIC_H_

#include "base/callback.h"
#include "media/base/audio_bus.h"

namespace cobalt {
namespace speech {

// An abstract class is used for interacting platform specific microphone.
class Mic {
 public:
  typedef ::media::AudioBus AudioBus;
  typedef base::Callback<void(scoped_ptr<AudioBus>)> DataReceivedCallback;
  typedef base::Callback<void(void)> CompletionCallback;
  typedef base::Callback<void(void)> ErrorCallback;

  virtual ~Mic() {}

  static scoped_ptr<Mic> Create(int sample_rate,
                                const DataReceivedCallback& data_received,
                                const CompletionCallback& completion,
                                const ErrorCallback& error);

  // Multiple calls to Start/Stop are allowed, the implementation should take
  // care of multiple calls. The Start/Stop methods are required to be called in
  // the same thread.
  // Start microphone to receive voice.
  virtual void Start() = 0;
  // Stop microphone from receiving voice.
  virtual void Stop() = 0;

 protected:
  Mic(int sample_rate, const DataReceivedCallback& data_received,
      const CompletionCallback& completion, const ErrorCallback& error)
      : sample_rate_(sample_rate),
        data_received_callback_(data_received),
        completion_callback_(completion),
        error_callback_(error) {}

  int sample_rate_;
  const DataReceivedCallback data_received_callback_;
  const CompletionCallback completion_callback_;
  const ErrorCallback error_callback_;
};

}  // namespace speech
}  // namespace cobalt

#endif  // COBALT_SPEECH_MIC_H_
