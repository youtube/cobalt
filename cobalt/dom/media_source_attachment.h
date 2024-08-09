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

#ifndef COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_H_
#define COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/media_source_ready_state.h"
#include "cobalt/dom/time_ranges.h"
#include "cobalt/media/base/chunk_demuxer_holder.h"
#include "cobalt/script/tracer.h"
#include "cobalt/web/url_registry.h"
#include "media/filters/chunk_demuxer.h"

namespace cobalt {
namespace dom {

// Interface for potential cross-context management of MediaSource objects.
// Used with the MediaSourceRegistry for attaching MediaSources to media
// elements.
class MediaSourceAttachment
    : public base::RefCountedThreadSafe<MediaSourceAttachment>,
      public script::Traceable {
 public:
  typedef web::UrlRegistry<MediaSourceAttachment> Registry;

  MediaSourceAttachment() = default;
  ~MediaSourceAttachment() override = default;

  // These two methods are called in sequence when an HTMLMediaElement is
  // attempting to attach to the MediaSource object using this attachment
  // instance. The ChunkDemuxer is not available to the element initially, so
  // between the two calls, the attachment could be considered partially setup.
  // If attachment start fails (for example, if the underlying MediaSource is
  // already attached, or if this attachment has already been unregistered from
  // the MediaSourceRegistry), StartAttachingToMediaElement() returns false.
  // Otherwise, the underlying MediaSource must be in 'closed' state, and
  // indicates success by returning true.
  // CompleteAttachingToMediaElement() provides the attached MediaSource with
  // the underlying ChunkDemuxer, enabling parsing of media provided by the
  // application for playback, for example.
  // Once attached, the MediaSource and the HTMLMediaElement use each other via
  // this attachment to accomplish the extended API.
  virtual bool StartAttachingToMediaElement(
      HTMLMediaElement* media_element) = 0;
  virtual void CompleteAttachingToMediaElement(
      media::ChunkDemuxerHolder* chunk_demuxer) = 0;

  virtual void Close() = 0;

  virtual scoped_refptr<TimeRanges> GetBufferedRange() const = 0;
  virtual MediaSourceReadyState GetReadyState() const = 0;

  // TODO(b/338425449): Remove media_source method after rollout.
  // References to underlying objects are exposed for when the H5VCC
  // flag MediaElement.EnableUsingMediaSourceAttachmentMethods is disabled.
  virtual scoped_refptr<MediaSource> media_source() const = 0;

 private:
  friend class base::RefCountedThreadSafe<MediaSourceAttachment>;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceAttachment);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_H_
