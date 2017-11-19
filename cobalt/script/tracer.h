// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_TRACER_H_
#define COBALT_SCRIPT_TRACER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/script/sequence.h"

namespace cobalt {
namespace script {

class Tracer;
class Traceable {
 public:
  // Trace all native |Traceable|s accessible by the |Traceable|.  Any class
  // that owns a |Traceable| must implement this interface and trace each of
  // its |Traceable| members.  Note that here, "owns" means is accessible from
  // JavaScript, which means that weak and raw pointers must be traced in
  // addition to |scoped_ptr|s and |scoped_refptr|s.  Consider, for example, a
  // list IDL interface, with parent and child references.  A C++
  // implementation backed by |scoped__refptr| will be forced to demote the
  // pointer from a child to a parent to a |base::WeakPtr| or raw pointer, in
  // order to break the reference cycle. In JavaScript however, holding a
  // reference to a child implies that the parent is still reachable, and
  // should not be garbage collected.
  virtual void TraceMembers(Tracer* tracer) = 0;

  // Whether the |Traceable| is a |Wrappable| or not.  This should never be
  // implemented by any class other than |Traceable| and |Wrappable|.
  virtual bool IsWrappable() const { return false; }
};

// Traverses |Traceable|s, while marking found JavaScript wrappers as
// reachable.
class Tracer {
 public:
  // Trace a |Traceable| by forwarding its wrapper (which is engine specific)
  // to the engine's internal tracer, or manually tracing reachable
  // |Traceable|s in the case where the wrapper did not exist.
  virtual void Trace(Traceable* traceable) = 0;

  void Trace(const Traceable& traceable) {
    Trace(const_cast<Traceable*>(&traceable));
  }

  // Trace the items of a container of |Traceable|s, such as |std::vector|.
  template <typename T>
  void TraceItems(const T& items) {
    for (const auto& item : items) {
      Trace(item);
    }
  }

  // Trace the value part of a container of key value pairs, where the values
  // are |Traceable|s, such as |std::map|.
  template <typename T>
  void TraceValues(const T& key_value_pairs) {
    for (const auto& pair : key_value_pairs) {
      Trace(pair.second);
    }
  }

  template <typename T>
  void TraceSequence(const Sequence<T>& sequence) {
    // TODO: Consider making |Sequence| more vector-like and just using the
    // iterator version.
    for (size_t i = 0; i < sequence.size(); i++) {
      Trace(sequence.at(i));
    }
  }

  // Note: If your new class wants to own a container of |Traceable|s that
  // does not have an iteration helper function here, you should add one.
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_TRACER_H_
