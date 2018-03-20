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

#ifndef COBALT_SCRIPT_SCRIPT_VALUE_H_
#define COBALT_SCRIPT_SCRIPT_VALUE_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace script {

class Wrappable;

// ScriptValue is a wrapper around raw JavaScript values that are being passed
// into Cobalt. These are values that do not correspond to Cobalt Wrappable
// objects. Specifically, this could include objects that implement the
// EventListener interface, callback functions, promises, or any Javascript
// value at all.  As long as Cobalt maintains a handle to such an value, it
// should not be garbage collected, if it is a garbage-collectible thing (GC
// thing). Web API implementations should hold on to a reference to a
// ScriptValue implementation by constructing a ScriptValue::Reference object.
//
// The Reference class takes as a constructor parameter a pointer to the
// Wrappable that is holding onto the ScriptValue. This ensures that the
// JavaScript engine's garbage collection is aware of the relationship between
// the ScriptValue and the Wrappable's corresponding JavaScript wrapper. This
// will ensure that the garbage collector can detect when these values are
// detached from the rest of the graph of JavaScript GC things, and can safely
// be garbage collected.
template <class T>
class ScriptValue : public Traceable {
 public:
  // A reference type for when a |Traceable| owns a |ScriptValue|, and only
  // wants to keep it alive if the owning object continues to get traced. This
  // is the preferred reference type in that if you can use it, you should (as
  // opposed to legacy types |Reference| or |StrongReference|, which predate
  // the TraceMembers system).
  class TracedReference : public Traceable {
   public:
    explicit TracedReference(scoped_ptr<ScriptValue> script_value)
        : referenced_value_(script_value.Pass()) {
      DCHECK(referenced_value_);
    }

    explicit TracedReference(const ScriptValue& script_value)
        : referenced_value_(script_value.MakeCopy()) {
      DCHECK(referenced_value_);
    }

    const T& value() const { return *(referenced_value_->GetScriptValue()); }

    // Return the referenced ScriptValue. This ScriptValue can be passed back
    // into the JavaScript bindings layer where the referenced JavaScript
    // value can be extracted from the ScriptValue.
    const ScriptValue<T>& referenced_value() const {
      return *(referenced_value_.get());
    }

    void TraceMembers(Tracer* tracer) override {
      tracer->Trace(referenced_value_.get());
    }

   private:
    scoped_ptr<ScriptValue> referenced_value_;

    DISALLOW_COPY_AND_ASSIGN(TracedReference);
  };

  // The Reference class maintains the ownership relationship between a
  // Wrappable and the JavaScript value wrapped by a ScriptValue. This is an
  // RAII object in that creation of a Reference instance will mark the
  // underlying value as owned by this wrappable, and the underlying value
  // will be unmarked when this Reference is destructed.  The lifetime of a
  // Reference must be at least as long as the Wrappable that has been passed
  // into the constructor.
  class Reference {
   public:
    Reference(Wrappable* wrappable, scoped_ptr<ScriptValue> script_value)
        : owner_(wrappable), referenced_value_(script_value.Pass()) {
      DCHECK(referenced_value_);
      referenced_value_->RegisterOwner(owner_);
    }

    Reference(Wrappable* wrappable, const ScriptValue& script_value)
        : owner_(wrappable), referenced_value_(script_value.MakeCopy()) {
      DCHECK(referenced_value_);
      referenced_value_->RegisterOwner(owner_);
    }

    const T& value() const { return *(referenced_value_->GetScriptValue()); }

    // Return the referenced ScriptValue. This ScriptValue can be passed back
    // into the JavaScript bindings layer where the referenced JavaScript
    // value can be extracted from the ScriptValue.
    const ScriptValue<T>& referenced_value() const {
      return *(referenced_value_.get());
    }

    ~Reference() { referenced_value_->DeregisterOwner(owner_); }

   private:
    Wrappable* const owner_;
    scoped_ptr<ScriptValue> referenced_value_;

    DISALLOW_COPY_AND_ASSIGN(Reference);
  };

  // Prevent garbage collection of the ScriptValue. This should be used with
  // care as it can result in resource leaks if not managed appropriately. A
  // common use case is to create a StrongReference on the stack when a
  // ScriptValue is passed into a function, but a reference to the ScriptValue
  // doesn't need to be retained past the scope of the function.
  class StrongReference {
   public:
    explicit StrongReference(scoped_ptr<ScriptValue> script_value)
        : referenced_value_(script_value.Pass()) {
      DCHECK(referenced_value_);
      referenced_value_->PreventGarbageCollection();
    }

    explicit StrongReference(const ScriptValue& script_value)
        : referenced_value_(script_value.MakeCopy()) {
      DCHECK(referenced_value_);
      referenced_value_->PreventGarbageCollection();
    }

    const T& value() const { return *(referenced_value_->GetScriptValue()); }

    // Return the referenced ScriptValue. This ScriptValue can be passed back
    // into the JavaScript bindings layer where the referenced JavaScript
    // value can be extracted from the ScriptValue.
    const ScriptValue<T>& referenced_value() const {
      return *(referenced_value_.get());
    }

    ~StrongReference() { referenced_value_->AllowGarbageCollection(); }

   private:
    scoped_ptr<ScriptValue> referenced_value_;

    DISALLOW_COPY_AND_ASSIGN(StrongReference);
  };

  // Return true if and only if |other| refers to the same underlying
  // JavaScript value.
  virtual bool EqualTo(const ScriptValue& other) const = 0;

  // Returns true if this ScriptValue is referring to a NULL JavaScript value.
  bool IsNull() const { return GetScriptValue() == NULL; }

  // Creates a new ScriptValue that contains a weak reference to the same
  // underlying JavaScript value. Note that this will not prevent the value
  // from being garbage collected, one must create a Reference to do that.
  scoped_ptr<ScriptValue> MakeWeakCopy() const {
    return MakeCopy().Pass();
  }

 protected:
  virtual ~ScriptValue() {}

 private:
  // Register this Wrappable as owning a handle to the underlying JavaScript
  // value.
  virtual void RegisterOwner(Wrappable* owner) = 0;
  virtual void DeregisterOwner(Wrappable* owner) = 0;

  // Prevent/Allow garbage collection of the underlying ScriptValue. Calls
  // must be balanced and are not idempodent. While the number of calls to
  // |Prevent| are greater than the number of calls to |Allow|, the underlying
  // value will never be garbage collected.
  virtual void PreventGarbageCollection() = 0;
  virtual void AllowGarbageCollection() = 0;

  // Return a pointer to the value that wraps the underlying JavaScript
  // value.
  virtual const T* GetScriptValue() const = 0;

  // Make a new ScriptValue instance that holds a handle to the same
  // underlying JavaScript value. This should not be called for a ScriptValue
  // that has a NULL script value (that is, GetScriptValue() returns NULL).
  virtual scoped_ptr<ScriptValue> MakeCopy() const = 0;

  friend class scoped_ptr<ScriptValue>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_VALUE_H_
