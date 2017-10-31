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

#include "cobalt/dom/source_buffer_list.h"

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
    : last_source_buffer_id_(0), event_queue_(event_queue) {
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

std::string SourceBufferList::GenerateUniqueId() {
  std::string result = base::StringPrintf("%d", last_source_buffer_id_);
  ++last_source_buffer_id_;
  return result;
}

void SourceBufferList::Add(const scoped_refptr<SourceBuffer>& source_buffer) {
  SourceBuffers::iterator iter =
      std::find(source_buffers_.begin(), source_buffers_.end(), source_buffer);
  DLOG_IF(WARNING, iter != source_buffers_.end())
      << "SourceBuffer " << source_buffer->id() << " has been added";
  if (iter == source_buffers_.end()) {
    source_buffers_.push_back(source_buffer);
    event_queue_->Enqueue(new Event(base::Tokens::addsourcebuffer()));
  }
}

bool SourceBufferList::Remove(
    const scoped_refptr<SourceBuffer>& source_buffer) {
  SourceBuffers::iterator iter =
      std::find(source_buffers_.begin(), source_buffers_.end(), source_buffer);
  DLOG_IF(WARNING, iter == source_buffers_.end())
      << "SourceBuffer " << source_buffer->id() << " hasn't been added";
  if (iter == source_buffers_.end()) {
    return false;
  }

  source_buffer->Close();
  source_buffers_.erase(iter);
  event_queue_->Enqueue(new Event(base::Tokens::removesourcebuffer()));
  return true;
}

void SourceBufferList::Clear() {
  while (!source_buffers_.empty()) {
    Remove(source_buffers_[0]);
  }
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

}  // namespace dom
}  // namespace cobalt
