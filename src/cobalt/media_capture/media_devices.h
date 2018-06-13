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

#ifndef COBALT_MEDIA_CAPTURE_MEDIA_DEVICES_H_
#define COBALT_MEDIA_CAPTURE_MEDIA_DEVICES_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media_capture/media_device_info.h"
#include "cobalt/media_stream/media_stream_constraints.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/speech/microphone.h"

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

  explicit MediaDevices(script::ScriptValueFactory* script_value_factory);

  script::Handle<MediaInfoSequencePromise> EnumerateDevices();
  script::Handle<MediaStreamPromise> GetUserMedia();
  script::Handle<MediaStreamPromise> GetUserMedia(
      const media_stream::MediaStreamConstraints& constraints);

  void SetEnvironmentSettings(script::EnvironmentSettings* settings) {
    settings_ = base::polymorphic_downcast<dom::DOMSettings*>(settings);
  }

  DEFINE_WRAPPABLE_TYPE(MediaDevices);

 private:
  void CreateMicrophoneIfNeeded();

  script::ScriptValueFactory* script_value_factory_;
  dom::DOMSettings* settings_ = nullptr;

  scoped_ptr<speech::Microphone> microphone_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevices);
};

}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_MEDIA_DEVICES_H_
