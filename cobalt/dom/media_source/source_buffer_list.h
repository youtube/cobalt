/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Modifications Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_MEDIA_SOURCE_SOURCE_BUFFER_LIST_H_
#define COBALT_DOM_MEDIA_SOURCE_SOURCE_BUFFER_LIST_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/event_queue.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/media_source/source_buffer.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The SourceBufferList interface represents a list of SourceBuffer. It is used
// to access all the SourceBuffers and the active SourceBuffers of a MediaSource
// object.
//   https://www.w3.org/TR/2016/CR-media-source-20160705/#sourcebufferlist
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
  void Add(const scoped_refptr<SourceBuffer>& source_buffer);
  void Insert(size_t position,
              const scoped_refptr<SourceBuffer>& source_buffer);
  void Remove(const scoped_refptr<SourceBuffer>& source_buffer);
  size_t Find(const scoped_refptr<SourceBuffer>& source_buffer) const;
  bool Contains(const scoped_refptr<SourceBuffer>& source_buffer) const;
  void Clear();

  DEFINE_WRAPPABLE_TYPE(SourceBufferList);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  // From EventTarget.
  std::string GetDebugName() override { return "SourceBufferList"; }
  void ScheduleEvent(base::Token event_name);

  // Use vector so we can reserve memory in ctor.
  typedef std::vector<scoped_refptr<SourceBuffer> > SourceBuffers;
  SourceBuffers source_buffers_;
  EventQueue* event_queue_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_SOURCE_SOURCE_BUFFER_LIST_H_
