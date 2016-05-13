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

#ifndef COBALT_H5VCC_H5VCC_RUNTIME_H_
#define COBALT_H5VCC_H5VCC_RUNTIME_H_

#include "cobalt/base/event_dispatcher.h"
#include "cobalt/h5vcc/h5vcc_runtime_event_target.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccRuntime : public script::Wrappable {
 public:
  explicit H5vccRuntime(base::EventDispatcher* event_dispatcher);
  ~H5vccRuntime();

  const scoped_refptr<H5vccRuntimeEventTarget>& on_pause() const;
  const scoped_refptr<H5vccRuntimeEventTarget>& on_resume() const;

  DEFINE_WRAPPABLE_TYPE(H5vccRuntime);

 private:
  // Called by the event dispatcher to handle an application event.
  void OnApplicationEvent(const base::Event* event);

  scoped_refptr<H5vccRuntimeEventTarget> on_pause_;
  scoped_refptr<H5vccRuntimeEventTarget> on_resume_;

  // Non-owned reference used to receive application event callbacks.
  base::EventDispatcher* event_dispatcher_;

  // Application event callback.
  base::EventCallback application_event_callback_;

  DISALLOW_COPY_AND_ASSIGN(H5vccRuntime);
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_RUNTIME_H_
