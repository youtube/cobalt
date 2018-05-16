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
#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/optional.h"
#include "base/string_piece.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media_capture/media_recorder_options.h"
#include "cobalt/media_capture/recording_state.h"
#include "cobalt/media_stream/media_stream.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace media_capture {

// This class represents a MediaRecorder, and implements the specification at:
// https://www.w3.org/TR/mediastream-recording/#mediarecorder-api
class MediaRecorder : public dom::EventTarget {
 public:
  // Constructors.
  explicit MediaRecorder(
      script::EnvironmentSettings* settings,
      const scoped_refptr<media_stream::MediaStream>& stream,
      const MediaRecorderOptions& options = MediaRecorderOptions());

  // Readonly attributes.
  const std::string& mime_type() const { return mime_type_; }

  // Functions
  static bool IsTypeSupported(const base::StringPiece mime_type);

  void Start(int32 timeslice, script::ExceptionState* exception_state);

  void Start(script::ExceptionState* exception_state) {
    Start(kint32max, exception_state);
  }

  void Stop(script::ExceptionState* exception_state);

  // EventHandlers.
  const EventListenerScriptValue* onerror() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return GetAttributeEventListener(base::Tokens::error());
  }

  void set_onerror(const EventListenerScriptValue& event_listener) {
    DCHECK(thread_checker_.CalledOnValidThread());
    SetAttributeEventListener(base::Tokens::error(), event_listener);
  }

  const EventListenerScriptValue* ondataavailable() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return GetAttributeEventListener(base::Tokens::dataavailable());
  }

  void set_ondataavailable(const EventListenerScriptValue& event_listener) {
    DCHECK(thread_checker_.CalledOnValidThread());
    SetAttributeEventListener(base::Tokens::dataavailable(), event_listener);
  }

  DEFINE_WRAPPABLE_TYPE(MediaRecorder);

 private:
  MediaRecorder(const MediaRecorder&) = delete;
  MediaRecorder& operator=(const MediaRecorder&) = delete;

  class Buffer {
   public:
    base::StringPiece GetWriteCursor(size_t number_of_bytes);
    void IncrementCursorPosition(size_t number_of_bytes);
    base::StringPiece GetWrittenChunk() const;
    void Reset();
    void HintTypicalSize(size_t number_of_bytes);

   private:
    size_t current_position_ = 0;
    std::vector<uint8> buffer_;
  };

  void ReadStreamAndDoCallback();
  void DoOnDataCallback();
  void ScheduleOnDataAvailableCallback();
  void ResetLastCallbackTime();
  void CalculateStreamBitrate();
  int64 GetRecommendedBufferSize(base::TimeDelta time_span) const;
  base::TimeTicks GetNextCallbackTime() const {
    return last_callback_time_ + callback_frequency_;
  }

  base::ThreadChecker thread_checker_;

  RecordingState recording_state_ = kRecordingStateInactive;

  script::EnvironmentSettings* settings_;
  std::string mime_type_;
  scoped_refptr<media_stream::MediaStream> stream_;

  base::TimeTicks last_callback_time_;
  // Frequency we will callback to Javascript.
  base::TimeDelta callback_frequency_;

  // Frequency we will read the stream.
  base::TimeDelta read_frequency_;

  int64 bitrate_bps_;
  Buffer recorded_buffer_;

  base::CancelableClosure stream_reader_callback_;
};

}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_MEDIA_RECORDER_H_
