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
#include "cobalt/script/javascriptcore/jsc_engine.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "cobalt/base/console_values.h"
#include "cobalt/base/poller.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "cobalt/script/javascriptcore/jsc_global_object_proxy.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/InitializeThreading.h"
#include "third_party/WebKit/Source/WTF/wtf/OSAllocator.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

#if !defined(__LB_SHELL__FOR_RELEASE__)
namespace {

class JSCEngineStats {
 public:
  JSCEngineStats();

  static JSCEngineStats* GetInstance() {
    return Singleton<JSCEngineStats,
                     StaticMemorySingletonTraits<JSCEngineStats> >::get();
  }

  void JSCEngineCreated() {
    base::AutoLock auto_lock(lock_);
    ++js_engine_count_;
  }

  void JSCEngineDestroyed() {
    base::AutoLock auto_lock(lock_);
    --js_engine_count_;
  }

 private:
  friend struct StaticMemorySingletonTraits<JSCEngineStats>;

  void Update() {
    base::AutoLock auto_lock(lock_);
    if (js_engine_count_ > 0) {
      js_memory_ = OSAllocator::getCurrentBytesAllocated();
    }
  }

  base::Lock lock_;
  base::CVal<size_t> js_memory_;
  base::CVal<size_t> js_engine_count_;
  scoped_ptr<base::Poller> poller_;
};

JSCEngineStats::JSCEngineStats()
    : js_memory_("Memory.JS", 0,
                 "Total memory occupied by the JSC page allocator."),
      js_engine_count_("JS.EngineCount", 0,
                       "Total JavaScript engine registered.") {
  // Register to get polled if there is a message loop. Otherwise, do nothing.
  // (Some unit tests won't have a message loop).
  if (MessageLoop::current()) {
    poller_.reset(new base::Poller(
        base::Bind(&JSCEngineStats::Update, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(20)));
  }
}

}  // namespace
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)

JSCEngine::JSCEngine() {
  global_data_ = JSC::JSGlobalData::create(JSC::LargeHeap);
#if !defined(__LB_SHELL__FOR_RELEASE__)
  JSCEngineStats::GetInstance()->JSCEngineCreated();
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)
}

JSCEngine::~JSCEngine() {
#if !defined(__LB_SHELL__FOR_RELEASE__)
  JSCEngineStats::GetInstance()->JSCEngineDestroyed();
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)
}

scoped_refptr<GlobalObjectProxy> JSCEngine::CreateGlobalObjectProxy() {
  return new JSCGlobalObjectProxy(this);
}

void JSCEngine::CollectGarbage() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSC::JSLockHolder lock(global_data_.get());
  DCHECK(global_data_->heap.isSafeToCollect());
  global_data_->heap.collectAllGarbage();
}

void JSCEngine::ReportExtraMemoryCost(size_t bytes) {
  JSC::JSLockHolder lock(global_data_.get());
  global_data_->heap.reportExtraMemoryCost(bytes);
}

}  // namespace javascriptcore

scoped_ptr<JavaScriptEngine> JavaScriptEngine::CreateEngine() {
  // This must be called once on any thread that will run JavaScriptCore. There
  // must be at most one JSGlobalData instance created per thread.
  JSC::initializeThreading();
  scoped_ptr<JavaScriptEngine> engine(new javascriptcore::JSCEngine());
  return engine.Pass();
}

}  // namespace script
}  // namespace cobalt
