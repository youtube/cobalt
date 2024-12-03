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

#ifndef COBALT_DOM_MEDIA_SOURCE_H_
#define COBALT_DOM_MEDIA_SOURCE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cobalt/base/token.h"
#include "cobalt/dom/audio_track.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/media_source_attachment_supplement.h"
#include "cobalt/dom/media_source_end_of_stream_error.h"
#include "cobalt/dom/media_source_ready_state.h"
#include "cobalt/dom/serialized_algorithm_runner.h"
#include "cobalt/dom/source_buffer.h"
#include "cobalt/dom/source_buffer_algorithm.h"
#include "cobalt/dom/source_buffer_list.h"
#include "cobalt/dom/time_ranges.h"
#include "cobalt/dom/video_track.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/web/event_target.h"
#include "media/filters/chunk_demuxer.h"

namespace cobalt {
namespace dom {

// The MediaSource interface exposes functionalities for playing adaptive
// streams.
//   https://www.w3.org/TR/2016/CR-media-source-20160705/#idl-def-MediaSource
class MediaSource : public web::EventTarget {
 public:
  typedef ::media::ChunkDemuxer ChunkDemuxer;

  // Custom, not in any spec.
  //
  explicit MediaSource(script::EnvironmentSettings* settings);
  ~MediaSource();

  // Web API: MediaSource
  //
  scoped_refptr<SourceBufferList> source_buffers() const;
  scoped_refptr<SourceBufferList> active_source_buffers() const;
  MediaSourceReadyState ready_state() const;
  double duration(script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void set_duration(double duration, script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);
  scoped_refptr<SourceBuffer> AddSourceBuffer(
      script::EnvironmentSettings* settings, const std::string& type,
      script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void RemoveSourceBuffer(const scoped_refptr<SourceBuffer>& source_buffer,
                          script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);

  void EndOfStreamAlgorithm(MediaSourceEndOfStreamError error)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void EndOfStream(script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void EndOfStream(MediaSourceEndOfStreamError error,
                   script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void SetLiveSeekableRange(double start, double end,
                            script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void ClearLiveSeekableRange(script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);

  static bool IsTypeSupported(script::EnvironmentSettings* settings,
                              const std::string& type);

  static bool can_construct_in_dedicated_worker(
      script::EnvironmentSettings* settings);

  // Custom, not in any spec.
  //
  // HTMLMediaSource
  // TODO(b/338425449): Remove direct references to HTMLMediaElement.
  bool StartAttachingToMediaElement(HTMLMediaElement* media_element);
  bool StartAttachingToMediaElement(
      scoped_refptr<MediaSourceAttachmentSupplement> media_source_attachment)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void CompleteAttachingToMediaElement(ChunkDemuxer* chunk_demuxer)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void Close();
  bool IsClosed() const;
  scoped_refptr<TimeRanges> GetBufferedRange() const
      LOCKS_EXCLUDED(attachment_link_lock_);
  scoped_refptr<TimeRanges> GetSeekable() const
      LOCKS_EXCLUDED(attachment_link_lock_);
  void OnAudioTrackChanged(AudioTrack* audio_track);
  void OnVideoTrackChanged(VideoTrack* video_track);

  // Used by SourceBuffer.
  void OpenIfInEndedState() LOCKS_EXCLUDED(attachment_link_lock_);
  bool IsOpen() const;
  void SetSourceBufferActive(SourceBuffer* source_buffer, bool is_active)
      LOCKS_EXCLUDED(attachment_link_lock_);
  // TODO(b/338425449): Remove direct references to HTMLMediaElement.
  HTMLMediaElement* GetMediaElement() const;
  MediaSourceAttachmentSupplement* GetMediaSourceAttachment() const
      LOCKS_EXCLUDED(attachment_link_lock_);
  bool MediaElementHasMaxVideoCapabilities()
      LOCKS_EXCLUDED(attachment_link_lock_);

  // "_Locked" methods requires these to be called while in the scope of
  // a MediaSourceAttachmentSupplement::RunExclusively bound function.
  double GetDuration_Locked() const LOCKS_EXCLUDED(attachment_link_lock_);
  SerializedAlgorithmRunner<SourceBufferAlgorithm>* GetAlgorithmRunner(
      int job_size);

  DEFINE_WRAPPABLE_TYPE(MediaSource);
  void TraceMembers(script::Tracer* tracer) override;

  bool RunUnlessElementGoneOrClosingUs(
      MediaSourceAttachmentSupplement::RunExclusivelyCB cb)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void AssertAttachmentsMutexHeldIfCrossThreadForDebugging() const
      LOCKS_EXCLUDED(attachment_link_lock_);

 private:
  void SetReadyState(MediaSourceReadyState ready_state);
  void SetReadyState(MediaSourceReadyState ready_state, bool in_dtor)
      LOCKS_EXCLUDED(attachment_link_lock_);
  bool IsUpdating() const;
  void ScheduleEvent(base_token::Token event_name);
  void DurationChangeAlgorithm(double new_duration,
                               script::ExceptionState* exception_state)
      LOCKS_EXCLUDED(attachment_link_lock_);

  // "_Locked" methods requires these to be called while in the scope of
  // a MediaSourceAttachmentSupplement::RunExclusively bound function.
  void AddSourceBuffer_Locked(script::EnvironmentSettings* settings,
                              const std::string& type,
                              script::ExceptionState* exception_state,
                              SourceBuffer** created_buffer)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void RemoveSourceBuffer_Locked(
      const scoped_refptr<SourceBuffer>& source_buffer)
      LOCKS_EXCLUDED(attachment_link_lock_);
  void GetDurationInternal_Locked(double* result)
      LOCKS_EXCLUDED(attachment_link_lock_);

  // Set to true to offload SourceBuffer buffer append and removal algorithms to
  // a non-web thread.
  const bool algorithm_offload_enabled_;
  // Set to true to reduce asynchronous behaviors.  For example, queued events
  // will be dispatached immediately when possible.
  const bool asynchronous_reduction_enabled_;
  // Only used when |asynchronous_reduction_enabled_| is set to true, where any
  // buffer append job smaller than its value will happen immediately instead of
  // being scheduled asynchronously.
  const int max_size_for_immediate_job_;

  // The default algorithm runner runs all steps on the web thread.
  DefaultAlgorithmRunner<SourceBufferAlgorithm> default_algorithm_runner_;
  // The immediate job algorithm runner runs job immediately on the web thread,
  // it has asynchronous reduction always enabled to run immediate jobs even
  // when asynchronous reduction is disabled on the `default_algorithm_runner_`.
  DefaultAlgorithmRunner<SourceBufferAlgorithm> immediate_job_algorithm_runner_{
      true};
  // The offload algorithm runner offloads some steps to a non-web thread.
  std::unique_ptr<OffloadAlgorithmRunner<SourceBufferAlgorithm>>
      offload_algorithm_runner_;
  std::unique_ptr<base::Thread> algorithm_process_thread_;

  ChunkDemuxer* chunk_demuxer_;
  MediaSourceReadyState ready_state_;
  EventQueue event_queue_;
  // TODO(b/338425449): Remove direct references to HTMLMediaElement.
  bool is_using_media_source_attachment_methods_ = false;
  bool is_mse_in_workers_enabled_ = false;
  base::WeakPtr<HTMLMediaElement> attached_element_;

  scoped_refptr<MediaSourceAttachmentSupplement> media_source_attachment_
      GUARDED_BY(attachment_link_lock_);

  scoped_refptr<SourceBufferList> source_buffers_;
  scoped_refptr<SourceBufferList> active_source_buffers_;

  scoped_refptr<TimeRanges> live_seekable_range_;
  // Used when MediaSourceAttachment methods and MSE-in-Workers is enabled
  // to avoid deadlocks while tracing.
  bool has_live_seekable_range_ GUARDED_BY(attachment_link_lock_);
  double live_seekable_range_start_ GUARDED_BY(attachment_link_lock_);
  double live_seekable_range_end_ GUARDED_BY(attachment_link_lock_);

  bool has_max_video_capabilities_ = false;

  mutable base::Lock attachment_link_lock_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_H_
