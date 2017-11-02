/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Modifications Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_MEDIA_SOURCE_MEDIA_SOURCE_H_
#define COBALT_DOM_MEDIA_SOURCE_MEDIA_SOURCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/base/token.h"
#include "cobalt/dom/audio_track.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/media_source/source_buffer.h"
#include "cobalt/dom/media_source/source_buffer_list.h"
#include "cobalt/dom/media_source_end_of_stream_error.h"
#include "cobalt/dom/media_source_ready_state.h"
#include "cobalt/dom/time_ranges.h"
#include "cobalt/dom/url_registry.h"
#include "cobalt/dom/video_track.h"
#include "cobalt/media/filters/chunk_demuxer.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"

namespace cobalt {
namespace dom {

// The MediaSource interface exposes functionalities for playing adaptive
// streams.
//   https://www.w3.org/TR/2016/CR-media-source-20160705/#idl-def-MediaSource
class MediaSource : public EventTarget {
 public:
  typedef UrlRegistry<MediaSource> Registry;

  // Custom, not in any spec.
  //
  MediaSource();
  ~MediaSource();

  // Web API: MediaSource
  //
  scoped_refptr<SourceBufferList> source_buffers() const;
  scoped_refptr<SourceBufferList> active_source_buffers() const;
  MediaSourceReadyState ready_state() const;
  double duration(script::ExceptionState* exception_state) const;
  void set_duration(double duration, script::ExceptionState* exception_state);
  scoped_refptr<SourceBuffer> AddSourceBuffer(
      const std::string& type, script::ExceptionState* exception_state);
  void RemoveSourceBuffer(const scoped_refptr<SourceBuffer>& source_buffer,
                          script::ExceptionState* exception_state);

  void EndOfStreamAlgorithm(MediaSourceEndOfStreamError error);
  void EndOfStream(script::ExceptionState* exception_state);
  void EndOfStream(MediaSourceEndOfStreamError error,
                   script::ExceptionState* exception_state);
  void SetLiveSeekableRange(double start, double end,
                            script::ExceptionState* exception_state);
  void ClearLiveSeekableRange(script::ExceptionState* exception_state);

  static bool IsTypeSupported(script::EnvironmentSettings* settings,
                              const std::string& type);

  // Custom, not in any spec.
  //
  // HTMLMediaSource
  bool AttachToElement(HTMLMediaElement* media_element);
  void SetChunkDemuxerAndOpen(media::ChunkDemuxer* chunk_demuxer);
  void Close();
  bool IsClosed() const;
  scoped_refptr<TimeRanges> GetBufferedRange() const;
  scoped_refptr<TimeRanges> GetSeekable() const;
  void OnAudioTrackChanged(AudioTrack* audio_track);
  void OnVideoTrackChanged(VideoTrack* video_track);

  // Used by SourceBuffer.
  void OpenIfInEndedState();
  bool IsOpen() const;
  void SetSourceBufferActive(SourceBuffer* source_buffer, bool is_active);
  HTMLMediaElement* GetMediaElement() const;

  DEFINE_WRAPPABLE_TYPE(MediaSource);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  void SetReadyState(MediaSourceReadyState ready_state);
  bool IsUpdating() const;
  void ScheduleEvent(base::Token eventName);

  media::ChunkDemuxer* chunk_demuxer_;
  MediaSourceReadyState ready_state_;
  EventQueue event_queue_;
  base::WeakPtr<HTMLMediaElement> attached_element_;

  scoped_refptr<SourceBufferList> source_buffers_;
  scoped_refptr<SourceBufferList> active_source_buffers_;

  scoped_refptr<TimeRanges> live_seekable_range_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_MEDIA_SOURCE_H_
