// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_V8C_EXCEPTION_STATE_H_
#define COBALT_SCRIPT_V8C_V8C_EXCEPTION_STATE_H_

#include <string>

#include "base/stringprintf.h"
#include "base/threading/thread_checker.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

v8::Local<v8::Value> CreateErrorObject(v8::Isolate* isolate,
                                       SimpleExceptionType type);

class V8cExceptionState : public ExceptionState {
 public:
  explicit V8cExceptionState(v8::Isolate* isolate)
      : isolate_(isolate), is_exception_set_(false) {}

  // ExceptionState interface:
  void SetException(const scoped_refptr<ScriptException>& exception) override;
  void SetSimpleExceptionVA(SimpleExceptionType type, const char* format,
                            va_list arguments) override;

  bool is_exception_set() const { return is_exception_set_; }

 private:
  base::ThreadChecker thread_checker_;

  v8::Isolate* isolate_;
  bool is_exception_set_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_EXCEPTION_STATE_H_
