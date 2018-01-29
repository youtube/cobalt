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

#ifndef COBALT_BINDINGS_TESTING_GARBAGE_COLLECTION_TEST_INTERFACE_H_
#define COBALT_BINDINGS_TESTING_GARBAGE_COLLECTION_TEST_INTERFACE_H_

#include <vector>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace bindings {
namespace testing {

// GarbageCollectionTestInterface maintains strong references from head -> tail,
// and raw pointers from tail -> head to prevent reference cycles, similar to
// how references between Nodes are handled in Cobalt.
class GarbageCollectionTestInterface : public script::Wrappable {
 public:
  typedef std::vector<GarbageCollectionTestInterface*>
      GarbageCollectionTestInterfaceVector;

  static GarbageCollectionTestInterfaceVector& instances();

  GarbageCollectionTestInterface();
  ~GarbageCollectionTestInterface();

  // The current |previous| node will become the tail of its list.
  void set_previous(
      const scoped_refptr<GarbageCollectionTestInterface>& previous);
  scoped_refptr<GarbageCollectionTestInterface> previous() {
    return make_scoped_refptr(previous_);
  }

  // The current |next| node will become the head of a new list.
  void set_next(const scoped_refptr<GarbageCollectionTestInterface>& next);
  scoped_refptr<GarbageCollectionTestInterface> next() { return next_; }

  DEFINE_WRAPPABLE_TYPE(GarbageCollectionTestInterface);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  static void Join(GarbageCollectionTestInterface* first,
                   GarbageCollectionTestInterface* second);

  void MakeHead();
  void MakeTail();

  // Raw pointers going upstream, strong pointers going downstream to prevent
  // reference cycles.
  GarbageCollectionTestInterface* previous_;
  scoped_refptr<GarbageCollectionTestInterface> next_;
};

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

#endif  // COBALT_BINDINGS_TESTING_GARBAGE_COLLECTION_TEST_INTERFACE_H_
