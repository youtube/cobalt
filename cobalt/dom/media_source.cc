// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/dom/media_source.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/event.h"
#include "cobalt/media/can_play_type_handler.h"
#include "starboard/double.h"

namespace cobalt {
namespace dom {

using ::media::WebMediaPlayer;

namespace {

// Parse mime and codecs from content type. It will return "video/mp4" and
// ("avc1.42E01E", "mp4a.40.2") for "video/mp4; codecs="avc1.42E01E, mp4a.40.2".
// Note that this function does minimum validation as the media stack will check
// the mime type and codecs strictly.
bool ParseContentType(const std::string& content_type, std::string* mime,
                      std::vector<std::string>* codecs) {
  DCHECK(mime);
  DCHECK(codecs);
  static const char kCodecs[] = "codecs=";

  std::vector<std::string> tokens;
  // SplitString will also trim the results.
  ::base::SplitString(content_type, ';', &tokens);
  // The first one has to be mime type with delimiter '/' like 'video/mp4'.
  if (tokens.size() < 2 || tokens[0].find('/') == tokens[0].npos) {
    return false;
  }
  *mime = tokens[0];
  for (size_t i = 1; i < tokens.size(); ++i) {
    if (strncasecmp(tokens[i].c_str(), kCodecs, strlen(kCodecs))) {
      continue;
    }
    std::string codec_string = tokens[i].substr(strlen("codecs="));
    TrimString(codec_string, " \"", &codec_string);
    // SplitString will also trim the results.
    ::base::SplitString(codec_string, ',', codecs);
    break;
  }
  return !codecs->empty();
}

}  // namespace

MediaSource::MediaSource()
    : ready_state_(kMediaSourceReadyStateClosed),
      player_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(event_queue_(this)),
      source_buffers_(new SourceBufferList(&event_queue_)) {}

scoped_refptr<SourceBufferList> MediaSource::source_buffers() const {
  return source_buffers_;
}

scoped_refptr<SourceBufferList> MediaSource::active_source_buffers() const {
  // All source buffers are 'active' as we don't support buffer selection.
  return source_buffers_;
}

double MediaSource::duration(script::ExceptionState* exception_state) const {
  UNREFERENCED_PARAMETER(exception_state);

  if (ready_state_ == kMediaSourceReadyStateClosed) {
    return std::numeric_limits<float>::quiet_NaN();
  }

  DCHECK(player_);
  return player_->SourceGetDuration();
}

void MediaSource::set_duration(double duration,
                               script::ExceptionState* exception_state) {
  if (duration < 0.0 || SbDoubleIsNan(duration)) {
    DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
    return;
  }
  if (ready_state_ != kMediaSourceReadyStateOpen) {
    DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
    return;
  }
  DCHECK(player_);
  player_->SourceSetDuration(duration);
}

scoped_refptr<SourceBuffer> MediaSource::AddSourceBuffer(
    const std::string& type, script::ExceptionState* exception_state) {
  DLOG(INFO) << "add SourceBuffer with type " << type;

  if (type.empty()) {
    DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
    // Return value should be ignored.
    return NULL;
  }

  std::string mime;
  std::vector<std::string> codecs;

  if (!ParseContentType(type, &mime, &codecs)) {
    DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
    // Return value should be ignored.
    return NULL;
  }

  if (!player_ || ready_state_ != kMediaSourceReadyStateOpen) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    // Return value should be ignored.
    return NULL;
  }

  // 5. Create a new SourceBuffer object and associated resources.
  std::string id = source_buffers_->GenerateUniqueId();
  DCHECK(!id.empty());

  scoped_refptr<SourceBuffer> source_buffer = new SourceBuffer(this, id);

  switch (player_->SourceAddId(source_buffer->id(), mime, codecs)) {
    case WebMediaPlayer::kAddIdStatusOk:
      source_buffers_->Add(source_buffer);
      return source_buffer;
    case WebMediaPlayer::kAddIdStatusNotSupported:
      DOMException::Raise(DOMException::kNotSupportedErr, exception_state);
      // Return value should be ignored.
      return NULL;
    case WebMediaPlayer::kAddIdStatusReachedIdLimit:
      DOMException::Raise(DOMException::kQuotaExceededErr, exception_state);
      // Return value should be ignored.
      return NULL;
  }

  NOTREACHED();
  return NULL;
}

void MediaSource::RemoveSourceBuffer(
    const scoped_refptr<SourceBuffer>& source_buffer,
    script::ExceptionState* exception_state) {
  if (source_buffer == NULL) {
    DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
    return;
  }

  if (!player_ || source_buffers_->length() == 0) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  if (!source_buffers_->Remove(source_buffer)) {
    DOMException::Raise(DOMException::kNotFoundErr, exception_state);
    return;
  }

  player_->SourceRemoveId(source_buffer->id());
}

MediaSourceReadyState MediaSource::ready_state() const { return ready_state_; }

void MediaSource::EndOfStream(script::ExceptionState* exception_state) {
  // If there is no error string provided, treat it as empty.
  EndOfStream(kMediaSourceEndOfStreamErrorNoError, exception_state);
}

void MediaSource::EndOfStream(MediaSourceEndOfStreamError error,
                              script::ExceptionState* exception_state) {
  if (!player_ || ready_state_ != kMediaSourceReadyStateOpen) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  WebMediaPlayer::EndOfStreamStatus eos_status =
      WebMediaPlayer::kEndOfStreamStatusNoError;

  if (error == kMediaSourceEndOfStreamErrorNoError) {
    eos_status = WebMediaPlayer::kEndOfStreamStatusNoError;
  } else if (error == kMediaSourceEndOfStreamErrorNetwork) {
    eos_status = WebMediaPlayer::kEndOfStreamStatusNetworkError;
  } else if (error == kMediaSourceEndOfStreamErrorDecode) {
    eos_status = WebMediaPlayer::kEndOfStreamStatusDecodeError;
  } else {
    DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
    return;
  }

  SetReadyState(kMediaSourceReadyStateEnded);
  player_->SourceEndOfStream(eos_status);
}

// static
bool MediaSource::IsTypeSupported(script::EnvironmentSettings* settings,
                                  const std::string& type) {
  DOMSettings* dom_settings =
      base::polymorphic_downcast<DOMSettings*>(settings);
  DCHECK(dom_settings);
  media::CanPlayTypeHandler* handler = dom_settings->can_play_type_handler();
  DCHECK(handler);
  std::string result = handler->CanPlayType(type, "");
  DLOG(INFO) << "MediaSource::IsTypeSupported(" << type << ") -> " << result;
  return result == "probably";
}

void MediaSource::SetPlayer(WebMediaPlayer* player) {
  // It is possible to reuse a MediaSource object but unlikely. DCHECK it until
  // it is used in this way.
  DCHECK(!player_);
  player_ = player;
}

void MediaSource::ScheduleEvent(base::Token event_name) {
  event_queue_.Enqueue(new Event(event_name));
}

scoped_refptr<TimeRanges> MediaSource::GetBuffered(
    const SourceBuffer* source_buffer,
    script::ExceptionState* exception_state) {
  if (!player_ || ready_state_ == kMediaSourceReadyStateClosed) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    // Return value should be ignored.
    return NULL;
  }

  ::media::Ranges<base::TimeDelta> media_time_ranges =
      player_->SourceBuffered(source_buffer->id());
  scoped_refptr<TimeRanges> dom_time_ranges = new TimeRanges;
  for (int index = 0; index < static_cast<int>(media_time_ranges.size());
       ++index) {
    dom_time_ranges->Add(media_time_ranges.start(index).InSecondsF(),
                         media_time_ranges.end(index).InSecondsF());
  }
  return dom_time_ranges;
}

bool MediaSource::SetTimestampOffset(const SourceBuffer* source_buffer,
                                     double timestamp_offset,
                                     script::ExceptionState* exception_state) {
  if (!player_ || ready_state_ == kMediaSourceReadyStateClosed) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    // Return value should be ignored.
    return false;
  }
  if (!player_->SourceSetTimestampOffset(source_buffer->id(),
                                         timestamp_offset)) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    // Return value should be ignored.
    return false;
  }
  return true;
}

void MediaSource::Append(const SourceBuffer* source_buffer, const uint8* buffer,
                         int size, script::ExceptionState* exception_state) {
  if (!buffer) {
    DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
    return;
  }

  if (!player_ || ready_state_ == kMediaSourceReadyStateClosed) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  if (ready_state_ == kMediaSourceReadyStateEnded) {
    SetReadyState(kMediaSourceReadyStateOpen);
  }

  TRACE_EVENT1("media_stack", "MediaSource::Append()", "size", size);

  // If size is greater than kMaxAppendSize, we will append the data in multiple
  // small chunks with size less than or equal to kMaxAppendSize.  This can
  // avoid memory allocation spike as ChunkDemuxer may try to allocator memory
  // in size around 'append_size * 2'.
  const int kMaxAppendSize = 128 * 1024;
  int offset = 0;
  while (offset < size) {
    int chunk_size = std::min(size - offset, kMaxAppendSize);
    if (!player_->SourceAppend(source_buffer->id(), buffer + offset,
                               static_cast<unsigned int>(chunk_size))) {
      DOMException::Raise(DOMException::kSyntaxErr, exception_state);
      return;
    }
    offset += chunk_size;
  }
}

void MediaSource::Abort(const SourceBuffer* source_buffer,
                        script::ExceptionState* exception_state) {
  if (!player_ || ready_state_ != kMediaSourceReadyStateOpen) {
    DOMException::Raise(DOMException::kInvalidStateErr, exception_state);
    return;
  }

  bool aborted = player_->SourceAbort(source_buffer->id());
  DCHECK(aborted);
}

void MediaSource::SetReadyState(MediaSourceReadyState ready_state) {
  if (ready_state_ == ready_state) {
    return;
  }

  MediaSourceReadyState old_state = ready_state_;
  ready_state_ = ready_state;

  if (ready_state_ == kMediaSourceReadyStateClosed) {
    source_buffers_->Clear();
    player_ = NULL;
    ScheduleEvent(base::Tokens::sourceclose());
    return;
  }

  if (old_state == kMediaSourceReadyStateOpen &&
      ready_state_ == kMediaSourceReadyStateEnded) {
    ScheduleEvent(base::Tokens::sourceended());
    return;
  }

  if (ready_state_ == kMediaSourceReadyStateOpen) {
    ScheduleEvent(base::Tokens::sourceopen());
  }
}

void MediaSource::TraceMembers(script::Tracer* tracer) {
  EventTarget::TraceMembers(tracer);

  event_queue_.TraceMembers(tracer);
  tracer->Trace(source_buffers_);
}

}  // namespace dom
}  // namespace cobalt
