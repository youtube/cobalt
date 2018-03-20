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

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/script/tracer.h"

namespace cobalt {
namespace script {

template <typename T>
class Handle;

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
class ScriptValue {
 public:
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

    Reference(Wrappable* wrappable, const Handle<T>& local)
        : owner_(wrappable),
          referenced_value_(local.GetScriptValue()->MakeCopy()) {
      DCHECK(referenced_value_);
      referenced_value_->RegisterOwner(owner_);
    }

    const T& value() const { return *(referenced_value_->GetValue()); }

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

  // Return true if and only if |other| refers to the same underlying
  // JavaScript value.
  virtual bool EqualTo(const ScriptValue& other) const = 0;

  // Returns true if this ScriptValue is referring to a NULL JavaScript value.
  bool IsNull() const { return GetValue() == NULL; }

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
  virtual T* GetValue() = 0;
  virtual const T* GetValue() const = 0;

  // Make a new ScriptValue instance that holds a handle to the same
  // underlying JavaScript value. This should not be called for a ScriptValue
  // that has a NULL script value (that is, GetScriptValue() returns NULL).
  virtual scoped_ptr<ScriptValue> MakeCopy() const = 0;

  int reference_count_ = 0;

  friend class scoped_ptr<ScriptValue>;

  template <typename F>
  friend class Handle;
};

// A handle that references a |ScriptValue|, and manages its garbage
// collection state via reference counting.  This is the preferred type for
// receiving, returning, and manipulating |ScriptValue|s.
template <typename T>
class Handle {
 public:
  // This should only be used by internals, next to where engine specific
  // |ScriptValue| implementations are constructed.  Calling this from common
  // code is a usage error.  If you want a new |Local<T>|, then you should
  // build it from an existing reference, rather than working directly with
  // the |ScriptValue|.
  explicit Handle(ScriptValue<T>* script_value) : script_value_(script_value) {
    DCHECK(script_value_);
    DCHECK_EQ(script_value_->reference_count_, 0);
    script_value_->PreventGarbageCollection();
    script_value_->reference_count_++;
  }
  // Intentionally not explicit because bindings' usage of ToJSValue relies on
  // implicit conversion.
  Handle(const ScriptValue<T>* script_value)  // NOLINT
      : Handle(script_value->MakeWeakCopy().release()) {}
  Handle(const ScriptValue<T>& script_value)  // NOLINT
      : Handle(script_value.MakeWeakCopy().release()) {}
  Handle(const typename ScriptValue<T>::Reference& reference)  // NOLINT
      : Handle(reference.referenced_value().MakeWeakCopy().release()) {}

  Handle(const Handle& other) : script_value_(other.script_value_) {
    script_value_->PreventGarbageCollection();
    script_value_->reference_count_++;
  }
  Handle& operator=(const Handle& other) {
    // Increment |other|'s value first to allow for self assignment.
    if (other.script_value_) {
      other.script_value_->PreventGarbageCollection();
      other.script_value_->reference_count_++;
    }
    Clear();
    script_value_ = other.script_value_;
    return *this;
  }

  Handle(Handle&& other) : script_value_(other.script_value_) {
    other.script_value_ = nullptr;
  }
  Handle& operator=(Handle&& other) {
    std::swap(script_value_, other.script_value_);
    return *this;
  }

  ~Handle() { Clear(); }

  T* operator*() { return script_value_->GetValue(); }
  T* operator->() { return script_value_->GetValue(); }

  // These are primarly exposed for internals.  In most cases you don't need
  // to work with the ScriptValue directly.
  ScriptValue<T>* GetScriptValue() { return script_value_; }
  const ScriptValue<T>* GetScriptValue() const { return script_value_; }

  bool IsEmpty() const { return script_value_ == nullptr; }

 private:
  ScriptValue<T>* script_value_;

  void Clear() {
    if (script_value_) {
      script_value_->reference_count_--;
      script_value_->AllowGarbageCollection();
      if (script_value_->reference_count_ == 0) {
        delete script_value_;
      }
    }
    script_value_ = nullptr;
  }
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_VALUE_H_
