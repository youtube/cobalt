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

#include "cobalt/dom/dom_exception.h"
#include "cobalt/media_capture/media_device_info.h"
#include "cobalt/media_stream/media_stream.h"
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
}

script::Handle<MediaDevices::MediaInfoSequencePromise>
MediaDevices::EnumerateDevices() {
  DCHECK(settings_);
  DCHECK(script_value_factory_);
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

script::Handle<MediaDevices::MediaStreamPromise> MediaDevices::GetUserMedia() {
  DCHECK(script_value_factory_);
  script::Handle<MediaDevices::MediaStreamPromise> promise =
      script_value_factory_->CreateInterfacePromise<
          script::ScriptValueFactory::WrappablePromise>();
  // Per specification at
  // https://w3c.github.io/mediacapture-main/#dom-mediadevices-getusermedia
  //
  // Step 3: If requestedMediaTypes is the empty set, return a promise rejected
  // with a TypeError. The word "optional" occurs in the WebIDL due to WebIDL
  // rules, but the argument must be supplied in order for the call to succeed.

  promise->Reject(script::kTypeError);
  return promise;
}

script::Handle<MediaDevices::MediaStreamPromise> MediaDevices::GetUserMedia(
    const media_stream::MediaStreamConstraints& constraints) {
  script::Handle<MediaDevices::MediaStreamPromise> promise =
      script_value_factory_->CreateInterfacePromise<
          script::ScriptValueFactory::WrappablePromise>();
  if (!constraints.audio()) {
    // Step 3: If requestedMediaTypes is the empty set, return a promise
    // rejected with a TypeError. The word "optional" occurs in the WebIDL due
    // to WebIDL rules, but the argument must be supplied in order for the call
    // to succeed.
    DLOG(INFO) << "Audio constraint must be true.";
    promise->Reject(script::kTypeError);
    return promise;
  }
  // Steps 4-7 are not needed for cobalt.
  // Step 8 is to create a promise (which is already done).

  // Step 9: Construct a list of MediaStreamTracks that we have permission.
  CreateMicrophoneIfNeeded();
  if (!microphone_) {
    promise->Reject(new dom::DOMException(dom::DOMException::kNotFoundErr));
    return promise;
  }
  bool open_success = microphone_->Open();
  if (!open_success) {
    // This typically happens if the user did not give permission.
    // Step 9.8, handle "Permission Failure"
    DLOG(INFO) << "Unable to open the microphone.";
    promise->Reject(new dom::DOMException(dom::DOMException::kNotAllowedErr));
  }
  using media_stream::MediaStream;
  scoped_refptr<media_stream::MediaStreamTrack> microphone_track;
  // TODO: Attach a microphone device to the microphone track.
  MediaStream::TrackSequences audio_tracks;
  audio_tracks.push_back(microphone_track);
  auto media_stream(make_scoped_refptr(new MediaStream(audio_tracks)));
  promise->Resolve(media_stream);

  // Step 10, return promise.
  return promise;
}

void MediaDevices::CreateMicrophoneIfNeeded() {
  DCHECK(settings_);
  if (microphone_) {
    return;
  }

  scoped_ptr<speech::Microphone> microphone =
      CreateMicrophone(settings_->microphone_options());

  if (!microphone) {
    DLOG(INFO) << "Unable to create a microphone.";
    return;
  }

  if (!microphone->IsValid()) {
    DLOG(INFO) << "Ignoring created microphone because it is invalid.";
    return;
  }

  DLOG(INFO) << "Created microphone: " << microphone->Label();
  microphone_ = microphone.Pass();
}

}  // namespace media_capture
}  // namespace cobalt
