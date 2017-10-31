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

#include "cobalt/script/mozjs-45/mozjs_exception_state.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "cobalt/script/mozjs-45/conversion_helpers.h"
#include "third_party/mozjs-45/js/src/jsexn.h"

namespace cobalt {
namespace script {
namespace mozjs {

namespace {

JSExnType ConvertToMozjsExceptionType(SimpleExceptionType type) {
  switch (type) {
    case kError:
      return JSEXN_ERR;
    case kTypeError:
      return JSEXN_TYPEERR;
    case kRangeError:
      return JSEXN_RANGEERR;
    case kReferenceError:
      return JSEXN_REFERENCEERR;
    case kSyntaxError:
      return JSEXN_SYNTAXERR;
    case kURIError:
      return JSEXN_URIERR;
  }
  NOTREACHED();
  return JSEXN_ERR;
}

const JSErrorFormatString* GetErrorMessage(void* user_ref,
                                           const unsigned error_number) {
  return static_cast<JSErrorFormatString*>(user_ref);
}

}  // namespace

void MozjsExceptionState::SetException(
    const scoped_refptr<ScriptException>& exception) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_exception_set_);

  MozjsGlobalEnvironment* global_environment =
      static_cast<MozjsGlobalEnvironment*>(JS_GetContextPrivate(context_));

  JS::RootedObject exception_object(
      context_,
      global_environment->wrapper_factory()->GetWrapperProxy(exception));
  JS::RootedValue exception_value(context_);
  exception_value.setObject(*exception_object);

  JS_SetPendingException(context_, exception_value);

  is_exception_set_ = true;
}

void MozjsExceptionState::SetSimpleExceptionVA(SimpleExceptionType type,
                                               const char* format,
                                               va_list arguments) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_exception_set_);

  std::string error_message = base::StringPrintV(format, arguments);
  JSErrorFormatString format_string;
  format_string.format = error_message.c_str();
  // Already fed arguments for format.
  format_string.argCount = 0;
  format_string.exnType = ConvertToMozjsExceptionType(type);

  // This function creates a JSErrorReport, populate it with an error message
  // obtained from the given JSErrorCallback. The resulting error message is
  // passed to the context's JSErrorReporter callback.
  JS_ReportErrorNumber(context_, GetErrorMessage,
                       static_cast<void*>(&format_string), type);
  is_exception_set_ = true;
}

// static
JSObject* MozjsExceptionState::CreateErrorObject(JSContext* context,
                                                 SimpleExceptionType type) {
  JSExnType mozjs_type = ConvertToMozjsExceptionType(type);
  JS::RootedObject error_prototype(context);
  if (!JS_GetClassPrototype(context, js::GetExceptionProtoKey(mozjs_type),
                            &error_prototype)) {
    DLOG(ERROR) << "Failed to get Error prototype.";
    return NULL;
  }
  return JS_NewObjectWithGivenProto(context, NULL, error_prototype);
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
