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
// standby SourceBuffers (MediaSource.addStandbySourceBuffer()) and
// MediaSource.changeTo(). The H5VCC setting
// `Media.EnableMultiSourceBufferSwitching` exposes them.

// clang-format off
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
// clang-format on

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "base/auto_reset.h"
#include "base/check.h"
#include "third_party/blink/public/platform/web_source_buffer.h"
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

SourceBuffer* MediaSource::addStandbySourceBuffer(
    const String& type,
    ExceptionState& exception_state) {
  // Multi-SourceBuffer switching is only for MediaSources on the main thread,
  // where addSourceBuffer() creates the WebSourceBuffer synchronously.
  DCHECK(IsMainThread());
  SourceBuffer* source_buffer = nullptr;
  {
    // Makes CreateWebSourceBuffer() add a standby WebSourceBuffer, which the
    // demuxer accepts even after it initialized. Everything else is as in
    // addSourceBuffer(), including its exceptions.
    base::AutoReset<bool> adding_standby(&adding_standby_source_buffer_, true);
    source_buffer = addSourceBuffer(type, exception_state);
  }
  if (source_buffer) {
    // Before any append can create its tracks.
    source_buffer->MarkStandby();
  }
  return source_buffer;
}

void MediaSource::changeTo(
    const HeapVector<Member<SourceBuffer>>& source_buffers,
    ExceptionState& exception_state) {
  // Multi-SourceBuffer switching is only for MediaSources on the main thread.
  DCHECK(IsMainThread());
  // An "ended" MediaSource reopens after the switch, as for appendBuffer():
  // the media switched to is played, and a page whose current media was fully
  // buffered has often called endOfStream().
  if (IsClosed()) {
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "The MediaSource's readyState is 'closed'.");
    return;
  }
  if (source_buffers.empty()) {
    LogAndThrowTypeError(exception_state, "No SourceBuffers to change to.");
    return;
  }
  for (const Member<SourceBuffer>& source_buffer : source_buffers) {
    if (!source_buffers_->Contains(source_buffer)) {
      LogAndThrowDOMException(
          exception_state, DOMExceptionCode::kInvalidStateError,
          "A SourceBuffer isn't in this MediaSource's sourceBuffers.");
      return;
    }
    // The seek that completes the switch needs media to seek to.
    TimeRanges* ranges = source_buffer->buffered(ASSERT_NO_EXCEPTION);
    if (!ranges || ranges->length() == 0) {
      LogAndThrowDOMException(exception_state,
                              DOMExceptionCode::kInvalidStateError,
                              "A SourceBuffer has no buffered media.");
      return;
    }
  }
  // Before the media element has metadata, setting its currentTime doesn't
  // seek, so the switch wouldn't complete.
  HTMLMediaElement* media_element = nullptr;
  {
    auto [attachment, tracer] = AttachmentAndTracer();
    // Only a same-thread attachment has a tracer.
    if (tracer) {
      media_element =
          static_cast<SameThreadMediaSourceTracer*>(tracer)->GetMediaElement();
    }
  }
  if (!media_element ||
      media_element->getReadyState() == HTMLMediaElement::kHaveNothing) {
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kInvalidStateError,
                            "The media element has no metadata yet.");
    return;
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
    return;
  }
  if (!changed) {
    LogAndThrowDOMException(exception_state,
                            DOMExceptionCode::kNotSupportedError,
                            "Can't change to these SourceBuffers.");
    return;
  }

  // The demuxer switches while ended, so that a refused switch leaves
  // readyState as it was. Reopening unmarks the end of stream of every
  // SourceBuffer, for the media switched to.
  OpenIfInEndedState();

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
  media_element->setCurrentTime(start);
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
  // already switched, so update the tracks without notifying the media
  // element: a track change would reach the pipeline, which would switch
  // streams again and cancel the seek that completes this switch.
  HTMLMediaElement* element = nullptr;
  bool audio_changed = false;
  bool video_changed = false;
  for (unsigned i = 0; i < source_buffers_->length(); ++i) {
    SourceBuffer* source_buffer = source_buffers_->item(i);
    if (source_buffers.Contains(source_buffer)) {
      continue;
    }
    AudioTrackList& audio_tracks = source_buffer->audioTracks();
    bool changed = false;
    for (unsigned j = 0; j < audio_tracks.length(); ++j) {
      AudioTrack* audio_track = audio_tracks.AnonymousIndexedGetter(j);
      if (audio_track->enabled()) {
        audio_track->ClearEnabled();
        element = element ? element : audio_track->MediaElement();
        changed = true;
      }
    }
    if (changed) {
      audio_tracks.ScheduleChangeEvent();
      audio_changed = true;
    }
    VideoTrackList& video_tracks = source_buffer->videoTracks();
    changed = false;
    for (unsigned j = 0; j < video_tracks.length(); ++j) {
      VideoTrack* video_track = video_tracks.AnonymousIndexedGetter(j);
      if (video_track->selected()) {
        video_track->ClearSelected();
        element = element ? element : video_track->MediaElement();
        changed = true;
      }
    }
    if (changed) {
      video_tracks.ScheduleChangeEvent();
      video_changed = true;
    }
  }
  for (const Member<SourceBuffer>& source_buffer : source_buffers) {
    AudioTrackList& audio_tracks = source_buffer->audioTracks();
    if (audio_tracks.length() > 0 &&
        !audio_tracks.AnonymousIndexedGetter(0)->enabled()) {
      AudioTrack* audio_track = audio_tracks.AnonymousIndexedGetter(0);
      HTMLMediaElement* track_element = audio_track->MediaElement();
      audio_track->SetMediaElement(nullptr);
      audio_track->setEnabled(true);
      audio_track->SetMediaElement(track_element);
      element = element ? element : track_element;
      audio_tracks.ScheduleChangeEvent();
      audio_changed = true;
    }
    VideoTrackList& video_tracks = source_buffer->videoTracks();
    if (video_tracks.length() > 0 &&
        !video_tracks.AnonymousIndexedGetter(0)->selected()) {
      VideoTrack* video_track = video_tracks.AnonymousIndexedGetter(0);
      HTMLMediaElement* track_element = video_track->MediaElement();
      video_track->SetMediaElement(nullptr);
      video_track->setSelected(true);
      video_track->SetMediaElement(track_element);
      element = element ? element : track_element;
      video_tracks.ScheduleChangeEvent();
      video_changed = true;
    }
  }
  if (element && audio_changed) {
    element->audioTracks().ScheduleChangeEvent();
  }
  if (element && video_changed) {
    element->videoTracks().ScheduleChangeEvent();
  }

  // Update activeSourceBuffers for every SourceBuffer.
  for (unsigned i = 0; i < source_buffers_->length(); ++i) {
    SourceBuffer* source_buffer = source_buffers_->item(i);
    const bool is_active = source_buffer->videoTracks().selectedIndex() != -1 ||
                           source_buffer->audioTracks().HasEnabledTrack();
    if (is_active != active_source_buffers_->Contains(source_buffer)) {
      SetSourceBufferActive(source_buffer, is_active);
    }
  }
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
