// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <memory>
#include <string>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/media_capture/media_device_info.h"
#include "cobalt/media_stream/media_stream.h"
#include "cobalt/media_stream/media_track_settings.h"
#include "cobalt/media_stream/microphone_audio_source.h"
#include "cobalt/speech/microphone.h"
#include "cobalt/speech/microphone_fake.h"
#include "cobalt/speech/microphone_starboard.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace media_capture {

#if SB_USE_SB_MICROPHONE && !defined(DISABLE_MICROPHONE_IDL)
#define ENABLE_MICROPHONE_IDL
#endif

namespace {

using speech::Microphone;

std::unique_ptr<Microphone> CreateMicrophone(
    const Microphone::Options& options) {
#if defined(ENABLE_FAKE_MICROPHONE)
  if (options.enable_fake_microphone) {
    return std::unique_ptr<speech::Microphone>(
        new speech::MicrophoneFake(options));
  }
#else
#endif  // defined(ENABLE_FAKE_MICROPHONE)

  std::unique_ptr<Microphone> mic;

#if defined(ENABLE_MICROPHONE_IDL)
  mic.reset(new speech::MicrophoneStarboard(
      speech::MicrophoneStarboard::kDefaultSampleRate,
      /* Buffer for one second. */
      speech::MicrophoneStarboard::kDefaultSampleRate *
          speech::MicrophoneStarboard::kSbMicrophoneSampleSizeInBytes));
#endif  // defined(ENABLE_MICROPHONE_IDL)

  return mic;
}

}  // namespace.

MediaDevices::MediaDevices(script::EnvironmentSettings* settings,
                           script::ScriptValueFactory* script_value_factory)
    : dom::EventTarget(settings),
      settings_(base::polymorphic_downcast<dom::DOMSettings*>(settings)),
      script_value_factory_(script_value_factory),
      javascript_message_loop_(base::MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      weak_this_(weak_ptr_factory_.GetWeakPtr()) {}

script::Handle<MediaDevices::MediaInfoSequencePromise>
MediaDevices::EnumerateDevices() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(settings_);
  DCHECK(script_value_factory_);
  script::Handle<MediaInfoSequencePromise> promise =
      script_value_factory_->CreateBasicPromise<MediaInfoSequence>();
  script::Sequence<scoped_refptr<Wrappable>> output;

  std::unique_ptr<speech::Microphone> microphone =
      CreateMicrophone(settings_->microphone_options());
  if (microphone && microphone->IsValid()) {
    scoped_refptr<Wrappable> media_device(
        new MediaDeviceInfo(kMediaDeviceKindAudioinput, microphone->Label()));
    output.push_back(media_device);
  }
  promise->Resolve(output);
  return promise;
}

script::Handle<MediaDevices::MediaStreamPromise> MediaDevices::GetUserMedia() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

  // Access Microphone Device
  // Create Audio Source (attach a device to it), source will periodically read
  // from the device, and push out data to ALL of the tracks
  if (!audio_source_) {
    audio_source_ = new media_stream::MicrophoneAudioSource(
        settings_->microphone_options(),
        base::Bind(&MediaDevices::OnMicrophoneSuccess, weak_this_),
        base::Closure(),  // TODO: remove this redundant callback.
        base::Bind(&MediaDevices::OnMicrophoneError, weak_this_));
    audio_source_->SetStopCallback(
        base::Bind(&MediaDevices::OnMicrophoneStopped, weak_this_));
  }

  std::unique_ptr<MediaStreamPromiseValue::Reference> promise_reference(
      new MediaStreamPromiseValue::Reference(this, promise));
  pending_microphone_promises_.push_back(std::move(promise_reference));

  if (!pending_microphone_track_) {
    pending_microphone_track_ =
        new media_stream::MediaStreamAudioTrack(settings_);
    // Starts the source, if needed.  Also calls start on the audio track.
    audio_source_->ConnectToTrack(
        base::polymorphic_downcast<media_stream::MediaStreamAudioTrack*>(
            pending_microphone_track_.get()));
  }

  // Step 10, return promise.
  return promise;
}

void MediaDevices::OnMicrophoneError(
    speech::MicrophoneManager::MicrophoneError error, std::string message) {
  DLOG(INFO) << "MediaDevices::OnMicrophoneError " << message;

  // No special error handling logic besides logging the message above, so just
  // delegate to the OnMicrophoneStopped() functionality.
  OnMicrophoneStopped();
}

void MediaDevices::OnMicrophoneStopped() {
  if (javascript_message_loop_->task_runner() !=
      base::MessageLoop::current()->task_runner()) {
    javascript_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&MediaDevices::OnMicrophoneStopped, weak_this_));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DLOG(INFO) << "MediaDevices::OnMicrophoneStopped()";
  pending_microphone_track_ = nullptr;
  audio_source_ = nullptr;

  for (auto& promise : pending_microphone_promises_) {
    promise->value().Reject(
        new dom::DOMException(dom::DOMException::kNotAllowedErr));
  }
  pending_microphone_promises_.clear();
}

void MediaDevices::OnMicrophoneSuccess() {
  if (javascript_message_loop_->task_runner() !=
      base::MessageLoop::current()->task_runner()) {
    javascript_message_loop_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&MediaDevices::OnMicrophoneSuccess, this));
    return;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  using media_stream::MediaStream;
  MediaStream::TrackSequences audio_tracks;
  pending_microphone_track_->SetMediaTrackSettings(
      audio_source_->GetMediaTrackSettings());
  audio_tracks.push_back(pending_microphone_track_);
  pending_microphone_track_ = nullptr;

  for (auto& promise : pending_microphone_promises_) {
    promise->value().Resolve(
        base::WrapRefCounted(new MediaStream(settings_, audio_tracks)));
  }
  pending_microphone_promises_.clear();
}

}  // namespace media_capture
}  // namespace cobalt
