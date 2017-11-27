// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_MOZJS_45_WRAPPER_PRIVATE_H_
#define COBALT_SCRIPT_MOZJS_45_WRAPPER_PRIVATE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/mozjs-45/wrapper_factory.h"
#include "cobalt/script/wrappable.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Our mozjs specific implementation of |script::Tracer|. Tracing sessions
// will be initiated from a |Wrapper| of SpiderMonkey's choice, and then it
// will be the |mozjs::Tracer|'s job to assist SpiderMonkey's garbage
// collector in traversing the graph of |Wrapper|s and |Wrappable|s.
// |Wrappable|s will inform us about what they can reach through their
// |TraceMembers| implementation, and then we will pass the reachable
// |Wrappable|'s |Wrapper| to SpiderMonkey GC if it exists, and otherwise
// continue traversing ourselves from |Wrappable| to (the unwrapped)
// |Wrappable|.
class Tracer : public ::cobalt::script::Tracer {
 public:
  explicit Tracer(JSTracer* js_tracer) : js_tracer_(js_tracer) {}
  void Trace(Wrappable* wrappable) override;

  void TraceFrom(Wrappable* wrappable);

 private:
  JSTracer* js_tracer_;
  // Pending |Wrappable|s that we must traverse ourselves, since they did not
  // have a |Wrapper|.
  std::vector<Wrappable*> frontier_;
};

// Contains private data associated with a JSObject representing a JS wrapper
// for a Cobalt platform object. There should be a one-to-one mapping of such
// JSObjects and WrapperPrivate instances, and the corresponding WrapperPrivate
// must be destroyed when its JSObject is garbage collected.
class WrapperPrivate : public base::SupportsWeakPtr<WrapperPrivate> {
 public:
  typedef std::vector<Wrappable*> WrappableVector;

  template <typename T>
  scoped_refptr<T> wrappable() const {
    return base::polymorphic_downcast<T*>(wrappable_.get());
  }

  JSObject* js_object_proxy() const { return wrapper_proxy_; }

  // Create a new WrapperPrivate instance and associate it with the wrapper.
  static void AddPrivateData(JSContext* context, JS::HandleObject wrapper_proxy,
                             const scoped_refptr<Wrappable>& wrappable);

  // Return true if the object has wrapper private.
  static bool HasWrapperPrivate(JSContext* context, JS::HandleObject object);

  // Get the WrapperPrivate associated with the given Wrappable. A new JSObject
  // and WrapperPrivate object may be created.
  static WrapperPrivate* GetFromWrappable(
      const scoped_refptr<Wrappable>& wrappable, JSContext* context,
      WrapperFactory* wrapper_factory);

  // Get the |WrapperPrivate| instance associated with this |Wrapper| object.
  // Will return |NULL| if |object| does not have a |Wrappable|, which also
  // implies that it is not a |Wrapper|.
  static WrapperPrivate* GetFromWrapperObject(JS::HandleObject object);

  // Get the WrapperPrivate instance associated with the target of this proxy.
  static WrapperPrivate* GetFromProxyObject(JSContext* context,
                                            JS::HandleObject proxy_object);

  // Get the |WrapperPrivate| instance associated with the object, which may
  // be a proxy or a proxy target. Will return |NULL| if the |JSObject| that
  // results from traversing |object|'s proxy chain does not have a
  // |Wrappable|, which also implies that it is not a |Wrapper|.
  static WrapperPrivate* GetFromObject(JSContext* context,
                                       JS::HandleObject object);

  // Called when the wrapper object is about to be deleted by the GC.
  static void Finalizer(JSFreeOp* /* free_op */, JSObject* object);

  // Trace callback called during garbage collection.
  static void Trace(JSTracer* trace, JSObject* object);

 private:
  WrapperPrivate(JSContext* context, const scoped_refptr<Wrappable>& wrappable,
                 JS::HandleObject wrapper_proxy);
  ~WrapperPrivate();

  JSContext* context_;
  scoped_refptr<Wrappable> wrappable_;
  JS::Heap<JSObject*> wrapper_proxy_;

  friend Tracer;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_WRAPPER_PRIVATE_H_
