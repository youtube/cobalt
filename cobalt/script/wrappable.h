// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_WRAPPABLE_H_
#define COBALT_SCRIPT_WRAPPABLE_H_

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/source_location.h"
#include "cobalt/base/type_id.h"

namespace cobalt {
namespace script {

class Wrappable;
class Tracer {
 public:
  virtual void Trace(Wrappable* wrappable) = 0;
};

class Wrappable : public base::RefCounted<Wrappable> {
 public:
  // A handle to this Wrappable's corresponding Wrapper object. It may be
  // NULL if no wrapper has been created. A Wrapper may get garbage collected
  // independent of the lifetime of its corresponding Wrappable object.
  class WeakWrapperHandle {
   protected:
    WeakWrapperHandle() {}
    virtual ~WeakWrapperHandle() {}

   private:
    friend class scoped_ptr<WeakWrapperHandle>;
  };

  // A class that creates ValueHandles should inherit from this interface
  // so that it can get/set the cached wrapper handle.
  class CachedWrapperAccessor {
   protected:
    WeakWrapperHandle* GetCachedWrapper(Wrappable* wrappable) const {
      return wrappable->cached_wrapper_.get();
    }
    void SetCachedWrapper(Wrappable* wrappable,
                          scoped_ptr<WeakWrapperHandle> wrapper) const {
      wrappable->cached_wrapper_ = wrapper.Pass();
    }
  };

  // Used for RTTI on wrappable types. This is implemented within the
  // DEFINE_WRAPPABLE_TYPE macro, defined below, which should be added to the
  // class definition of each wrappable type.
  virtual base::TypeId GetWrappableType() const = 0;

  virtual base::SourceLocation GetInlineSourceLocation() const = 0;

  // If this function returns true, the JavaScript engine will keep the wrapper
  // for this Wrappable from being garbage collected even if there are no strong
  // references to it. If this Wrappable is no longer referenced from anything
  // other than the wrapper, the wrappable will be garbage collected despite
  // this (which will result in the Wrappable being destructed as well.)
  virtual bool ShouldKeepWrapperAlive() { return false; }

  // Trace all native |Wrappable|s accessible by the |Wrappable|. Must be
  // manually implemented by the |Wrappable|.
  // TODO: Should be pure virtual after static analysis tool for |Wrappable|s
  // is created.
  virtual void TraceMembers(Tracer* /*tracer*/) {}

 protected:
  virtual ~Wrappable() { }

 private:
  // A cached weak reference to the interface's corresponding wrapper object.
  // This may get garbage-collected before the associated Wrappable is
  // destroyed.
  scoped_ptr<WeakWrapperHandle> cached_wrapper_;

  friend class base::RefCounted<Wrappable>;
};

}  // namespace script
}  // namespace cobalt

// This macro should be added with public accessibility in a class that inherits
// from Wrappable. It is used to implement RTTI that is used by the bindings
// layer to downcast a wrappable class if necessary before creating the JS
// wrapper.
//
// Using the interface name as the template parameter for the GetTypeId()
// function allows us to ensure at compile time that the static method is
// defined for a given type, and not just on one of its ancestors.
//
// It is also used to implement GetSourceLocationName() and
// GetInlineSourceLocation() that are used to generate source locations
// returned in messages from the parsers.
#define DEFINE_WRAPPABLE_TYPE(INTERFACE_NAME)                     \
  static base::TypeId INTERFACE_NAME##WrappableType() {           \
    return base::GetTypeId<INTERFACE_NAME>();                     \
  }                                                               \
  base::TypeId GetWrappableType() const override {                \
    return INTERFACE_NAME##WrappableType();                       \
  }                                                               \
  static const char* GetSourceLocationName() {                    \
    return "[object " #INTERFACE_NAME "]";                        \
  }                                                               \
  base::SourceLocation GetInlineSourceLocation() const override { \
    return base::SourceLocation(GetSourceLocationName(), 1, 1);   \
  }

#endif  // COBALT_SCRIPT_WRAPPABLE_H_
