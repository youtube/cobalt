// Copyright 2016 Google Inc. All Rights Reserved.
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
#ifndef COBALT_SCRIPT_MOZJS_45_MOZJS_EXCEPTION_STATE_H_
#define COBALT_SCRIPT_MOZJS_45_MOZJS_EXCEPTION_STATE_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "cobalt/script/exception_state.h"
#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

class MozjsExceptionState : public ExceptionState {
 public:
  explicit MozjsExceptionState(JSContext* context)
      : is_exception_set_(false), context_(context) {}
  // ExceptionState interface
  void SetException(const scoped_refptr<ScriptException>& exception) OVERRIDE;
  void SetSimpleException(MessageTypeVar message_type, ...) OVERRIDE;

  bool is_exception_set() const { return is_exception_set_; }

 private:
  bool is_exception_set_;
  JSContext* context_;
  base::ThreadChecker thread_checker_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_EXCEPTION_STATE_H_
