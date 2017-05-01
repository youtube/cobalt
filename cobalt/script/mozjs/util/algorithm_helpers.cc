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

#include "cobalt/script/mozjs/util/algorithm_helpers.h"

#include "base/logging.h"
#include "cobalt/script/mozjs/mozjs_exception_state.h"
#include "third_party/mozjs/js/src/jsapi.h"
#include "third_party/mozjs/js/src/jsarray.h"
#include "third_party/mozjs/js/src/jsiter.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {

bool IsSameGcThing(JSContext* context, JS::HandleValue value1,
                   JS::HandleValue value2) {
  if (value1.isNullOrUndefined()) {
    return value2.isNullOrUndefined();
  }

  if (!value1.isGCThing() || !value2.isGCThing()) {
    return false;
  }

  return value1.toGCThing() == value2.toGCThing();
}

bool GetIterator(JSContext* context, JS::HandleObject object,
                 JS::MutableHandleObject out_iterator) {
  // Uses the jsiter API to get the iterator, as SM24 it doesn't seem to
  // implement Array[@@iterator] yet.
  JS::RootedValue iterator_value(context);
  if (!js::GetIterator(context, object, JSITER_FOR_OF, &iterator_value)) {
    return false;
  }

  JS::RootedObject iterator_object(context);
  if (!iterator_value.isObject() ||
      !JS_ValueToObject(context, iterator_value, iterator_object.address())) {
    MozjsExceptionState exception(context);
    exception.SetSimpleException(kNotObjectType);
    return false;
  }

  out_iterator.set(iterator_object);
  return true;
}

bool IteratorNext(JSContext* context, JS::HandleObject iterator,
                  JS::MutableHandleObject out_iterator_result) {
  // IMPORTANT NOTE! If there is More, we cannot enter another script without
  // calling Next to clear the result, or the interpreter will assert.
  JS::RootedValue more(context);
  if (!js_IteratorMore(context, iterator, &more)) {
    MozjsExceptionState exception(context);
    exception.SetSimpleException(kNotIterableType);
    return false;
  }

  JS::RootedObject result(context, JS_NewObject(context, NULL, NULL, NULL));
  JS::RootedValue done(context);
  done.setBoolean(!JS::ToBoolean(more));
  JS_SetProperty(context, result, "done", done.address());

  if (JS::ToBoolean(done)) {
    out_iterator_result.set(result);
    return true;
  }

  JS::RootedValue value(context);
  if (!js_IteratorNext(context, iterator, &value)) {
    MozjsExceptionState exception(context);
    exception.SetSimpleException(kNotIterableType);
    return false;
  }

  JS_SetProperty(context, result, "value", value.address());

  out_iterator_result.set(result);
  return true;
}

bool IteratorComplete(JSContext* context, JS::HandleObject iterator_result) {
  JS::RootedValue done(context);
  if (!JS_GetProperty(context, iterator_result, "done", done.address())) {
    return false;
  }

  return JS::ToBoolean(done);
}

bool IteratorValue(JSContext* context, JS::HandleObject iterator_result,
                   JS::MutableHandleValue out_result) {
  JS::RootedValue value(context);
  if (!JS_GetProperty(context, iterator_result, "value", value.address())) {
    return false;
  }

  out_result.set(value);
  return true;
}

bool IteratorStep(JSContext* context, JS::HandleObject iterator,
                  JS::MutableHandleObject out_iterator_result) {
  JS::RootedObject result(context);
  if (!IteratorNext(context, iterator, &result)) {
    return false;
  }

  if (IteratorComplete(context, result)) {
    return false;
  }
  out_iterator_result.set(result);
  return true;
}

bool IteratorClose(JSContext* context, JS::HandleObject iterator) {
  return js::CloseIterator(context, iterator);
}

bool Call0(JSContext* context, JS::HandleFunction function,
           JS::HandleObject value, JS::MutableHandleValue out_result) {
  const size_t kNumArguments = 0;
  JS::Value *args = NULL;
  js::SetValueRangeToNull(args, kNumArguments);
  js::AutoValueArray auto_array_rooter(context, args, kNumArguments);

  return JS::Call(context, value, function, kNumArguments, args,
                  out_result.address());
}

bool Invoke0(JSContext* context, JS::HandleObject value,
             const char* property_name, JS::MutableHandleValue out_result) {
  JS::RootedValue property(context);
  if (!JS_GetProperty(context, value, property_name, property.address())) {
    return false;
  }

  if (!property.isObject() ||
      !JS_ObjectIsCallable(context, JSVAL_TO_OBJECT(property))) {
    return false;
  }

  JS::RootedFunction function(context, JS_ValueToFunction(context, property));
  DCHECK(function);

  return util::Call0(context, function, value, out_result);
}

}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
