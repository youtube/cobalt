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
  void SetSimpleExceptionVA(SimpleExceptionType type, const char* format,
                            va_list arguments) OVERRIDE;

  bool is_exception_set() const { return is_exception_set_; }

  // For when a Cobalt operation that is expected to modify an
  // |ExceptionState| to communicate failure performs operations that can
  // potentially cause an exception to be thrown by SpiderMonkey, and we want
  // to reuse that exception instead of creating our own (which also likely
  // would not be able to match the descriptiveness of the SpiderMonkey
  // exception).
  void SetExceptionAlreadySet(JSContext* context) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_exception_set_);
    DCHECK(JS_IsExceptionPending(context));
    is_exception_set_ = true;
  }

  static JSObject* CreateErrorObject(JSContext* context,
                                     SimpleExceptionType type);

 private:
  bool is_exception_set_;
  JSContext* context_;
  base::ThreadChecker thread_checker_;
};

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_45_MOZJS_EXCEPTION_STATE_H_
