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

#ifndef COBALT_SCRIPT_MOZJS_CONVERSION_HELPERS_H_
#define COBALT_SCRIPT_MOZJS_CONVERSION_HELPERS_H_

#include <string>

#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/script/mozjs/mozjs_exception_state.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

void ToJSValue(JS::MutableHandleValue out_value, bool in_boolean,
               MozjsExceptionState* exception_state);
void FromJSValue(bool* out_boolean, JS::HandleValue value,
                 MozjsExceptionState* exception_state);

// TODO: These will be removed once conversion for all types is implemented.
template <typename T>
void ToJSValue(JS::MutableHandleValue out_value, const T& unimplemented,
               MozjsExceptionState* exception_state) {
  NOTIMPLEMENTED();
}

template <typename T>
void FromJSValue(T* out_unimplemented, JS::HandleValue value,
                 MozjsExceptionState* exception_state) {
  NOTIMPLEMENTED();
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_CONVERSION_HELPERS_H_
