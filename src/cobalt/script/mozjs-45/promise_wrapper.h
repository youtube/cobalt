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

#ifndef COBALT_SCRIPT_MOZJS_45_PROMISE_WRAPPER_H_
#define COBALT_SCRIPT_MOZJS_45_PROMISE_WRAPPER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/script/mozjs-45/weak_heap_object.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Native class that maintains a weak handle to a JS object that is a proxy to
// a JS Promise object. The wrapper object maintains references to the reject
// and resolve functions that are passed to the Promise executor function.
class PromiseWrapper {
 public:
  // Creates a new JS object that wraps a new Promise, created using the
  // Promise constructor on |global_object|. Returns NULL on failure.
  static JSObject* Create(JSContext* context, JS::HandleObject global_object);

  PromiseWrapper(JSContext* context, JS::HandleValue promise_wrapper);

  const WeakHeapObject& get() const { return weak_promise_wrapper_; }
  JSObject* GetPromise() const;
  void Resolve(JS::HandleValue value) const;
  void Reject(JS::HandleValue value) const;

 private:
  JSContext* context_;
  WeakHeapObject weak_promise_wrapper_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_PROMISE_WRAPPER_H_
