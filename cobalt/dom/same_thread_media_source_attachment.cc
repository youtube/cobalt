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

#include "cobalt/dom/same_thread_media_source_attachment.h"

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

SameThreadMediaSourceAttachment::SameThreadMediaSourceAttachment(
    scoped_refptr<MediaSource> media_source)
    : media_source_(media_source),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      recent_element_time_(0.0),
      element_has_error_(false) {}

void SameThreadMediaSourceAttachment::TraceMembers(script::Tracer* tracer) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  tracer->Trace(attached_element_);
  tracer->Trace(media_source_);
}

bool SameThreadMediaSourceAttachment::StartAttachingToMediaElement(
    HTMLMediaElement* media_element) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!media_source_) {
    return false;
  }
  attached_element_ = base::AsWeakPtr(media_element);

  bool result = media_source_->StartAttachingToMediaElement(this);

  // Let the result of MediaSource::StartAttachingToMediaElement dictate if
  // the reference to the HTMLMediaElement should be retained.
  if (!result) {
    attached_element_.reset();
  }

  return result;
}

void SameThreadMediaSourceAttachment::CompleteAttachingToMediaElement(
    ::media::ChunkDemuxer* chunk_demuxer) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  media_source_->CompleteAttachingToMediaElement(chunk_demuxer);
}

void SameThreadMediaSourceAttachment::Close() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  media_source_->Close();
}

scoped_refptr<TimeRanges> SameThreadMediaSourceAttachment::GetBufferedRange()
    const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  return media_source_->GetBufferedRange();
}

MediaSourceReadyState SameThreadMediaSourceAttachment::GetReadyState() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  return media_source_->ready_state();
}

void SameThreadMediaSourceAttachment::NotifyDurationChanged(double duration) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(attached_element_);

  bool request_seek = attached_element_->current_time(NULL) > duration;
  attached_element_->DurationChanged(duration, request_seek);
}

bool SameThreadMediaSourceAttachment::HasMaxVideoCapabilities() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(attached_element_);

  return attached_element_->HasMaxVideoCapabilities();
}

double SameThreadMediaSourceAttachment::GetRecentMediaTime() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(attached_element_);

  double result = attached_element_->current_time(NULL);

  DVLOG(2) << __func__ << ": recent time=" << recent_element_time_
           << ", actual current time=" << result;

  return result;
}

bool SameThreadMediaSourceAttachment::GetElementError() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(attached_element_);

  bool result = static_cast<bool>(attached_element_->error());

  DCHECK_EQ(result, element_has_error_);

  return result;
}

scoped_refptr<AudioTrackList>
SameThreadMediaSourceAttachment::CreateAudioTrackList(
    script::EnvironmentSettings* settings) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(attached_element_);

  return base::MakeRefCounted<AudioTrackList>(settings, attached_element_);
}

scoped_refptr<VideoTrackList>
SameThreadMediaSourceAttachment::CreateVideoTrackList(
    script::EnvironmentSettings* settings) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(attached_element_);

  return base::MakeRefCounted<VideoTrackList>(settings, attached_element_);
}

void SameThreadMediaSourceAttachment::OnElementTimeUpdate(double time) {
  recent_element_time_ = time;
}

void SameThreadMediaSourceAttachment::OnElementError() {
  DCHECK(!element_has_error_)
      << "At most one transition to element error per attachment is expected";

  element_has_error_ = true;
}

}  // namespace dom
}  // namespace cobalt
