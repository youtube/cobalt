// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_ABORT_CONTROLLER_H_
#define COBALT_DOM_ABORT_CONTROLLER_H_

#include "cobalt/dom/abort_signal.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// This represents the DOM AbortController object.
//    https://dom.spec.whatwg.org/#interface-abortcontroller
class AbortController : public script::Wrappable {
 public:
  // Web API: AbortController
  explicit AbortController(script::EnvironmentSettings* settings);

  const scoped_refptr<AbortSignal>& signal() const { return abort_signal_; }
  void Abort();

  DEFINE_WRAPPABLE_TYPE(AbortController);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  scoped_refptr<AbortSignal> abort_signal_;

  DISALLOW_COPY_AND_ASSIGN(AbortController);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ABORT_CONTROLLER_H_
