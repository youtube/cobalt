// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

// MediaSource and SourceBuffer methods for multi-SourceBuffer switching:
// standby SourceBuffers (addSourceBuffer(type, {standby: true})) and
// MediaSource.changeTo(). The H5VCC setting
// `Media.EnableMultiSourceBufferSwitching` exposes them.

// clang-format off
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
// clang-format on

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "base/check.h"
#include "third_party/blink/public/platform/web_source_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_source_buffer_init.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/html/track/audio_track.h"
#include "third_party/blink/renderer/core/html/track/audio_track_list.h"
#include "third_party/blink/renderer/core/html/track/video_track.h"
#include "third_party/blink/renderer/core/html/track/video_track_list.h"
#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_tracer.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer.h"
#include "third_party/blink/renderer/modules/mediasource/source_buffer_list.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

SourceBuffer* MediaSource::AddSourceBufferWithInit(
    const String& type,
    const SourceBufferInit* init,
    ExceptionState& exception_state) {
  SourceBuffer* source_buffer = addSourceBuffer(type, exception_state);
  if (!source_buffer || !init->standby()) {
    return source_buffer;
  }

  bool succeeded = false;
  if (!RunUnlessElementGoneOrClosingUs(WTF::BindOnce(
          &MediaSource::SetStandby_Locked, WrapPersistent(this),
          WrapPersistent(source_buffer), WTF::Unretained(&succeeded))) ||
      !succeeded) {
    // Don't leave behind a SourceBuffer that would take part in playback.
    removeSourceBuffer(source_buffer, exception_state);
    if (!exception_state.HadException()) {
      LogAndThrowDOMException(exception_state,
                              DOMExceptionCode::kNotSupportedError,
                              "Standby SourceBuffers aren't supported.");
    }
    return nullptr;
  }
  return source_buffer;
}

ScriptPromise<IDLUndefined> MediaSource::changeTo(
    ScriptState* script_state,
    const HeapVector<Member<SourceBuffer>>& source_buffers,
    ExceptionState& exception_state) {
  // Multi-SourceBuffer switching is only for MediaSources on the main thread.
  DCHECK(IsMainThread());
  if (!IsOpen()) {
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "The MediaSource's readyState is not 'open'.");
    return EmptyPromise();
  }
  if (source_buffers.empty()) {
    LogAndThrowTypeError(exception_state, "No SourceBuffers to change to.");
    return EmptyPromise();
  }
  for (const Member<SourceBuffer>& source_buffer : source_buffers) {
    if (!source_buffers_->Contains(source_buffer)) {
      LogAndThrowDOMException(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "A SourceBuffer isn't in this MediaSource's sourceBuffers.");
      return EmptyPromise();
    }
  }

  // Switch the streams that playback reads.
  std::vector<WebSourceBuffer*> web_source_buffers;
  web_source_buffers.reserve(source_buffers.size());
  for (const Member<SourceBuffer>& source_buffer : source_buffers) {
    web_source_buffers.push_back(
        source_buffer->GetWebSourceBufferForChangeTo());
  }
  bool changed = false;
  if (!RunUnlessElementGoneOrClosingUs(WTF::BindOnce(
          &MediaSource::ChangeTo_Locked, WrapPersistent(this),
          WTF::Unretained(&web_source_buffers), WTF::Unretained(&changed)))) {
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "The MediaSource attachment is closing.");
    return EmptyPromise();
  }
  if (!changed) {
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kNotSupportedError,
                            "Can't change to these SourceBuffers.");
    return EmptyPromise();
  }

  UpdateTracksAfterChangeTo(source_buffers);

  // The timeline starts over where the media of |source_buffers| starts. The
  // seek completes the switch.
  double start = std::numeric_limits<double>::infinity();
  for (const Member<SourceBuffer>& source_buffer : source_buffers) {
    TimeRanges* ranges = source_buffer->buffered(ASSERT_NO_EXCEPTION);
    if (ranges && ranges->length() > 0) {
      start = std::min(start, ranges->start(0, ASSERT_NO_EXCEPTION));
    }
  }
  if (!std::isfinite(start)) {
    start = 0;
  }
  auto [attachment, tracer] = AttachmentAndTracer();
  // Only a same-thread attachment has a tracer.
  if (tracer) {
    if (HTMLMediaElement* element =
            static_cast<SameThreadMediaSourceTracer*>(tracer)
                ->GetMediaElement()) {
      element->setCurrentTime(start);
    }
  }
  return ToResolvedUndefinedPromise(script_state);
}

void MediaSource::SetStandby_Locked(
    SourceBuffer* source_buffer,
    bool* succeeded,
    MediaSourceAttachmentSupplement::ExclusiveKey pass_key) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();
  *succeeded = source_buffer->SetStandby_Locked(pass_key);
}

void MediaSource::ChangeTo_Locked(
    const std::vector<WebSourceBuffer*>* web_source_buffers,
    bool* changed,
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  AssertAttachmentsMutexHeldIfCrossThreadForDebugging();
  *changed = web_media_source_->ChangeTo(*web_source_buffers);
}

void MediaSource::UpdateTracksAfterChangeTo(
    const HeapVector<Member<SourceBuffer>>& source_buffers) {
  for (unsigned i = 0; i < source_buffers_->length(); ++i) {
    SourceBuffer* source_buffer = source_buffers_->item(i);
    source_buffer->OnChangeTo(
        /*selected=*/source_buffers.Contains(source_buffer),
        /*was_active=*/active_source_buffers_->Contains(source_buffer));
  }

  // Disable the audio of the SourceBuffers switched away from, then enable or
  // select the first track of each type of |source_buffers|. The demuxer has
  // already switched, so the resulting track changes don't switch streams.
  for (unsigned i = 0; i < source_buffers_->length(); ++i) {
    SourceBuffer* source_buffer = source_buffers_->item(i);
    if (source_buffers.Contains(source_buffer)) {
      continue;
    }
    AudioTrackList& audio_tracks = source_buffer->audioTracks();
    for (unsigned j = 0; j < audio_tracks.length(); ++j) {
      AudioTrack* audio_track = audio_tracks.AnonymousIndexedGetter(j);
      if (audio_track->enabled()) {
        audio_track->setEnabled(false);
      }
    }
  }
  for (const Member<SourceBuffer>& source_buffer : source_buffers) {
    AudioTrackList& audio_tracks = source_buffer->audioTracks();
    if (audio_tracks.length() > 0) {
      audio_tracks.AnonymousIndexedGetter(0)->setEnabled(true);
    }
    VideoTrackList& video_tracks = source_buffer->videoTracks();
    if (video_tracks.length() > 0) {
      video_tracks.AnonymousIndexedGetter(0)->setSelected(true);
    }
  }

  // Selecting a video track deselects the element's other video tracks
  // without telling this MediaSource, so update activeSourceBuffers for every
  // SourceBuffer.
  for (unsigned i = 0; i < source_buffers_->length(); ++i) {
    SourceBuffer* source_buffer = source_buffers_->item(i);
    const bool is_active = source_buffer->videoTracks().selectedIndex() != -1 ||
                           source_buffer->audioTracks().HasEnabledTrack();
    if (is_active != active_source_buffers_->Contains(source_buffer)) {
      SetSourceBufferActive(source_buffer, is_active);
    }
  }
}

bool SourceBuffer::SetStandby_Locked(
    MediaSourceAttachmentSupplement::ExclusiveKey /* passkey */) {
  DCHECK(web_source_buffer_);
  standby_ = web_source_buffer_->SetStandby();
  return standby_;
}

void SourceBuffer::OnChangeTo(bool selected, bool was_active) {
  if (selected) {
    standby_ = false;
    switched_away_ = false;
  } else if (was_active) {
    switched_away_ = true;
  }
}

}  // namespace blink
