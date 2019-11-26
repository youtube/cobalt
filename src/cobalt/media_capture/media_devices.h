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

#ifndef COBALT_MEDIA_CAPTURE_MEDIA_DEVICES_H_
#define COBALT_MEDIA_CAPTURE_MEDIA_DEVICES_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media_capture/media_device_info.h"
#include "cobalt/media_stream/media_stream_audio_source.h"
#include "cobalt/media_stream/media_stream_constraints.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/microphone.h"
#include "cobalt/speech/microphone_manager.h"

namespace cobalt {
namespace media_capture {

// The MediaDevices object is the entry point to the API used to examine and
// get access to media devices available to the User Agent.
//   https://www.w3.org/TR/mediacapture-streams/#mediadevices
class MediaDevices : public dom::EventTarget {
 public:
  using MediaInfoSequence = script::Sequence<scoped_refptr<script::Wrappable>>;
  using MediaInfoSequencePromise = script::Promise<MediaInfoSequence>;
  using MediaStreamPromise =
      script::Promise<script::ScriptValueFactory::WrappablePromise>;
  using MediaStreamPromiseValue = script::ScriptValue<MediaStreamPromise>;

  explicit MediaDevices(script::EnvironmentSettings* settings,
                        script::ScriptValueFactory* script_value_factory);

  script::Handle<MediaInfoSequencePromise> EnumerateDevices();
  script::Handle<MediaStreamPromise> GetUserMedia();
  script::Handle<MediaStreamPromise> GetUserMedia(
      const media_stream::MediaStreamConstraints& constraints);

  DEFINE_WRAPPABLE_TYPE(MediaDevices);

 private:
  friend class GetUserMediaTest;
  FRIEND_TEST_ALL_PREFIXES(GetUserMediaTest, PendingPromise);
  FRIEND_TEST_ALL_PREFIXES(GetUserMediaTest, MicrophoneStoppedRejectedPromise);
  FRIEND_TEST_ALL_PREFIXES(GetUserMediaTest, MicrophoneErrorRejectedPromise);
  FRIEND_TEST_ALL_PREFIXES(GetUserMediaTest, MicrophoneSuccessFulfilledPromise);
  FRIEND_TEST_ALL_PREFIXES(GetUserMediaTest,
                           MultipleMicrophoneSuccessFulfilledPromise);

  ~MediaDevices() override = default;

  // Stop callback used with MediaStreamAudioSource, and the OnMicrophoneError
  // logic will also call in to this.
  void OnMicrophoneStopped();

  // Callbacks used with MicrophoneManager.
  void OnMicrophoneError(speech::MicrophoneManager::MicrophoneError error,
                         std::string message);
  void OnMicrophoneSuccess();

  dom::DOMSettings* settings_;
  script::ScriptValueFactory* script_value_factory_;

  scoped_refptr<media_stream::MediaStreamAudioSource> audio_source_;

  scoped_refptr<media_stream::MediaStreamTrack> pending_microphone_track_;
  std::vector<std::unique_ptr<MediaStreamPromiseValue::Reference>>
      pending_microphone_promises_;

  base::MessageLoop* javascript_message_loop_;
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<MediaDevices> weak_ptr_factory_;
  base::WeakPtr<MediaDevices> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevices);
};

}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_MEDIA_DEVICES_H_
