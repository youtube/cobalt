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
#include "base/message_loop_proxy.h"
#include "base/optional.h"
#include "base/string_piece.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "cobalt/dom/blob_property_bag.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media_capture/media_recorder_options.h"
#include "cobalt/media_capture/recording_state.h"
#include "cobalt/media_stream/audio_parameters.h"
#include "cobalt/media_stream/media_stream.h"
#include "cobalt/media_stream/media_stream_audio_sink.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace media_capture {

// This class represents a MediaRecorder, and implements the specification at:
// https://www.w3.org/TR/mediastream-recording/#mediarecorder-api
class MediaRecorder : public media_stream::MediaStreamAudioSink,
                      public dom::EventTarget {
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

  // MediaStreamAudioSink overrides.
  void OnData(const ShellAudioBus& audio_bus,
              base::TimeTicks reference_time) override;
  void OnSetFormat(const media_stream::AudioParameters& params) override;

  DEFINE_WRAPPABLE_TYPE(MediaRecorder);

  void TraceMembers(script::Tracer* tracer) override {
    EventTarget::TraceMembers(tracer);
    tracer->Trace(stream_);
  }

 private:
  MediaRecorder(const MediaRecorder&) = delete;
  MediaRecorder& operator=(const MediaRecorder&) = delete;

  void StopRecording();
  void DoOnDataCallback(scoped_ptr<std::vector<uint8>> data,
                        base::TimeTicks timecode);
  void WriteData(const char* data, size_t length, bool last_in_slice,
                 base::TimeTicks timecode);

  base::ThreadChecker thread_checker_;
  std::vector<uint8> buffer_;

  RecordingState recording_state_ = kRecordingStateInactive;

  script::EnvironmentSettings* settings_;
  std::string mime_type_;
  dom::BlobPropertyBag blob_options_;
  scoped_refptr<media_stream::MediaStream> stream_;

  scoped_refptr<base::MessageLoopProxy> javascript_message_loop_;
  base::TimeDelta timeslice_;
  // Only used to determine if |timeslice_| amount of time has
  // been passed since the slice was started.
  base::TimeTicks slice_origin_timestamp_;

  int64 bits_per_second_ = 0;
};

}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_MEDIA_RECORDER_H_
