// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_TRACK_BASE_H_
#define COBALT_DOM_TRACK_BASE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/script/wrappable.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace dom {

class SourceBuffer;

// This class contains common operations for AudioTrack and VideoTrack.
class TrackBase : public script::Wrappable {
 public:
  const std::string& id() const { return id_; }
  const std::string& kind() const { return kind_; }
  const std::string& label() const { return label_; }
  const std::string& language() const { return language_; }

  void SetMediaElement(HTMLMediaElement* media_element) {
    media_element_ = base::AsWeakPtr(media_element);
  }
  SourceBuffer* source_buffer() const { return source_buffer_; }

 protected:
  typedef bool (*IsValidKindFn)(const char*);

  TrackBase(const std::string& id, const std::string& kind,
            const std::string& label, const std::string& language,
            IsValidKindFn is_valid_kind_fn)
      : id_(id), label_(label), language_(language), source_buffer_(NULL) {
    if (is_valid_kind_fn(kind.c_str())) {
      kind_ = kind;
    }
  }

  std::string id_;
  std::string kind_;
  std::string label_;
  std::string language_;
  base::WeakPtr<HTMLMediaElement> media_element_;
  SourceBuffer* source_buffer_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TRACK_BASE_H_
