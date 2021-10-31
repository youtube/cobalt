// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/source_location.h"
#include "cobalt/base/type_id.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace script {

class Wrappable : public base::RefCounted<Wrappable>, public Traceable {
 public:
  // A handle to this Wrappable's corresponding Wrapper object. It may be
  // NULL if no wrapper has been created. A Wrapper may get garbage collected
  // independent of the lifetime of its corresponding Wrappable object.
  class WeakWrapperHandle {
   protected:
    WeakWrapperHandle() {}
    virtual ~WeakWrapperHandle() {}

   private:
    friend std::unique_ptr<WeakWrapperHandle>::deleter_type;
  };

  // A class that creates ValueHandles should inherit from this interface
  // so that it can get/set the cached wrapper handle.
  class CachedWrapperAccessor {
   protected:
    WeakWrapperHandle* GetCachedWrapper(Wrappable* wrappable) const {
      return wrappable->cached_wrapper_.get();
    }
    void SetCachedWrapper(Wrappable* wrappable,
                          std::unique_ptr<WeakWrapperHandle> wrapper) const {
      wrappable->cached_wrapper_ = std::move(wrapper);
    }
  };

  enum class JSObjectType {
    kObject,
    kNode,
    kArray,
    kError,
    kBlob,
  };

  // Used for RTTI on wrappable types. This is implemented within the
  // DEFINE_WRAPPABLE_TYPE macro, defined below, which should be added to the
  // class definition of each wrappable type.
  virtual base::TypeId GetWrappableType() const = 0;

  virtual base::SourceLocation GetInlineSourceLocation() const = 0;

  // Our implementation of the |Traceable| interface.  All |Wrappable|s that
  // own any |Traceable|s must override |TraceMembers| and trace them.
  void TraceMembers(Tracer* tracer) override {}
  bool IsWrappable() const final { return true; }
  virtual JSObjectType GetJSObjectType() { return JSObjectType::kObject; }

 protected:
  virtual ~Wrappable() { }

 private:
  // A cached weak reference to the interface's corresponding wrapper object.
  // This may get garbage-collected before the associated Wrappable is
  // destroyed.
  std::unique_ptr<WeakWrapperHandle> cached_wrapper_;

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
