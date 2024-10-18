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
    : media_source_(media_source),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      recent_element_time_(0.0),
      element_has_error_(false) {}

void CrossThreadMediaSourceAttachment::TraceMembers(script::Tracer* tracer) {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());

  tracer->Trace(attached_element_);
  tracer->Trace(media_source_);
}

bool CrossThreadMediaSourceAttachment::StartAttachingToMediaElement(
    HTMLMediaElement* media_element) {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
  return false;
}

void CrossThreadMediaSourceAttachment::CompleteAttachingToMediaElement(
    ::media::ChunkDemuxer* chunk_demuxer) {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
}

void CrossThreadMediaSourceAttachment::Close() {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
}

scoped_refptr<TimeRanges> CrossThreadMediaSourceAttachment::GetBufferedRange()
    const {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
  return nullptr;
}

MediaSourceReadyState CrossThreadMediaSourceAttachment::GetReadyState() const {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
  return kMediaSourceReadyStateClosed;
}

void CrossThreadMediaSourceAttachment::NotifyDurationChanged(double duration) {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
}

bool CrossThreadMediaSourceAttachment::HasMaxVideoCapabilities() const {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
  return false;
}

double CrossThreadMediaSourceAttachment::GetRecentMediaTime() {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
  return 0;
}

bool CrossThreadMediaSourceAttachment::GetElementError() {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
  return false;
}

scoped_refptr<AudioTrackList>
CrossThreadMediaSourceAttachment::CreateAudioTrackList(
    script::EnvironmentSettings* settings) {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
  return nullptr;
}

scoped_refptr<VideoTrackList>
CrossThreadMediaSourceAttachment::CreateVideoTrackList(
    script::EnvironmentSettings* settings) {
  DCHECK_NE(task_runner_, base::SequencedTaskRunner::GetCurrentDefault());
  NOTIMPLEMENTED();
  return nullptr;
}

void CrossThreadMediaSourceAttachment::OnElementTimeUpdate(double time) {
  NOTIMPLEMENTED();
  recent_element_time_ = time;
}

void CrossThreadMediaSourceAttachment::OnElementError() {
  DCHECK(!element_has_error_)
      << "At most one transition to element error per attachment is expected";
  NOTIMPLEMENTED();
  element_has_error_ = true;
}

}  // namespace dom
}  // namespace cobalt
