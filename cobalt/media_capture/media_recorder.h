// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_CAPTURE_MEDIA_RECORDER_H_
#define COBALT_MEDIA_CAPTURE_MEDIA_RECORDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/optional.h"
#include "base/string_piece.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media_capture/media_recorder_options.h"
#include "cobalt/media_stream/media_stream.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace media_capture {

// This class represents a MediaRecorder, and implements the specification at:
// https://www.w3.org/TR/mediastream-recording/#mediarecorder-api
class MediaRecorder : public dom::EventTarget {
 public:
  // Constructors.
  explicit MediaRecorder(
      const scoped_refptr<media_stream::MediaStream>& stream) {
    UNREFERENCED_PARAMETER(stream);
    NOTIMPLEMENTED();
  }

  MediaRecorder(const scoped_refptr<media_stream::MediaStream>& stream,
                const MediaRecorderOptions& options) {
    UNREFERENCED_PARAMETER(stream);
    UNREFERENCED_PARAMETER(options);
    NOTIMPLEMENTED();
  }

  // Readonly attributes.
  const std::string& mime_type() const { return mime_type_; }

  // Functions
  static bool IsTypeSupported(const base::StringPiece mime_type);

  void Start(int32 timeslice);

  void Start() { Start(kint32max); }

  void Stop();

  // EventHandlers.
  const EventListenerScriptValue* onerror() const {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void set_onerror(const EventListenerScriptValue& event_listener) {
    UNREFERENCED_PARAMETER(event_listener);
    NOTIMPLEMENTED();
  }

  const EventListenerScriptValue* ondataavailable() const {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void set_ondataavailable(const EventListenerScriptValue& event_listener) {
    UNREFERENCED_PARAMETER(event_listener);
    NOTIMPLEMENTED();
  }

  DEFINE_WRAPPABLE_TYPE(MediaRecorder);

 private:
  MediaRecorder(const MediaRecorder&) = delete;
  MediaRecorder& operator=(const MediaRecorder&) = delete;

  std::string mime_type_;
};

}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_MEDIA_RECORDER_H_
