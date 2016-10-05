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
#include "cobalt/script/javascriptcore/jsc_global_environment.h"
#include "cobalt/script/javascriptcore/jsc_global_object.h"
#include "third_party/WebKit/Source/JavaScriptCore/runtime/InitializeThreading.h"
#include "third_party/WebKit/Source/WTF/wtf/OSAllocator.h"

namespace cobalt {
namespace script {
namespace javascriptcore {

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

  size_t UpdateMemoryStatsAndReturnReserved() {
    base::AutoLock auto_lock(lock_);
    if (js_engine_count_.value() == 0) {
      return 0;
    }
    return OSAllocator::getCurrentBytesAllocated();
  }

 private:
  friend struct StaticMemorySingletonTraits<JSCEngineStats>;

  base::Lock lock_;
  base::CVal<size_t> js_engine_count_;
};

JSCEngineStats::JSCEngineStats()
    : js_engine_count_("Count.JS.Engine", 0,
                       "Total JavaScript engine registered.") {}

}  // namespace

JSCEngine::JSCEngine() {
  global_data_ = JSC::JSGlobalData::create(JSC::LargeHeap);
  JSCEngineStats::GetInstance()->JSCEngineCreated();
}

JSCEngine::~JSCEngine() {
  DCHECK(thread_checker_.CalledOnValidThread());
  JSCEngineStats::GetInstance()->JSCEngineDestroyed();
  script_object_registry_.ClearEntries();
}

scoped_refptr<GlobalEnvironment> JSCEngine::CreateGlobalEnvironment() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return new JSCGlobalEnvironment(this);
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

size_t JSCEngine::UpdateMemoryStatsAndReturnReserved() {
  return JSCEngineStats::GetInstance()->UpdateMemoryStatsAndReturnReserved();
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
