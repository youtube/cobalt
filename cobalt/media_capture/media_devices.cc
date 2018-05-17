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
#include "cobalt/speech/microphone_fake.h"
#include "cobalt/speech/microphone_starboard.h"
#include "starboard/string.h"

namespace cobalt {
namespace media_capture {

#if SB_USE_SB_MICROPHONE && !defined(DISABLE_MICROPHONE_IDL)
#define ENABLE_MICROPHONE_IDL
#endif

namespace {

using speech::Microphone;

scoped_ptr<Microphone> CreateMicrophone(const Microphone::Options& options) {
#if defined(ENABLE_FAKE_MICROPHONE)
  if (options.enable_fake_microphone) {
    return make_scoped_ptr<speech::Microphone>(
        new speech::MicrophoneFake(options));
  }
#else
  UNREFERENCED_PARAMETER(options);
#endif  // defined(ENABLE_FAKE_MICROPHONE)

  scoped_ptr<Microphone> mic;

#if defined(ENABLE_MICROPHONE_IDL)
  mic.reset(new speech::MicrophoneStarboard(
      speech::MicrophoneStarboard::kDefaultSampleRate,
      /* Buffer for one second. */
      speech::MicrophoneStarboard::kDefaultSampleRate *
          speech::MicrophoneStarboard::kSbMicrophoneSampleSizeInBytes));
#endif  // defined(ENABLE_MICROPHONE_IDL)

  return mic.Pass();
}

}  // namespace.

MediaDevices::MediaDevices(script::ScriptValueFactory* script_value_factory)
    : script_value_factory_(script_value_factory) {
  DCHECK(script_value_factory);
}

script::Handle<MediaDevices::MediaInfoSequencePromise>
MediaDevices::EnumerateDevices() {
  DCHECK(settings_);
  script::Handle<MediaInfoSequencePromise> promise =
      script_value_factory_->CreateBasicPromise<MediaInfoSequence>();
  script::Sequence<scoped_refptr<Wrappable>> output;

  scoped_ptr<speech::Microphone> microphone =
      CreateMicrophone(settings_->microphone_options());
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
