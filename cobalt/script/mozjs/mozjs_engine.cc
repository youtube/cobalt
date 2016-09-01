/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/script/mozjs/mozjs_engine.h"

#include <algorithm>

#include "base/logging.h"
#include "cobalt/script/mozjs/mozjs_global_object_proxy.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace {
// After this many bytes have been allocated, the garbage collector will run.
const uint32_t kGarbageCollectionThresholdBytes = 8 * 1024 * 1024;
}

MozjsEngine::MozjsEngine() {
  // TODO: Investigate the benefit of helper threads and things like
  // parallel compilation.
  runtime_ =
      JS_NewRuntime(kGarbageCollectionThresholdBytes, JS_NO_HELPER_THREADS);
  CHECK(runtime_);

  JS_SetRuntimePrivate(runtime_, this);

  // Use incremental garbage collection.
  JS_SetGCParameter(runtime_, JSGC_MODE, JSGC_MODE_INCREMENTAL);

  // Allow Spidermonkey to allocate as much memory as it needs. If this limit
  // is set we could limit the memory used by JS and "gracefully" restart,
  // for example.
  JS_SetGCParameter(runtime_, JSGC_MAX_BYTES, 0xffffffff);

  // Callback to be called whenever a JSContext is created or destroyed for this
  // JSRuntime.
  JS_SetContextCallback(runtime_, &MozjsEngine::ContextCallback);

  // Callback to be called during garbage collection during the sweep phase.
  JS_SetFinalizeCallback(runtime_, &MozjsEngine::FinalizeCallback);
}

MozjsEngine::~MozjsEngine() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JS_DestroyRuntime(runtime_);
}

scoped_refptr<GlobalObjectProxy> MozjsEngine::CreateGlobalObjectProxy() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return new MozjsGlobalObjectProxy(runtime_);
}

void MozjsEngine::CollectGarbage() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JS_GC(runtime_);
}

void MozjsEngine::ReportExtraMemoryCost(size_t bytes) { NOTIMPLEMENTED(); }

JSBool MozjsEngine::ContextCallback(JSContext* context, unsigned context_op) {
  JSRuntime* runtime = JS_GetRuntime(context);
  MozjsEngine* engine =
      static_cast<MozjsEngine*>(JS_GetRuntimePrivate(runtime));
  DCHECK(engine->thread_checker_.CalledOnValidThread());
  if (context_op == JSCONTEXT_NEW) {
    engine->contexts_.push_back(context);
  } else if (context_op == JSCONTEXT_DESTROY) {
    ContextVector::iterator it =
        std::find(engine->contexts_.begin(), engine->contexts_.end(), context);
    if (it != engine->contexts_.end()) {
      engine->contexts_.erase(it);
    }
  }
  return true;
}

void MozjsEngine::FinalizeCallback(JSFreeOp* free_op, JSFinalizeStatus status,
                                   JSBool is_compartment) {
  MozjsEngine* engine =
      static_cast<MozjsEngine*>(JS_GetRuntimePrivate(free_op->runtime()));
  DCHECK(engine->thread_checker_.CalledOnValidThread());
  if (status == JSFINALIZE_GROUP_START) {
    for (int i = 0; i < engine->contexts_.size(); ++i) {
      MozjsGlobalObjectProxy* global_environment =
          MozjsGlobalObjectProxy::GetFromContext(engine->contexts_[i]);
      global_environment->DoSweep();
    }
  }
}

}  // namespace mozjs

scoped_ptr<JavaScriptEngine> JavaScriptEngine::CreateEngine() {
  return make_scoped_ptr<JavaScriptEngine>(new mozjs::MozjsEngine());
}

}  // namespace script
}  // namespace cobalt
