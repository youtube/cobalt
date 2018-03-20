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

#include <string>

#include "cobalt/media_capture/media_device_info.h"
#include "cobalt/speech/microphone.h"
#include "cobalt/speech/microphone_starboard.h"
#include "starboard/string.h"

namespace cobalt {
namespace media_capture {

#if SB_USE_SB_MICROPHONE && !defined(DISABLE_MICROPHONE_IDL)
#define ENABLE_MICROPHONE_IDL
#endif

namespace {

scoped_ptr<speech::Microphone> CreateMicrophone() {
  scoped_ptr<speech::Microphone> mic;
#ifdef ENABLE_MICROPHONE_IDL
  mic.reset(new speech::MicrophoneStarboard(
      speech::MicrophoneStarboard::kDefaultSampleRate,
      speech::MicrophoneStarboard::kDefaultSampleRate));
#endif
  return mic.Pass();
}

}  // namespace.

MediaDevices::MediaDevices(script::ScriptValueFactory* script_value_factory)
    : script_value_factory_(script_value_factory) {
}

script::Handle<MediaDevices::MediaInfoSequencePromise>
MediaDevices::EnumerateDevices() {
  script::Handle<MediaInfoSequencePromise> promise =
      script_value_factory_->CreateBasicPromise<MediaInfoSequence>();
  script::Sequence<scoped_refptr<Wrappable>> output;
  scoped_ptr<speech::Microphone> microphone = CreateMicrophone();
  if (microphone) {
    scoped_refptr<Wrappable> media_device(
        new MediaDeviceInfo(script_value_factory_, kMediaDeviceKindAudioinput,
                            microphone->Label()));
    output.push_back(media_device);
  }

  promise->Resolve(output);
  return promise;
}

}  // namespace media_capture
}  // namespace cobalt
