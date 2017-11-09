// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/media_capture/media_devices.h"

#include "cobalt/media_capture/media_device_info.h"
#include "starboard/microphone.h"

namespace cobalt {
namespace media_capture {

namespace {

int CountMicrophones() {
#if SB_HAS(MICROPHONE)
  SbMicrophoneInfo info;
  int num_mics = SbMicrophoneGetAvailable(&info, 1);
  return num_mics;
#else
  return 0;
#endif
}

}  // namespace.

using PromiseSequenceMediaInfo = MediaDevices::PromiseSequenceMediaInfo;

MediaDevices::MediaDevices(script::ScriptValueFactory* script_value_factory)
    : script_value_factory_(script_value_factory) {
}

scoped_ptr<PromiseSequenceMediaInfo> MediaDevices::EnumerateDevices() {
  scoped_ptr<PromiseSequenceMediaInfo> promise =
      script_value_factory_->CreateBasicPromise<
          script::Sequence<scoped_refptr<script::Wrappable>>>();

  MediaDevices::PromiseSequenceMediaInfo::StrongReference promise_reference(
      *promise);
  script::Sequence<scoped_refptr<Wrappable>> output;
  const int n_microphones = CountMicrophones();
  for (int i = 0; i < n_microphones; ++i) {
    output.push_back(new MediaDeviceInfo(script_value_factory_,
                                         kMediaDeviceKindAudioinput));
  }

  promise_reference.value().Resolve(output);
  return promise.Pass();
}

}  // namespace media_capture
}  // namespace cobalt
