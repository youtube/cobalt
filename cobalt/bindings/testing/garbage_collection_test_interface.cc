/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

GarbageCollectionTestInterface::GarbageCollectionTestInterface() {
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

GarbageCollectionTestInterface::GarbageCollectionTestInterfaceVector&
GarbageCollectionTestInterface::instances() {
  return ::cobalt::bindings::testing::instances.Get();
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt
