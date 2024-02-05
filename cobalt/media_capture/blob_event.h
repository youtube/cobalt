// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_CAPTURE_BLOB_EVENT_H_
#define COBALT_MEDIA_CAPTURE_BLOB_EVENT_H_

#include <limits>
#include <string>

#include "cobalt/media_capture/blob_event_init.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/blob.h"
#include "cobalt/web/event.h"

namespace cobalt {
namespace media_capture {

// Implements the WebAPI defined at
// https://w3c.github.io/mediacapture-record/#blob-event
class BlobEvent : public web::Event {
 public:
  explicit BlobEvent(base_token::Token type,
                     const media_capture::BlobEventInit& blob_event_init)
      : web::Event(type) {
    Init(blob_event_init);
  }
  explicit BlobEvent(const std::string& type,
                     const media_capture::BlobEventInit& blob_event_init)
      : web::Event(type) {
    Init(blob_event_init);
  }
  explicit BlobEvent(base_token::Token type,
                     const scoped_refptr<web::Blob>& blob, double timecode)
      : web::Event(type), blob_(blob), timecode_(timecode) {}
  explicit BlobEvent(const std::string& type,
                     const scoped_refptr<web::Blob>& blob, double timecode)
      : web::Event(type), blob_(blob), timecode_(timecode) {}

  scoped_refptr<web::Blob> data() const { return blob_; }
  double timecode() const { return timecode_; }

  DEFINE_WRAPPABLE_TYPE(BlobEvent);

  void TraceMembers(script::Tracer* tracer) override {
    web::Event::TraceMembers(tracer);
    tracer->Trace(blob_);
  }

 protected:
  void Init(const media_capture::BlobEventInit& blob_event_init) {
    if (blob_event_init.has_data()) {
      blob_ = blob_event_init.data();
    }
    if (blob_event_init.has_timecode()) {
      timecode_ = blob_event_init.timecode();
    }
  }

  scoped_refptr<web::Blob> blob_;
  double timecode_ = std::numeric_limits<double>::quiet_NaN();
};

}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_BLOB_EVENT_H_
