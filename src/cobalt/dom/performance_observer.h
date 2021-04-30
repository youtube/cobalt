// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_PERFORMANCE_OBSERVER_H_
#define COBALT_DOM_PERFORMANCE_OBSERVER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "cobalt/dom/performance_observer_entry_list.h"
#include "cobalt/dom/performance_observer_init.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Performance;
class PerformanceObserver;

// Implements the PerformanceObserver IDL interface, as described here:
//   https://www.w3.org/TR/performance-timeline-2/#the-performanceobserver-interface
class PerformanceObserver : public script::Wrappable {
 public:
  // https://www.w3.org/TR/2019/WD-performance-timeline-2-20191024/#dom-performanceobservercallback
  typedef script::CallbackFunction<void(
      const scoped_refptr<PerformanceObserverEntryList>& entries,
      const scoped_refptr<PerformanceObserver>& observer)> PerformanceObserverCallback;
  typedef script::ScriptValue<PerformanceObserverCallback> PerformanceObserverCallbackArg;
  typedef PerformanceObserverCallbackArg::Reference PerformanceObserverCallbackReference;

  typedef base::Callback<void(
      const scoped_refptr<PerformanceObserverEntryList>& entries,
      const scoped_refptr<PerformanceObserver>& observer)>
      NativePerformanceObserverCallback;

  // Not part of the spec. Support creating PerformanceObserver from native
  // Cobalt code.
  PerformanceObserver(const NativePerformanceObserverCallback& native_callback,
                      const scoped_refptr<Performance>& performance);

  // Web Api: PerformanceObserver
  PerformanceObserver(script::EnvironmentSettings* env_settings,
                      const PerformanceObserverCallbackArg& callback);

  ~PerformanceObserver();

  // Wep Apis.
  void Observe(const PerformanceObserverInit& options,
               script::ExceptionState* exception_state);
  void Observe(script::ExceptionState* exception_state);
  void Disconnect();
  PerformanceEntryList TakeRecords();
  static script::Sequence<std::string> supported_entry_types();

  void EnqueuePerformanceEntry(const scoped_refptr<PerformanceEntry>& entry);

  // Internal helper class to allow creation of a PerformanceObserver with either a
  // native or script callback. Must be public so it can be inherited from in
  // the .cc file.
  class CallbackInternal {
   public:
    virtual bool RunCallback(const scoped_refptr<PerformanceObserverEntryList>& entries,
                             const scoped_refptr<PerformanceObserver>& observer) = 0;
    virtual ~CallbackInternal() {}
  };

  void EmptyObserverBuffer() { observer_buffer_.clear();}
  PerformanceEntryList GetObserverBuffer() { return observer_buffer_; }
  std::unique_ptr<CallbackInternal>& GetPerformanceObserverCallback() {
    return callback_;
  }

  DEFINE_WRAPPABLE_TYPE(PerformanceObserver);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  friend class Performance;
  friend class MockPerformanceObserver;

  enum class PerformanceObserverType {
    kMultiple,
    kSingle,
    kUndefined,
  };

  std::unique_ptr<CallbackInternal> callback_;
  base::WeakPtr<Performance> performance_;
  PerformanceEntryList observer_buffer_;
  PerformanceObserverType observer_type_;
  bool is_registered_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceObserver);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PERFORMANCE_OBSERVER_H_
