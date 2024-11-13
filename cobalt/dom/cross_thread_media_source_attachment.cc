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

#include "cobalt/dom/cross_thread_media_source_attachment.h"

#include <utility>

#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/media_source_attachment.h"
#include "cobalt/dom/media_source_ready_state.h"
#include "cobalt/dom/time_ranges.h"
#include "cobalt/script/tracer.h"
#include "cobalt/web/url_registry.h"
#include "media/filters/chunk_demuxer.h"


namespace cobalt {
namespace dom {

CrossThreadMediaSourceAttachment::CrossThreadMediaSourceAttachment(
    scoped_refptr<MediaSource> media_source)
    : media_source_(media_source.get()),
      worker_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      recent_element_time_(0.0),
      element_has_error_(false),
      element_has_max_video_capabilities_(false),
      have_ever_attached_(false),
      have_ever_started_closing_(false) {}

void CrossThreadMediaSourceAttachment::TraceMembers(script::Tracer* tracer) {
  base::AutoLock auto_lock(attachment_state_lock_);
  if (worker_runner_->RunsTasksInCurrentSequence()) {
    tracer->Trace(media_source_);
  } else {
    tracer->Trace(attached_element_);
  }
}

bool CrossThreadMediaSourceAttachment::StartAttachingToMediaElement(
    HTMLMediaElement* media_element) {
  base::AutoLock auto_lock(attachment_state_lock_);
  // Called from main thread. At this point, we can only check if
  // this is not being called from the worker thread.
  DCHECK(!worker_runner_->RunsTasksInCurrentSequence());

  if (have_ever_attached_) {
    return false;
  }

  DCHECK(!have_ever_started_closing_);
  DCHECK(!attached_element_);

  // Attach on the main thread.
  if (!media_source_->StartAttachingToMediaElement(this)) {
    return false;
  }

  attached_element_ = media_element;
  main_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  DCHECK(main_runner_->RunsTasksInCurrentSequence());

  // Grab initial state from the HTMLMediaElement while on the main thread.
  recent_element_time_ = media_element->current_time(NULL);
  element_has_error_ = static_cast<bool>(media_element->error());
  element_has_max_video_capabilities_ =
      media_element->HasMaxVideoCapabilities();

  DCHECK(!element_has_error_);

  have_ever_attached_ = true;
  return true;
}

void CrossThreadMediaSourceAttachment::CompleteAttachingToMediaElement(
    ::media::ChunkDemuxer* chunk_demuxer) {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(have_ever_attached_);
  DCHECK(main_runner_->RunsTasksInCurrentSequence());
  DCHECK(!worker_runner_->RunsTasksInCurrentSequence());

  worker_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CrossThreadMediaSourceAttachment::
                         CompleteAttachingToMediaElementOnWorkerThread,
                     this, chunk_demuxer));
}

void CrossThreadMediaSourceAttachment::
    CompleteAttachingToMediaElementOnWorkerThread(
        ::media::ChunkDemuxer* chunk_demuxer) {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());
  DCHECK(!main_runner_->RunsTasksInCurrentSequence());

  if (have_ever_started_closing_) {
    return;
  }

  media_source_->CompleteAttachingToMediaElement(chunk_demuxer);
}

void CrossThreadMediaSourceAttachment::Close() {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(have_ever_attached_);
  DCHECK(!have_ever_started_closing_);
  DCHECK(main_runner_->RunsTasksInCurrentSequence());
  DCHECK(!worker_runner_->RunsTasksInCurrentSequence());

  have_ever_started_closing_ = true;

  worker_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CrossThreadMediaSourceAttachment::CloseOnWorkerThread,
                     this));
}

void CrossThreadMediaSourceAttachment::CloseOnWorkerThread() {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(have_ever_started_closing_);
  DCHECK(attached_element_);
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());
  DCHECK(!main_runner_->RunsTasksInCurrentSequence());

  media_source_->Close();
}

scoped_refptr<TimeRanges> CrossThreadMediaSourceAttachment::GetBufferedRange()
    const {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(attached_element_);
  DCHECK(!have_ever_started_closing_);
  DCHECK(have_ever_attached_);
  DCHECK(main_runner_->RunsTasksInCurrentSequence());
  DCHECK(!worker_runner_->RunsTasksInCurrentSequence());

  return media_source_->GetBufferedRange();
}

MediaSourceReadyState CrossThreadMediaSourceAttachment::GetReadyState() const {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(attached_element_);
  DCHECK(!have_ever_started_closing_);
  DCHECK(have_ever_attached_);
  DCHECK(main_runner_->RunsTasksInCurrentSequence());
  DCHECK(!worker_runner_->RunsTasksInCurrentSequence());

  return media_source_->ready_state();
}

void CrossThreadMediaSourceAttachment::NotifyDurationChanged(
    double /*duration*/) {
  attachment_state_lock_.AssertAcquired();
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());

  // No-op for cross thread MSA.
}

bool CrossThreadMediaSourceAttachment::HasMaxVideoCapabilities() const {
  attachment_state_lock_.AssertAcquired();
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());

  return element_has_max_video_capabilities_;
}

double CrossThreadMediaSourceAttachment::GetRecentMediaTime() {
  attachment_state_lock_.AssertAcquired();
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());

  return recent_element_time_;
}

bool CrossThreadMediaSourceAttachment::GetElementError() {
  attachment_state_lock_.AssertAcquired();
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());

  return element_has_error_;
}

scoped_refptr<AudioTrackList>
CrossThreadMediaSourceAttachment::CreateAudioTrackList(
    script::EnvironmentSettings* settings) {
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_refptr<VideoTrackList>
CrossThreadMediaSourceAttachment::CreateVideoTrackList(
    script::EnvironmentSettings* settings) {
  NOTIMPLEMENTED();
  return nullptr;
}

void CrossThreadMediaSourceAttachment::OnElementTimeUpdate(double time) {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(main_runner_->RunsTasksInCurrentSequence());
  DCHECK(!worker_runner_->RunsTasksInCurrentSequence());

  worker_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CrossThreadMediaSourceAttachment::UpdateWorkerThreadTimeCache, this,
          time));
}

void CrossThreadMediaSourceAttachment::UpdateWorkerThreadTimeCache(
    double time) {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());
  DCHECK(!main_runner_->RunsTasksInCurrentSequence());

  recent_element_time_ = time;
}

void CrossThreadMediaSourceAttachment::OnElementError() {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(main_runner_->RunsTasksInCurrentSequence());
  DCHECK(!worker_runner_->RunsTasksInCurrentSequence());

  worker_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CrossThreadMediaSourceAttachment::HandleElementErrorOnWorkerThread,
          this));
}

void CrossThreadMediaSourceAttachment::HandleElementErrorOnWorkerThread() {
  base::AutoLock auto_lock(attachment_state_lock_);
  DCHECK(!element_has_error_)
      << "At most one transition to element error per attachment is expected";
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());
  DCHECK(main_runner_->RunsTasksInCurrentSequence());

  element_has_error_ = true;
}

bool CrossThreadMediaSourceAttachment::FullyAttachedOrSameThread() const {
  attachment_state_lock_.AssertAcquired();
  DCHECK(have_ever_attached_);
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());
  DCHECK(!main_runner_->RunsTasksInCurrentSequence());

  return attached_element_ && !have_ever_started_closing_;
}

bool CrossThreadMediaSourceAttachment::RunExclusively(
    bool abort_if_not_fully_attached, RunExclusivelyCB cb) {
  base::AutoLock auto_lock(attachment_state_lock_);

  DCHECK(have_ever_attached_);
  DCHECK(worker_runner_->RunsTasksInCurrentSequence());

  if (abort_if_not_fully_attached &&
      (!attached_element_ || have_ever_started_closing_)) {
    return false;
  }

  std::move(cb).Run();
  return true;
}

void CrossThreadMediaSourceAttachment::
    AssertCrossThreadMutexIsAcquiredForDebugging() {
  attachment_state_lock_.AssertAcquired();
}

}  // namespace dom
}  // namespace cobalt
