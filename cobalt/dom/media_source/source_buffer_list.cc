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

#include "cobalt/dom/media_source/source_buffer_list.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/event.h"

namespace cobalt {
namespace dom {

namespace {
// SourceBufferList usually contains 2 source buffers.
const int kSizeOfSourceBufferToReserveInitially = 2;
}  // namespace

SourceBufferList::SourceBufferList(EventQueue* event_queue)
    : event_queue_(event_queue) {
  DCHECK(event_queue_);
  source_buffers_.reserve(kSizeOfSourceBufferToReserveInitially);
}

SourceBufferList::~SourceBufferList() { DCHECK(source_buffers_.empty()); }

uint32 SourceBufferList::length() const {
  DCHECK_LE(source_buffers_.size(), std::numeric_limits<uint32>::max());
  return static_cast<uint32>(source_buffers_.size());
}

scoped_refptr<SourceBuffer> SourceBufferList::Item(uint32 index) const {
  if (index < source_buffers_.size()) {
    return source_buffers_[index];
  }
  return NULL;
}

void SourceBufferList::Add(const scoped_refptr<SourceBuffer>& source_buffer) {
  source_buffers_.push_back(source_buffer);
  ScheduleEvent(base::Tokens::addsourcebuffer());
}

void SourceBufferList::Insert(
    size_t position, const scoped_refptr<SourceBuffer>& source_buffer) {
  source_buffers_.insert(source_buffers_.begin() + position, source_buffer);
  ScheduleEvent(base::Tokens::addsourcebuffer());
}

void SourceBufferList::Remove(
    const scoped_refptr<SourceBuffer>& source_buffer) {
  SourceBuffers::iterator iter =
      std::find(source_buffers_.begin(), source_buffers_.end(), source_buffer);
  if (iter == source_buffers_.end()) {
    return;
  }

  source_buffers_.erase(iter);
  ScheduleEvent(base::Tokens::removesourcebuffer());
}

size_t SourceBufferList::Find(
    const scoped_refptr<SourceBuffer>& source_buffer) const {
  DCHECK(Contains(source_buffer));
  return std::distance(
      source_buffers_.begin(),
      std::find(source_buffers_.begin(), source_buffers_.end(), source_buffer));
}

bool SourceBufferList::Contains(
    const scoped_refptr<SourceBuffer>& source_buffer) const {
  return std::find(source_buffers_.begin(), source_buffers_.end(),
                   source_buffer) != source_buffers_.end();
}

void SourceBufferList::Clear() {
  source_buffers_.clear();
  ScheduleEvent(base::Tokens::removesourcebuffer());
}

void SourceBufferList::TraceMembers(script::Tracer* tracer) {
  EventTarget::TraceMembers(tracer);

  if (event_queue_) {
    event_queue_->TraceMembers(tracer);
  }
  for (const auto& source_buffer : source_buffers_) {
    tracer->Trace(source_buffer);
  }
}

void SourceBufferList::ScheduleEvent(base::Token event_name) {
  scoped_refptr<Event> event = new Event(event_name);
  event->set_target(this);
  event_queue_->Enqueue(event);
}

}  // namespace dom
}  // namespace cobalt
