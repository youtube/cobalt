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

#include "base/bind.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/script/script_object_handle.h"
#include "cobalt/script/script_object_handle_creator.h"

namespace cobalt {
namespace script {

class Wrappable : public base::RefCounted<Wrappable> {
 public:
  class Type {
   public:
    explicit Type(intptr_t value) : value_(value) {}
    bool operator==(const Type& other) const {
      return this->value_ == other.value_;
    }

#if defined(__LB_LINUX__)
    struct hash_function {
      std::size_t operator()(const Wrappable::Type& wrappable_type) const {
        return BASE_HASH_NAMESPACE::hash<intptr_t>()(wrappable_type.value_);
      }
    };
#else
    struct hash_compare : public BASE_HASH_NAMESPACE::hash_compare<intptr_t> {
      typedef BASE_HASH_NAMESPACE::hash_compare<intptr_t> Base;
      size_t operator()(const Wrappable::Type& key) const {
        return static_cast<size_t>(key.value_);
      }
      bool operator()(const Wrappable::Type& key1,
                      const Wrappable::Type& key2) const {
        return Base::operator()(key1.value_, key2.value_);
      }
    };
#endif

   private:
    intptr_t value_;
  };

  typedef base::Callback<scoped_ptr<ScriptObjectHandle>(
      const scoped_refptr<Wrappable>&)> CreateWrapperFunction;

  ScriptObjectHandle* GetWrapperHandle(
      const CreateWrapperFunction& create_wrapper_function) {
    if (!wrapper_handle_ || !wrapper_handle_->IsValidHandle()) {
      wrapper_handle_ = create_wrapper_function.Run(this);
    }
    DCHECK(wrapper_handle_.get());
    DCHECK(wrapper_handle_->IsValidHandle());
    return wrapper_handle_.get();
  }

  // Used for RTTI on wrappable types. This is implemented within the
  // DEFINE_WRAPPABLE_TYPE macro, defined below, which should be added to the
  // class definition of each wrappable type.
  virtual Type GetWrappableType() = 0;

 protected:
  virtual ~Wrappable() { }

 private:
  scoped_ptr<ScriptObjectHandle> wrapper_handle_;

  friend class base::RefCounted<Wrappable>;
};

}  // namespace script
}  // namespace cobalt

// This macro should be added with public accessibility in a class that inherits
// from Wrappable. It is used to implement RTTI that will be used by the
// bindings layer to downcast a wrappable class if necessary before creating
// the JS wrapper.
//
// The address of the static variable 'dummy' will be unique for each definition
// of the WrappableClass::WrappableType() function, which means that it can be
// used to uniquely identify a class.
// Using the interface name as a part of the static function allows us to ensure
// at compile time that the static method is defined for a given type, and not
// just on one of its ancestors.
#define DEFINE_WRAPPABLE_TYPE(INTERFACE_NAME)                   \
  static Wrappable::Type INTERFACE_NAME##WrappableType() {      \
    static int dummy = 0;                                       \
    return Wrappable::Type(reinterpret_cast<intptr_t>(&dummy)); \
  }                                                             \
  Wrappable::Type GetWrappableType() OVERRIDE {                 \
    return INTERFACE_NAME##WrappableType();                     \
  }


#endif  // SCRIPT_WRAPPABLE_H_
