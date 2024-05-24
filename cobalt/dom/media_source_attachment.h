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

#ifndef COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_H_
#define COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/script/tracer.h"
#include "cobalt/web/url_registry.h"

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

  explicit MediaSourceAttachment(scoped_refptr<MediaSource> media_source)
      : media_source_(media_source) {}

  scoped_refptr<MediaSource> media_source() const { return media_source_; }

  void TraceMembers(script::Tracer* tracer) override {
    tracer->Trace(media_source_);
  }

 private:
  friend class base::RefCountedThreadSafe<MediaSourceAttachment>;
  ~MediaSourceAttachment() = default;

  scoped_refptr<MediaSource> media_source_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceAttachment);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_ATTACHMENT_H_
