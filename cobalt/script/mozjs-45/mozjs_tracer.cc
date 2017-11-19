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

#include "cobalt/script/mozjs-45/mozjs_tracer.h"

#include "base/hash_tables.h"
#include "cobalt/script/mozjs-45/mozjs_global_environment.h"
#include "cobalt/script/mozjs-45/wrapper_private.h"
#include "third_party/mozjs-45/js/public/Proxy.h"

namespace cobalt {
namespace script {
namespace mozjs {

void MozjsTracer::Trace(Traceable* traceable) {
  if (!traceable) {
    return;
  }

  // Unfortunately, |JSTracer| will only supply us with a |JSRuntime|,
  // rather than a |JSContext|. Fortunately, Cobalt will only create one
  // global environment per runtime, so we can still safely get back to our
  // context, and thus our global environment.
  JSContext* context = NULL;
  JS_ContextIterator(js_tracer_->runtime(), &context);
  DCHECK(context);
  MozjsGlobalEnvironment* global_environment =
      MozjsGlobalEnvironment::GetFromContext(context);
  DCHECK(global_environment);

  // Clearly, if we have already visited this wrappable during the current
  // tracing session, there is no need to visit it again. We rely on
  // |JS_SetGCCallback| in the |MozjsEngine| to properly manage clearing
  // |visited_wrappables_| in between GC sessions.
  base::hash_set<Traceable*>* visited_traceables =
      global_environment->visited_traceables();
  DCHECK(visited_traceables);
  if (!visited_traceables->insert(traceable).second) {
    return;
  }

  if (!traceable->IsWrappable()) {
    frontier_.push_back(traceable);
    return;
  }

  Wrappable* wrappable = base::polymorphic_downcast<Wrappable*>(traceable);

  // There are now two cases left to handle. Since we cannot create the
  // wrapper while tracing due to internal SpiderMonkey restrictions, we will
  // instead directly call |TraceMembers| here if the wrapper does not exist.
  // In the case where the wrapper already does exist, we will pass the
  // wrapper to |JS_CallObjectTracer|, and rely on SpiderMonkey to begin
  // another |WrapperPrivate::Trace| on that wrapper.
  WrapperFactory* wrapper_factory = global_environment->wrapper_factory();
  if (!wrapper_factory->HasWrapperProxy(wrappable)) {
    frontier_.push_back(wrappable);
  } else {
    JSObject* proxy_object = wrapper_factory->GetWrapperProxy(wrappable);
    JSObject* target = js::GetProxyTargetObject(proxy_object);
    WrapperPrivate* wrapper_private =
        static_cast<WrapperPrivate*>(JS_GetPrivate(target));
    DCHECK(wrapper_private->context_ == context);
    DCHECK(wrapper_private->wrapper_proxy_);
    JS_CallObjectTracer(js_tracer_, &wrapper_private->wrapper_proxy_,
                        "WrapperPrivate::TraceWrappable");
  }

  DCHECK(JS_ContextIterator(js_tracer_->runtime(), &context) == NULL);
}

void MozjsTracer::TraceFrom(Wrappable* wrappable) {
  DCHECK(frontier_.empty());
  frontier_.push_back(wrappable);
  while (!frontier_.empty()) {
    Traceable* traceable = frontier_.back();
    frontier_.pop_back();
    traceable->TraceMembers(this);
  }
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
