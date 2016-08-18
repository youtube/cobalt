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

#ifndef COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_INTERFACE_H_
#define COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_INTERFACE_H_

#include "cobalt/script/callback_interface_traits.h"
#include "third_party/mozjs/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Helper class to get the actual callable object from a JSObject implementing
// a callback interface.
// Returns true if a callable was found, and false if not.
bool GetCallableForCallbackInterface(JSContext* context,
                                     JS::HandleObject implementing_object,
                                     const char* property_name,
                                     JS::MutableHandleValue out_callable);

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_MOZJS_MOZJS_CALLBACK_INTERFACE_H_
