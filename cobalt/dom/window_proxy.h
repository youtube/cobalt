// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_WINDOW_PROXY_H_
#define COBALT_DOM_WINDOW_PROXY_H_


#include "cobalt/dom/window.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/web/event.h"
#include "cobalt/web/event_target.h"

namespace cobalt {
namespace dom {

class WindowProxy : public web::EventTarget {
 public:
  WindowProxy(script::EnvironmentSettings* settings, Window* window,
              Window* parent)
      : web::EventTarget(settings), window_(window), parent_(parent) {}
  ~WindowProxy() = default;

  bool DispatchEvent(const scoped_refptr<web::Event>& event) override {
    window_->environment_settings()
        ->context()
        ->message_loop()
        ->task_runner()
        ->PostBlockingTask(FROM_HERE,
                           base::Bind(&WindowProxy::DispatchEventInWindowThread,
                                      base::Unretained(this), event));
    return true;
  }

  Window* parent() { return parent_; }
  Window* window() {
    LOG(WARNING) << "getting window from proxy";
    script::v8c::EntryScope entry_scope(parent_->environment_settings()
                                            ->context()
                                            ->global_environment()
                                            ->isolate());
    script::v8c::EntryScope entry_scope1(window_->environment_settings()
                                             ->context()
                                             ->global_environment()
                                             ->isolate());
    return window_;
  }

  DEFINE_WRAPPABLE_TYPE(WindowProxy);

 private:
  friend class base::RefCountedThreadSafe<WindowProxy>;

  void DispatchEventInWindowThread(const scoped_refptr<web::Event>& event) {
    window_->DispatchEvent(event);
  }

  Window* window_;
  Window* parent_;

  DISALLOW_COPY_AND_ASSIGN(WindowProxy);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_WINDOW_PROXY_H_
