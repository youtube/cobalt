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

#ifndef COBALT_SCRIPT_V8C_WRAPPER_PRIVATE_H_
#define COBALT_SCRIPT_V8C_WRAPPER_PRIVATE_H_

#include "base/memory/weak_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

// Contains private data associated with a v8::Object representing a
// JavaScript wrapper for a Cobalt platform object. There should be a one-to-
// one mapping of such v8::Object and WrapperPrivate instances, and the
// corresponding |WrapperPrivate| must be destroyed when its |v8::Object| is
// garbage collected.
struct WrapperPrivate : public base::SupportsWeakPtr<WrapperPrivate> {
 public:
  // The callback that V8 will run when the |v8::Object| that we live inside
  // of dies.
  static void Callback(const v8::WeakCallbackInfo<WrapperPrivate>& data) {
    data.GetParameter()->wrapper_.Reset();
    delete data.GetParameter();
  }

  // Get the internal |WrapperPrivate*| data within |object|.  This function
  // should never be called on objects that don't have private data.
  static WrapperPrivate* GetFromWrapperObject(v8::Local<v8::Object> object) {
    DCHECK(object->InternalFieldCount() == kInternalFieldCount);
    return static_cast<WrapperPrivate*>(
        object->GetAlignedPointerFromInternalField(kInternalFieldDataIndex));
  }

  // Check whether |object| has private wrapper data.
  static bool HasWrapperPrivate(v8::Local<v8::Object> object) {
    return object->InternalFieldCount() == kInternalFieldCount;
  }

  static const int kInternalFieldCount = 2;

  WrapperPrivate() = delete;
  WrapperPrivate(v8::Isolate* isolate,
                 const scoped_refptr<Wrappable>& wrappable,
                 v8::Local<v8::Object> wrapper)
      : isolate_(isolate), wrappable_(wrappable), wrapper_(isolate, wrapper) {
    wrapper->SetAlignedPointerInInternalField(kInternalFieldDataIndex, this);
    wrapper->SetAlignedPointerInInternalField(kInternalFieldDummyIndex,
                                              nullptr);
    wrapper_.SetWeak(this, &WrapperPrivate::Callback,
                     v8::WeakCallbackType::kParameter);
  }

  // Mark |wrapper_| as reachable from other |Traceable|s.  This will be
  // called by |V8cHeapTracer| during tracing.
  void Mark() { wrapper_.RegisterExternalReference(isolate_); }

  template <typename T>
  scoped_refptr<T> wrappable() const {
    return base::polymorphic_downcast<T*>(wrappable_.get());
  }

  Wrappable* raw_wrappable() const { return wrappable_.get(); }

  v8::Local<v8::Object> wrapper() const { return wrapper_.Get(isolate_); }

 private:
  // For the time being, we only use a single internal field, which stores a
  // pointer back to us (us being the |WrapperPrivate|).
  static const int kInternalFieldDataIndex = 0;
  // Blink uses two fields, so V8 won't believe we're a potential wrapper
  // unless we have two fields.
  //      .-""""""-.
  //    .'          '.
  //   /   O      O   \
  //  :                :
  //  |                |
  //  :    .------.    :
  //   \  '        '  /
  //    '.          .'
  //      '-......-'
  static const int kInternalFieldDummyIndex = 1;

  v8::Isolate* isolate_;
  scoped_refptr<Wrappable> wrappable_;
  v8::Global<v8::Object> wrapper_;

  DISALLOW_COPY_AND_ASSIGN(WrapperPrivate);
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_WRAPPER_PRIVATE_H_
