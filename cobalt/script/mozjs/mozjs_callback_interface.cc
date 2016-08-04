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

#include "cobalt/script/mozjs/mozjs_callback_interface.h"

#include "base/logging.h"

namespace cobalt {
namespace script {
namespace mozjs {

// Helper class to get the actual callable object from a JSObject implementing
// a callback interface.
// Returns true if a callable was found, and false if not.
bool GetCallableForCallbackInterface(JSContext* context,
                                     JS::HandleObject implementing_object,
                                     const char* property_name,
                                     JS::MutableHandleValue out_callable) {
  DCHECK(implementing_object);
  DCHECK(property_name);

  if (JS_ObjectIsCallable(context, implementing_object)) {
    out_callable.set(OBJECT_TO_JSVAL(implementing_object));
    return true;
  }
  // Implementing object is not callable. Check for a callable property of the
  // specified name.
  JS::RootedValue property(context);
  if (JS_GetProperty(context, implementing_object, property_name,
                     property.address())) {
    if (property.isObject() &&
        JS_ObjectIsCallable(context, JSVAL_TO_OBJECT(property))) {
      out_callable.set(property);
      return true;
    }
  }

  // Implementing object is not callable, nor does it have a callable property
  // of the specified name.
  return false;
}

}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
