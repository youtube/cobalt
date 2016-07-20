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

#ifndef COBALT_SCRIPT_SCRIPT_OBJECT_H_
#define COBALT_SCRIPT_SCRIPT_OBJECT_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"

namespace cobalt {
namespace script {

class Wrappable;

// ScriptObject is a wrapper around JavaScript objects that are being passed
// into Cobalt. These are objects that do not correspond to cobalt Wrappable
// objects. Specifically, this could include objects that implement the
// EventListener interface, and callback functions.
// As long as Cobalt maintains a handle to such an object, it should not be
// garbage collected. Web API implementations should hold on to a reference to
// a ScriptObject implementation by constructing a ScriptObject::Reference
// object.
// The Reference class takes as a constructor parameter a pointer to the
// Wrappable that is holding onto the ScriptObject. This ensures that the
// JavaScript engine's garbage collection is aware of the relationship between
// the ScriptObject and the Wrappable's corresponding JavaScript wrapper. This
// will ensure that the garbage collector can detect when these objects are
// detached from the rest of the graph of JavaScript objects, and can safely
// be garbage collected.
template <class T>
class ScriptObject {
 public:
  // The Reference class maintains the ownership relationship between a
  // Wrappable and the JavaScript object wrapped by a ScriptObject. This is an
  // RAII object in that creation of a Reference instance will mark the
  // underlying object as owned by this wrappable, and the underlying object
  // will be unmarked when this Reference is destructed.
  // The lifetime of a Reference must be at least as long as the Wrappable that
  // has been passed into the constructor.
  class Reference {
   public:
    Reference(Wrappable* wrappable, const ScriptObject& script_object)
        : owner_(wrappable), referenced_object_(script_object.MakeCopy()) {
      DCHECK(referenced_object_);
      referenced_object_->RegisterOwner(owner_);
    }

    const T& value() const { return *(referenced_object_->GetScriptObject()); }

    // Return the referenced ScriptObject. This ScriptObject can
    // be passed back into the JavaScript bindings layer where the referenced
    // JavaScript object can be extracted from the ScriptObject.
    const ScriptObject<T>& referenced_object() const {
      return *(referenced_object_.get());
    }

    ~Reference() { referenced_object_->DeregisterOwner(owner_); }

   private:
    Wrappable* const owner_;
    scoped_ptr<ScriptObject> referenced_object_;

    DISALLOW_COPY_AND_ASSIGN(Reference);
  };

  // Return true iff |other| refers to the same underlying JavaScript object.
  virtual bool EqualTo(const ScriptObject& other) const = 0;

  // Returns true if this ScriptObject is referring to a NULL JavaScript object.
  bool IsNull() const { return GetScriptObject() == NULL; }

 protected:
  virtual ~ScriptObject() {}

 private:
  // Mark/unmark this Wrappable as owning a handle to the underlying JavaScript
  // object.
  virtual void RegisterOwner(Wrappable* owner) = 0;
  virtual void DeregisterOwner(Wrappable* owner) = 0;

  // Return a pointer to the object that wraps the underlying JavaScript object.
  virtual const T* GetScriptObject() const = 0;

  // Make a new ScriptObject instance that holds a handle to the same underlying
  // JavaScript object. This should not be called for a ScriptObject that has a
  // NULL script object (that is, GetScriptObject() returns NULL).
  virtual scoped_ptr<ScriptObject> MakeCopy() const = 0;

  friend class scoped_ptr<ScriptObject>;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_SCRIPT_OBJECT_H_
