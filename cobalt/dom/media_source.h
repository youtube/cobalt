// Copyright 2015 Google Inc. All Rights Reserved.
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

#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/dom/media_source/media_source.h"
#define COBALT_DOM_MEDIA_SOURCE_H_
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

#ifndef COBALT_DOM_MEDIA_SOURCE_H_
#define COBALT_DOM_MEDIA_SOURCE_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/media_source_end_of_stream_error.h"
#include "cobalt/dom/media_source_ready_state.h"
#include "cobalt/dom/source_buffer.h"
#include "cobalt/dom/source_buffer_list.h"
#include "cobalt/dom/url_registry.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "media/player/web_media_player.h"

namespace cobalt {
namespace dom {

// The MediaSource interface exposes functionalities for playing adaptive
// streams.
//   https://rawgit.com/w3c/media-source/6747ed7a3206f646963d760100b0f37e2fc7e47e/media-source.html#mediasource
//
// The class also serves as the point of interaction between SourceBuffer and
// the media player. For example, SourceBuffer forward its append() call to
// MediaSource and is forwarded to the player in turn.
// Note that our implementation is based on the MSE draft on 09 August 2012.
class MediaSource : public EventTarget {
 public:
  typedef UrlRegistry<MediaSource> Registry;

  // Custom, not in any spec.
  //
  MediaSource();

  // Web API: MediaSource
  //
  scoped_refptr<SourceBufferList> source_buffers() const;
  scoped_refptr<SourceBufferList> active_source_buffers() const;
  double duration(script::ExceptionState* exception_state) const;
  void set_duration(double duration, script::ExceptionState* exception_state);
  scoped_refptr<SourceBuffer> AddSourceBuffer(
      const std::string& type, script::ExceptionState* exception_state);
  void RemoveSourceBuffer(const scoped_refptr<SourceBuffer>& source_buffer,
                          script::ExceptionState* exception_state);

  MediaSourceReadyState ready_state() const;

  void EndOfStream(script::ExceptionState* exception_state);
  void EndOfStream(MediaSourceEndOfStreamError error,
                   script::ExceptionState* exception_state);

  static bool IsTypeSupported(script::EnvironmentSettings* settings,
                              const std::string& type);

  // Custom, not in any spec.
  //
  // The player is set when the media source is attached to a media element.
  // TODO: Invent a MediaSourceClient interface and make WebMediaPlayer inherit
  // from it.
  void SetPlayer(::media::WebMediaPlayer* player);
  void ScheduleEvent(base::Token event_name);

  // Methods used by SourceBuffer.
  scoped_refptr<TimeRanges> GetBuffered(
      const SourceBuffer* source_buffer,
      script::ExceptionState* exception_state);
  bool SetTimestampOffset(const SourceBuffer* source_buffer,
                          double timestamp_offset,
                          script::ExceptionState* exception_state);
  void Append(const SourceBuffer* source_buffer, const uint8* buffer, int size,
              script::ExceptionState* exception_state);
  void Abort(const SourceBuffer* source_buffer,
             script::ExceptionState* exception_state);

  // Methods used by HTMLMediaElement
  void SetReadyState(MediaSourceReadyState ready_state);

  void TraceMembers(script::Tracer* tracer) OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(MediaSource);

 private:
  // From EventTarget.
  std::string GetDebugName() OVERRIDE { return "MediaSource"; }

  MediaSourceReadyState ready_state_;
  ::media::WebMediaPlayer* player_;
  EventQueue event_queue_;
  scoped_refptr<SourceBufferList> source_buffers_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_H_
