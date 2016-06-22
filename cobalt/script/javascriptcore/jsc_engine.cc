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
#include "cobalt/base/c_val.h"
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
  base::DebugCVal<size_t> js_memory_;
  base::DebugCVal<size_t> js_engine_count_;
  scoped_ptr<base::PollerWithThread> poller_;
};

JSCEngineStats::JSCEngineStats()
    : js_memory_("Memory.JS", 0,
                 "Total memory occupied by the JSC page allocator."),
      js_engine_count_("JS.EngineCount", 0,
                       "Total JavaScript engine registered.") {
  poller_.reset(new base::PollerWithThread(
      base::Bind(&JSCEngineStats::Update, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(20)));
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
  DCHECK(thread_checker_.CalledOnValidThread());
#if !defined(__LB_SHELL__FOR_RELEASE__)
  JSCEngineStats::GetInstance()->JSCEngineDestroyed();
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)
  script_object_registry_.ClearEntries();
}

scoped_refptr<GlobalObjectProxy> JSCEngine::CreateGlobalObjectProxy() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return new JSCGlobalObjectProxy(this);
}

void JSCEngine::CollectGarbage() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSC::JSLockHolder lock(global_data_.get());
  DCHECK(global_data_->heap.isSafeToCollect());
  global_data_->heap.collectAllGarbage();
}

void JSCEngine::ReportExtraMemoryCost(size_t bytes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSC::JSLockHolder lock(global_data_.get());
  global_data_->heap.reportExtraMemoryCost(bytes);
}

}  // namespace javascriptcore

scoped_ptr<JavaScriptEngine> JavaScriptEngine::CreateEngine() {
  // Documentation in InitializeThreading.h states this function should be
  // called from the main thread. However, calling it here from the thread each
  // engine is created on appears to show no difference in practice.
  JSC::initializeThreading();

  // There must be at most one JSGlobalData instance created per thread.
  scoped_ptr<javascriptcore::JSCEngine> engine(new javascriptcore::JSCEngine());

  // Global data should be of Default type - one per thread.
  DCHECK(!engine->global_data()->usingAPI());

  return engine.PassAs<JavaScriptEngine>();
}

}  // namespace script
}  // namespace cobalt
