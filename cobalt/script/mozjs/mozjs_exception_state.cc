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
#include "cobalt/script/mozjs/mozjs_exception_state.h"

#include <string>

#include "base/logging.h"
#include "cobalt/script/mozjs/conversion_helpers.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

namespace {
std::string SimpleExceptionToString(ExceptionState::SimpleExceptionType type) {
  switch (type) {
    case ExceptionState::kError:
      return "Error";
    case ExceptionState::kTypeError:
      return "TypeError";
    case ExceptionState::kRangeError:
      return "RangeError";
    case ExceptionState::kReferenceError:
      return "ReferenceError";
    case ExceptionState::kSyntaxError:
      return "SyntaxError";
    case ExceptionState::kURIError:
      return "URIError";
  }
  NOTREACHED();
  return "";
}
}  // namespace

void MozjsExceptionState::SetException(
    const scoped_refptr<ScriptException>& exception) {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
}

void MozjsExceptionState::SetSimpleException(
    SimpleExceptionType simple_exception, const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_exception_set_);

  std::stringstream stream;
  stream << SimpleExceptionToString(simple_exception) << ": " << message;
  // JS_ReportError first builds an error message from the given sprintf-style
  // format string and any additional arguments passed after it. The resulting
  // error message is passed to the context's JSErrorReporter callback.
  JS_ReportError(context_, stream.str().c_str());
  is_exception_set_ = true;
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
