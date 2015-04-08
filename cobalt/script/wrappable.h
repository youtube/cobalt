/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
#ifndef SCRIPT_WRAPPABLE_H_
#define SCRIPT_WRAPPABLE_H_

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/script/script_object_handle.h"
#include "cobalt/script/script_object_handle_creator.h"

namespace cobalt {
namespace script {

// Objects that can be bound to a JavaScript object must inherit from Wrappable.
// The Wrappable class holds a handle to this Wrappable objects corresponding
// JavaScript wrapper object, if such an object exists. This allows us to
// maintain a one-to-one mapping between implementation objects and wrapper
// objects.
class Wrappable : public base::RefCounted<Wrappable> {
 public:
  ScriptObjectHandle* GetOrCreateWrapper(
      ScriptObjectHandleCreator* handle_creator) {
    if (!wrapper_handle_.get() || !wrapper_handle_->IsValidHandle()) {
      scoped_ptr<ScriptObjectHandle> new_wrapper_handle =
          handle_creator->CreateHandle();
      DCHECK(new_wrapper_handle.get());
      DCHECK(!wrapper_handle_.get() || !wrapper_handle_->IsValidHandle());
      wrapper_handle_ = new_wrapper_handle.Pass();
    }
    return wrapper_handle_.get();
  }
  ScriptObjectHandle* get_wrapper_handle() const {
    return wrapper_handle_.get();
  }

 protected:
  virtual ~Wrappable() { }

 private:
  scoped_ptr<ScriptObjectHandle> wrapper_handle_;

  friend class base::RefCounted<Wrappable>;
};

}  // namespace script
}  // namespace cobalt

#endif  // SCRIPT_WRAPPABLE_H_
