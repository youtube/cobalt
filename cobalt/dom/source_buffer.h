/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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

// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_SOURCE_BUFFER_H_
#define COBALT_DOM_SOURCE_BUFFER_H_

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "cobalt/base/token.h"
#include "cobalt/dom/audio_track_list.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/serialized_algorithm_runner.h"
#include "cobalt/dom/source_buffer_algorithm.h"
#include "cobalt/dom/source_buffer_append_mode.h"
#include "cobalt/dom/source_buffer_metrics.h"
#include "cobalt/dom/time_ranges.h"
#include "cobalt/dom/track_default_list.h"
#include "cobalt/dom/video_track_list.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/web/event_target.h"
#include "media/base/media_tracks.h"
#include "media/filters/chunk_demuxer.h"
#include "starboard/common/mutex.h"

namespace cobalt {
namespace dom {

class MediaSource;

// The SourceBuffer interface exposes the media source buffer so its user can
// append media data to.
//   https://www.w3.org/TR/2016/CR-media-source-20160705/#sourcebuffer
class SourceBuffer : public web::EventTarget {
 public:
  typedef ::media::ChunkDemuxer ChunkDemuxer;

  // Custom, not in any spec.
  //
  SourceBuffer(script::EnvironmentSettings* settings, const std::string& id,
               MediaSource* media_source, ChunkDemuxer* chunk_demuxer,
               EventQueue* event_queue);

  // Web API: SourceBuffer
  //
  SourceBufferAppendMode mode(script::ExceptionState* exception_state) const {
    return mode_;
  }
  void set_mode(SourceBufferAppendMode mode,
                script::ExceptionState* exception_state);
  bool updating() const { return active_algorithm_handle_ != nullptr; }
  scoped_refptr<TimeRanges> buffered(
      script::ExceptionState* exception_state) const;
  double timestamp_offset(script::ExceptionState* exception_state) const;
  void set_timestamp_offset(double offset,
                            script::ExceptionState* exception_state);
  scoped_refptr<AudioTrackList> audio_tracks() const { return audio_tracks_; }
  scoped_refptr<VideoTrackList> video_tracks() const { return video_tracks_; }
  double append_window_start(script::ExceptionState* exception_state) const {
    return append_window_start_;
  }
  void set_append_window_start(double start,
                               script::ExceptionState* exception_state);
  double append_window_end(script::ExceptionState* exception_state) const {
    return append_window_end_;
  }
  void set_append_window_end(double start,
                             script::ExceptionState* exception_state);
  void AppendBuffer(const script::Handle<script::ArrayBuffer>& data,
                    script::ExceptionState* exception_state);
  void AppendBuffer(const script::Handle<script::ArrayBufferView>& data,
                    script::ExceptionState* exception_state);
  void Abort(script::ExceptionState* exception_state);
  void Remove(double start, double end,
              script::ExceptionState* exception_state);
  scoped_refptr<TrackDefaultList> track_defaults(
      script::ExceptionState* exception_state) const {
    return track_defaults_;
  }
  void set_track_defaults(const scoped_refptr<TrackDefaultList>& track_defaults,
                          script::ExceptionState* exception_state);

  // Custom, not in any spec.
  //
  // Return the highest presentation timestamp written to SbPlayer.
  double write_head(script::ExceptionState* exception_state) const;

  void OnRemovedFromMediaSource();
  double GetHighestPresentationTimestamp() const;

  DEFINE_WRAPPABLE_TYPE(SourceBuffer);
  void TraceMembers(script::Tracer* tracer) override;

  size_t memory_limit(script::ExceptionState* exception_state) const;
  void set_memory_limit(size_t limit, script::ExceptionState* exception_state);

 private:
  typedef ::media::MediaTracks MediaTracks;
  typedef script::ArrayBuffer ArrayBuffer;
  typedef script::ArrayBufferView ArrayBufferView;

  // SourceBuffer is inherited from base::RefCounted<> and its ref count cannot
  // be used on non-web threads.  On the other hand the call to
  // OnInitSegmentReceived() may happen on a worker thread for algorithm
  // offloading.  This object allows to manage the life time of the SourceBuffer
  // object across threads while still maintain it as a sub-class of
  // base::RefCounted<>.
  class OnInitSegmentReceivedHelper
      : public base::RefCountedThreadSafe<OnInitSegmentReceivedHelper> {
   public:
    explicit OnInitSegmentReceivedHelper(SourceBuffer* source_buffer);
    void Detach();
    void TryToRunOnInitSegmentReceived(std::unique_ptr<MediaTracks> tracks);

   private:
    scoped_refptr<base::SequencedTaskRunner> task_runner_ =
        base::SequencedTaskRunner::GetCurrentDefault();
    // The access to |source_buffer_| always happens on |task_runner_|, and
    // needn't be explicitly synchronized by a mutex.
    SourceBuffer* source_buffer_;
  };


  void OnInitSegmentReceived(std::unique_ptr<MediaTracks> tracks);
  void ScheduleEvent(base_token::Token event_name);
  bool PrepareAppend(size_t new_data_size,
                     script::ExceptionState* exception_state);
  void AppendBufferInternal(const unsigned char* data, size_t size,
                            script::ExceptionState* exception_state);

  void OnAlgorithmFinalized();
  void UpdateTimestampOffset(base::TimeDelta timestamp_offset_);

  const TrackDefault* GetTrackDefault(
      const std::string& track_type,
      const std::string& byte_stream_track_id) const;
  std::string DefaultTrackLabel(const std::string& track_type,
                                const std::string& byte_stream_track_id) const;
  std::string DefaultTrackLanguage(
      const std::string& track_type,
      const std::string& byte_stream_track_id) const;

  static const size_t kDefaultMaxAppendBufferSize = 128 * 1024;

  scoped_refptr<OnInitSegmentReceivedHelper> on_init_segment_received_helper_;
  const std::string id_;
  const size_t evict_extra_in_bytes_;
  const size_t max_append_buffer_size_;

  MediaSource* media_source_;
  ChunkDemuxer* chunk_demuxer_;
  scoped_refptr<TrackDefaultList> track_defaults_ = new TrackDefaultList(NULL);
  EventQueue* event_queue_;

  bool first_initialization_segment_received_ = false;
  scoped_refptr<AudioTrackList> audio_tracks_;
  scoped_refptr<VideoTrackList> video_tracks_;

  SourceBufferAppendMode mode_ = kSourceBufferAppendModeSegments;

  starboard::Mutex timestamp_offset_mutex_;
  double timestamp_offset_ = 0;

  scoped_refptr<SerializedAlgorithmRunner<SourceBufferAlgorithm>::Handle>
      active_algorithm_handle_;
  double append_window_start_ = 0;
  double append_window_end_ = std::numeric_limits<double>::infinity();

  bool is_avoid_copying_array_buffer_enabled_ = false;
  script::Handle<ArrayBuffer> array_buffer_in_use_;
  script::Handle<ArrayBufferView> array_buffer_view_in_use_;
  std::unique_ptr<uint8_t[]> pending_append_data_;
  size_t pending_append_data_capacity_ = 0;

  SourceBufferMetrics metrics_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SOURCE_BUFFER_H_
