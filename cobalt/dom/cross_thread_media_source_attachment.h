// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
//
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_DOM_CROSS_THREAD_MEDIA_SOURCE_ATTACHMENT_H_
#define COBALT_DOM_CROSS_THREAD_MEDIA_SOURCE_ATTACHMENT_H_

#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "cobalt/dom/audio_track_list.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/media_source_attachment_supplement.h"
#include "cobalt/dom/media_source_ready_state.h"
#include "cobalt/dom/time_ranges.h"
#include "cobalt/dom/video_track_list.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/tracer.h"
#include "cobalt/web/url_registry.h"
#include "media/filters/chunk_demuxer.h"

namespace cobalt {
namespace dom {

// MediaSourceAttachment that supports operations between a HTMLMediaElement on
// the main browser thread and a MediaSource on a DedicatedWorker.
class CrossThreadMediaSourceAttachment
    : public MediaSourceAttachmentSupplement {
 public:
  explicit CrossThreadMediaSourceAttachment(
      scoped_refptr<MediaSource> media_source);

  // Traceable
  void TraceMembers(script::Tracer* tracer) override;

  // MediaSourceAttachment
  bool StartAttachingToMediaElement(HTMLMediaElement* media_element) override
      LOCKS_EXCLUDED(attachment_state_lock_);
  void CompleteAttachingToMediaElement(::media::ChunkDemuxer* chunk_demuxer)
      override LOCKS_EXCLUDED(attachment_state_lock_);
  void Close() override LOCKS_EXCLUDED(attachment_state_lock_);
  scoped_refptr<TimeRanges> GetBufferedRange() const override
      LOCKS_EXCLUDED(attachment_state_lock_);
  MediaSourceReadyState GetReadyState() const override
      LOCKS_EXCLUDED(attachment_state_lock_);

  void OnElementTimeUpdate(double time) override
      LOCKS_EXCLUDED(attachment_state_lock_);
  void OnElementError() override LOCKS_EXCLUDED(attachment_state_lock_);

  // MediaSourceAttachmentSupplement
  void NotifyDurationChanged(double duration) override
      EXCLUSIVE_LOCKS_REQUIRED(attachment_state_lock_);
  bool HasMaxVideoCapabilities() const override
      EXCLUSIVE_LOCKS_REQUIRED(attachment_state_lock_);
  double GetRecentMediaTime() override
      EXCLUSIVE_LOCKS_REQUIRED(attachment_state_lock_);
  bool GetElementError() override
      EXCLUSIVE_LOCKS_REQUIRED(attachment_state_lock_);
  scoped_refptr<AudioTrackList> CreateAudioTrackList(
      script::EnvironmentSettings* settings) override;
  scoped_refptr<VideoTrackList> CreateVideoTrackList(
      script::EnvironmentSettings* settings) override;
  bool RunExclusively(bool abort_if_not_fully_attached,
                      RunExclusivelyCB cb) override
      LOCKS_EXCLUDED(attachment_state_lock_);
  void AssertCrossThreadMutexIsAcquiredForDebugging() override
      EXCLUSIVE_LOCKS_REQUIRED(attachment_state_lock_);
  bool FullyAttachedOrSameThread() const override
      EXCLUSIVE_LOCKS_REQUIRED(attachment_state_lock_);

  // TODO(338425449): Remove methods after feature rollout.
  scoped_refptr<MediaSource> media_source() const override {
    // Intentionally not implemented. Needed for the migration to
    // MediaSourceAttachment methods.
    NOTIMPLEMENTED();
    return nullptr;
  }
  base::WeakPtr<HTMLMediaElement> media_element() const override {
    // Intentionally not implemented. Needed for the migration to
    // MediaSourceAttachment methods.
    NOTIMPLEMENTED();
    return nullptr;
  }

 private:
  ~CrossThreadMediaSourceAttachment() = default;

  void CompleteAttachingToMediaElementOnWorkerThread(
      ::media::ChunkDemuxer* chunk_demuxer)
      LOCKS_EXCLUDED(attachment_state_lock_);
  void CloseOnWorkerThread() LOCKS_EXCLUDED(attachment_state_lock_);
  void UpdateWorkerThreadTimeCache(double time)
      LOCKS_EXCLUDED(attachment_state_lock_);
  void HandleElementErrorOnWorkerThread()
      LOCKS_EXCLUDED(attachment_state_lock_);

  mutable base::Lock attachment_state_lock_;

  scoped_refptr<base::SequencedTaskRunner> worker_runner_
      GUARDED_BY(attachment_state_lock_);
  scoped_refptr<base::SequencedTaskRunner> main_runner_
      GUARDED_BY(attachment_state_lock_);

  // Reference to the registered MediaSource.
  MediaSource* media_source_ GUARDED_BY(attachment_state_lock_);

  // Reference to the HTMLMediaElement the associated MediaSource is attached
  // to. Only set after StartAttachingToMediaElement is called.
  HTMLMediaElement* attached_element_ GUARDED_BY(attachment_state_lock_) =
      nullptr;

  double recent_element_time_
      GUARDED_BY(attachment_state_lock_);  // See OnElementTimeUpdate().
  bool element_has_error_
      GUARDED_BY(attachment_state_lock_);  // See OnElementError().
  bool element_has_max_video_capabilities_ GUARDED_BY(attachment_state_lock_);

  bool have_ever_attached_ GUARDED_BY(attachment_state_lock_);
  bool have_ever_started_closing_ GUARDED_BY(attachment_state_lock_);

  DISALLOW_COPY_AND_ASSIGN(CrossThreadMediaSourceAttachment);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_CROSS_THREAD_MEDIA_SOURCE_ATTACHMENT_H_
