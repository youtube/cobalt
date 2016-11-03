/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DOM_MEDIA_SOURCE_H_
#define COBALT_DOM_MEDIA_SOURCE_H_

#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/source_buffer.h"
#include "cobalt/dom/source_buffer_list.h"
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
  // This class manages a registry of MediaSource objects.  Its user can
  // associate a MediaSource object to a blob url as well as to retrieve a
  // MediaSource object by a blob url.  This is because to assign a MediaSource
  // object to the src of an HTMLMediaElement, we have to first convert the
  // MediaSource object into a blob url by calling URL.createObjectURL().
  // And eventually the HTMLMediaElement have to retrieve the MediaSource object
  // from the blob url.
  // Note: It is unsafe to directly encode the pointer to the MediaSource object
  // in the url as the url is assigned from JavaScript.
  class Registry {
   public:
    void Register(const std::string& blob_url,
                  const scoped_refptr<MediaSource>& media_source);
    scoped_refptr<MediaSource> Retrieve(const std::string& blob_url);
    void Unregister(const std::string& blob_url);

   private:
    typedef base::hash_map<std::string, scoped_refptr<MediaSource> >
        MediaSourceRegistry;
    MediaSourceRegistry media_source_registry_;
  };

  // Custom, not in any spec.
  //
  // MediaSource spec defines states as strings "closed", "open" and "ended".
  enum ReadyState { kReadyStateClosed, kReadyStateOpen, kReadyStateEnded };

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

  std::string ready_state() const;

  void EndOfStream(script::ExceptionState* exception_state);
  void EndOfStream(const std::string& error,
                   script::ExceptionState* exception_state);

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
  ReadyState GetReadyState() const;
  void SetReadyState(ReadyState ready_state);

  DEFINE_WRAPPABLE_TYPE(MediaSource);

 private:
  // From EventTarget.
  std::string GetDebugName() OVERRIDE { return "MediaSource"; }

  ReadyState ready_state_;
  ::media::WebMediaPlayer* player_;
  EventQueue event_queue_;
  scoped_refptr<SourceBufferList> source_buffers_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_H_
