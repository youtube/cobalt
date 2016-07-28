/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_FUNCTION_IMPL_H_
#define COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_FUNCTION_IMPL_H_

#include "cobalt/script/javascriptcore/conversion_helpers.h"
#include "cobalt/script/javascriptcore/jsc_callback_function.h"

namespace cobalt {
namespace script {
namespace javascriptcore {
namespace internal {
// Helper template functions for Callback functions' return values before being
// returned to Cobalt.
// Converts the return value from JavaScript into the correct Cobalt type, or
// sets the exception bit if conversion fails.
template <typename R>
inline CallbackResult<R> ConvertReturnValue(JSC::ExecState* exec_state,
                                            JSC::JSValue jsvalue) {
  // TODO: Pass conversion flags to callback function return value.
  const int kConversionFlags = 0;
  CallbackResult<R> callback_result;
  LoggingExceptionState exception_state;
  FromJSValue(exec_state, jsvalue, kConversionFlags, &exception_state,
              &callback_result.result);
  callback_result.exception = exception_state.is_exception_set();
  return callback_result;
}
template <>
inline CallbackResult<void> ConvertReturnValue(JSC::ExecState* exec_state,
                                               JSC::JSValue jsvalue) {
  // No conversion necessary.
  return CallbackResult<void>();
}
}  // namespace internal
}  // namespace javascriptcore
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_JAVASCRIPTCORE_JSC_CALLBACK_FUNCTION_IMPL_H_
