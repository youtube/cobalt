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

#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/dom/media_source/source_buffer_list.h"
#define COBALT_DOM_SOURCE_BUFFER_LIST_H_
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

#ifndef COBALT_DOM_SOURCE_BUFFER_LIST_H_
#define COBALT_DOM_SOURCE_BUFFER_LIST_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/source_buffer.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The SourceBufferList interface represents a list of SourceBuffer. It is used
// to access all the SourceBuffers and the active SourceBuffers of a MediaSource
// object.
//   https://rawgit.com/w3c/media-source/6747ed7a3206f646963d760100b0f37e2fc7e47e/media-source.html#sourcebufferlist
class SourceBufferList : public EventTarget {
 public:
  // Custom, not in any spec.
  //
  explicit SourceBufferList(EventQueue* event_queue);
  ~SourceBufferList() override;

  // Web API: SourceBuffer
  //
  uint32 length() const;
  scoped_refptr<SourceBuffer> Item(uint32 index) const;

  // Custom, not in any spec.
  //
  // Generates an id for adding a new SourceBuffer. This id is unique in the
  // context of this SourceBufferList.
  std::string GenerateUniqueId();
  void Add(const scoped_refptr<SourceBuffer>& source_buffer);
  bool Remove(const scoped_refptr<SourceBuffer>& source_buffer);
  void Clear();

  DEFINE_WRAPPABLE_TYPE(SourceBufferList);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  // From EventTarget.
  std::string GetDebugName() override { return "SourceBufferList"; }

  // Use vector so we can reserve memory in ctor.
  typedef std::vector<scoped_refptr<SourceBuffer> > SourceBuffers;
  SourceBuffers source_buffers_;
  int32 last_source_buffer_id_;
  EventQueue* event_queue_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SOURCE_BUFFER_LIST_H_
