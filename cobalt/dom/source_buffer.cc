/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/source_buffer.h"

#include <vector>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/dom/media_source.h"

namespace cobalt {
namespace dom {

SourceBuffer::SourceBuffer(const scoped_refptr<MediaSource>& media_source,
                           const std::string& id)
    : media_source_(media_source), id_(id), timestamp_offset_(0) {
  DCHECK(media_source_);
  DCHECK(!id_.empty());
}

scoped_refptr<TimeRanges> SourceBuffer::buffered() const {
  if (!media_source_) {
    // TODO(***REMOVED***): Raise INVALID_STATE_ERR.
    NOTREACHED();
    return NULL;
  }

  return media_source_->GetBuffered(this);
}

double SourceBuffer::timestamp_offset() const { return timestamp_offset_; }

void SourceBuffer::set_timestamp_offset(double offset) {
  if (!media_source_) {
    // TODO(***REMOVED***): Raise INVALID_STATE_ERR if media_source_ is NULL;
    NOTREACHED();
    return;
  }

  if (media_source_->SetTimestampOffset(this, offset)) {
    timestamp_offset_ = offset;
  }
}

void SourceBuffer::Append(const scoped_refptr<Uint8Array>& data) {
  if (!media_source_) {
    // TODO(***REMOVED***): Raise INVALID_STATE_ERR if media_source_ is NULL;
    NOTREACHED();
    return;
  }

  if (data->length()) {
    media_source_->Append(this, data->data(), static_cast<int>(data->length()));
  }
}

void SourceBuffer::Abort() {
  if (!media_source_) {
    // TODO(***REMOVED***): Raise INVALID_STATE_ERR if media_source_ is NULL;
    NOTREACHED();
    return;
  }

  media_source_->Abort(this);
}

void SourceBuffer::Close() { media_source_ = NULL; }

}  // namespace dom
}  // namespace cobalt
