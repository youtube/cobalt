// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_LOGGING_EXCEPTION_STATE_H_
#define COBALT_SCRIPT_LOGGING_EXCEPTION_STATE_H_

#include <string>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "cobalt/script/exception_state.h"

namespace cobalt {
namespace script {

class LoggingExceptionState : public ExceptionState {
 public:
  LoggingExceptionState() : is_exception_set_(false) {}
  void SetException(const scoped_refptr<ScriptException>& exception) OVERRIDE {
    LogException(exception->name(), exception->message());
  }
  void SetSimpleExceptionVA(SimpleExceptionType type, const char* format,
                            va_list arguments) OVERRIDE {
    LogException(SimpleExceptionToString(type),
                 base::StringPrintV(format, arguments));
  }

  bool is_exception_set() const { return is_exception_set_; }

 private:
  void LogException(const std::string& name, const std::string& message) {
    is_exception_set_ = true;
    DLOG(ERROR) << "Script Exception " << name << ": " << message;
  }
  std::string SimpleExceptionToString(SimpleExceptionType type) const {
    switch (type) {
      case kError:
        return "Error";
      case kTypeError:
        return "TypeError";
      case kRangeError:
        return "RangeError";
      case kReferenceError:
        return "ReferenceError";
      case kSyntaxError:
        return "SyntaxError";
      case kURIError:
        return "URIError";
    }
    NOTREACHED();
    return "";
  }

  bool is_exception_set_;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_LOGGING_EXCEPTION_STATE_H_
