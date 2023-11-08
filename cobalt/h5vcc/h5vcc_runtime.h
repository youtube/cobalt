// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_H5VCC_RUNTIME_H_
#define COBALT_H5VCC_H5VCC_RUNTIME_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/deep_link_event.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/h5vcc/h5vcc_deep_link_event_target.h"
#include "cobalt/h5vcc/h5vcc_runtime_event_target.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccRuntime : public script::Wrappable {
 public:
  explicit H5vccRuntime(base::EventDispatcher* event_dispatcher);
  ~H5vccRuntime();

  std::string initial_deep_link();
  const scoped_refptr<H5vccDeepLinkEventTarget>& on_deep_link() const;
  const scoped_refptr<H5vccRuntimeEventTarget>& on_pause() const;
  const scoped_refptr<H5vccRuntimeEventTarget>& on_resume() const;

  DEFINE_WRAPPABLE_TYPE(H5vccRuntime);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  // Called by the event dispatcher to handle deep link events.
  void OnEventForDeepLink(const base::Event* event);
  void OnDeepLinkEvent(std::unique_ptr<base::DeepLinkEvent> event);

  // Returns the initial deep link if it's unconsumed.
  const std::string& GetUnconsumedDeepLink();

  scoped_refptr<H5vccDeepLinkEventTarget> on_deep_link_;
  scoped_refptr<H5vccRuntimeEventTarget> on_pause_;
  scoped_refptr<H5vccRuntimeEventTarget> on_resume_;

  // Non-owned reference used to receive application event callbacks.
  base::EventDispatcher* event_dispatcher_;
  std::string initial_deep_link_;
  const std::string empty_string_;

  // Event callbacks.
  base::EventCallback deep_link_event_callback_;
  base::OnceClosure consumed_callback_;

  // Track the task runner from where this object is created so deep link events
  // are handled from the same task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Thread checker ensures all calls to DOM element are made from the same
  // thread that it is created in.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(H5vccRuntime);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_RUNTIME_H_
