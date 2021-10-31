// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "SkTLS.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local_storage.h"

namespace {
typedef base::ThreadLocalStorage::Slot Slot;

// Overriding (leaky) lazy traits so we can pass the TLS destructor into the TLS
// Slot constructor.
struct SlotTraits : public base::internal::LeakyLazyInstanceTraits<Slot> {
  static Slot* New(void* instance) {
    return new (instance) Slot(SkTLS::Destructor);
  }
};

base::LazyInstance<Slot, SlotTraits> gSkTLSKey = LAZY_INSTANCE_INITIALIZER;
}  // namespace

void* SkTLS::PlatformGetSpecific(bool forceCreateTheSlot) {
  return gSkTLSKey.Get().Get();
}

void SkTLS::PlatformSetSpecific(void* ptr) { return gSkTLSKey.Get().Set(ptr); }
