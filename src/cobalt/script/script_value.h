/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_SCRIPT_VALUE_H_
#define COBALT_SCRIPT_SCRIPT_VALUE_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

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
class ScriptValue {
 public:
  // The Reference class maintains the ownership relationship between a
  // Wrappable and the JavaScript value wrapped by a ScriptValue. This is an
  // RAII object in that creation of a Reference instance will mark the
  // underlying value as owned by this wrappable, and the underlying object will
  // be unmarked when this Reference is destructed.  The lifetime of a Reference
  // must be at least as long as the Wrappable that has been passed into the
  // constructor.
  class Reference {
   public:
    Reference(Wrappable* wrappable, const ScriptValue& script_value)
        : owner_(wrappable), referenced_value_(script_value.MakeCopy()) {
      DCHECK(referenced_value_);
      referenced_value_->RegisterOwner(owner_);
    }

    const T& value() const { return *(referenced_value_->GetScriptValue()); }

    // Return the referenced ScriptValue. This ScriptValue can
    // be passed back into the JavaScript bindings layer where the referenced
    // JavaScript object can be extracted from the ScriptValue.
    const ScriptValue<T>& referenced_value() const {
      return *(referenced_value_.get());
    }

    ~Reference() { referenced_value_->DeregisterOwner(owner_); }

   private:
    Wrappable* const owner_;
    scoped_ptr<ScriptValue> referenced_value_;

    DISALLOW_COPY_AND_ASSIGN(Reference);
  };

  // Return true iff |other| refers to the same underlying JavaScript object.
  virtual bool EqualTo(const ScriptValue& other) const = 0;

  // Returns true if this ScriptValue is referring to a NULL JavaScript object.
  bool IsNull() const { return GetScriptValue() == NULL; }

  // Creates a new ScriptValue that contains a weak reference to the same
  // underlying JavaScript object. Note that this will not prevent the object
  // from being garbage collected, one must create a Reference to do that.
  scoped_ptr<ScriptValue> MakeWeakCopy() const {
    return MakeCopy().Pass();
  }

 protected:
  virtual ~ScriptValue() {}

 private:
  // Mark/unmark this Wrappable as owning a handle to the underlying JavaScript
  // object.
  virtual void RegisterOwner(Wrappable* owner) = 0;
  virtual void DeregisterOwner(Wrappable* owner) = 0;

  // Return a pointer to the object that wraps the underlying JavaScript object.
  virtual const T* GetScriptValue() const = 0;

  // Make a new ScriptValue instance that holds a handle to the same underlying
  // JavaScript object. This should not be called for a ScriptValue that has a
  // NULL script object (that is, GetScriptValue() returns NULL).
  virtual scoped_ptr<ScriptValue> MakeCopy() const = 0;

  friend class scoped_ptr<ScriptValue>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_VALUE_H_
