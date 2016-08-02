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

#ifndef COBALT_SCRIPT_MOZJS_CONVERT_CALLBACK_RETURN_VALUE_H_
#define COBALT_SCRIPT_MOZJS_CONVERT_CALLBACK_RETURN_VALUE_H_

#include "base/logging.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/logging_exception_state.h"
#include "cobalt/script/mozjs/conversion_helpers.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Helper template functions for Callback functions' return values before being
// returned to Cobalt.
// Converts the return value from JavaScript into the correct Cobalt type, or
// sets the exception bit if conversion fails.
template <typename R>
CallbackResult<R> ConvertCallbackReturnValue(JSContext* context,
                                             JS::HandleValue value) {
  // TODO: Pass conversion flags to callback function return value if
  // appropriate.
  const int kConversionFlags = 0;
  CallbackResult<R> callback_result;
  LoggingExceptionState exception_state;
  FromJSValue(context, value, kConversionFlags, &exception_state,
              &callback_result.result);
  callback_result.exception = exception_state.is_exception_set();
  return callback_result;
}

template <>
inline CallbackResult<void> ConvertCallbackReturnValue(JSContext* context,
                                                       JS::HandleValue value) {
  // No conversion necessary.
  return CallbackResult<void>();
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_CONVERT_CALLBACK_RETURN_VALUE_H_
