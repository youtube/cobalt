// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/bindings/testing/garbage_collection_test_interface.h"

#include "base/lazy_instance.h"

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
base::LazyInstance<
    GarbageCollectionTestInterface::GarbageCollectionTestInterfaceVector>
    instances;
}  // namespace

GarbageCollectionTestInterface::GarbageCollectionTestInterfaceVector&
GarbageCollectionTestInterface::instances() {
  return ::cobalt::bindings::testing::instances.Get();
}

GarbageCollectionTestInterface::GarbageCollectionTestInterface()
    : previous_(NULL) {
  instances().push_back(this);
}

GarbageCollectionTestInterface::~GarbageCollectionTestInterface() {
  for (GarbageCollectionTestInterfaceVector::iterator it = instances().begin();
       it != instances().end(); ++it) {
    if (*it == this) {
      instances().erase(it);
      break;
    }
  }
}

void GarbageCollectionTestInterface::set_previous(
    const scoped_refptr<GarbageCollectionTestInterface>& previous) {
  // Make the current previous_ the tail of its own list.
  if (previous_) {
    previous_->MakeTail();
  }
  Join(previous.get(), this);
}

void GarbageCollectionTestInterface::set_next(
    const scoped_refptr<GarbageCollectionTestInterface>& next) {
  // Make the current next_ the head of its own list.
  if (next_) {
    next_->MakeHead();
  }
  Join(this, next.get());
}

void GarbageCollectionTestInterface::Join(
    GarbageCollectionTestInterface* first,
    GarbageCollectionTestInterface* second) {
  if (first) {
    first->next_ = second;
  }
  if (second) {
    second->previous_ = first;
  }
}

void GarbageCollectionTestInterface::MakeHead() {
  if (previous_) {
    DCHECK(previous_->next_ == this);
    previous_->next_ = NULL;
  }
  previous_ = NULL;
}

void GarbageCollectionTestInterface::MakeTail() {
  if (next_) {
    DCHECK(next_->previous_ == this);
    next_->previous_ = NULL;
  }
  next_ = NULL;
}

void GarbageCollectionTestInterface::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(previous_);
  tracer->Trace(next_);
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
