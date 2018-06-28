/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/mozjs-45/util/algorithm_helpers.h"

#include "base/logging.h"
#include "cobalt/script/mozjs-45/mozjs_exception_state.h"
#include "cobalt/script/mozjs-45/mozjs_global_environment.h"
#include "third_party/mozjs-45/js/src/jsapi.h"
#include "third_party/mozjs-45/js/src/jsarray.h"
#include "third_party/mozjs-45/js/src/jsiter.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {

namespace {
bool GetObjectProperty(JSContext* context, JS::HandleObject object,
                       const char* property_name,
                       JS::MutableHandleObject out_property) {
  DCHECK(context);
  DCHECK(object);
  DCHECK(property_name);

  JS::RootedValue property_value(context);
  if (!JS_GetProperty(context, object, property_name, &property_value)) {
    return false;
  }

  if (!property_value.isObjectOrNull()) {
    return false;
  }

  out_property.set(property_value.toObjectOrNull());
  return true;
}

bool GetSymbolIteratorId(JSContext* context, JS::MutableHandleId out_id) {
  DCHECK(context);

  MozjsGlobalEnvironment* env = MozjsGlobalEnvironment::GetFromContext(context);
  JS::RootedObject global(context, env->global_object_proxy());
  JS::RootedObject symbol(context);
  if (!GetObjectProperty(context, global, "Symbol", &symbol) || !symbol) {
    DLOG(INFO);
    return false;
  }

  JS::RootedValue iterator_symbol(context);
  if (!JS_GetProperty(context, symbol, "iterator", &iterator_symbol)) {
    DLOG(INFO);
    return false;
  }

  if (!iterator_symbol.isSymbol()) {
    DLOG(INFO);
    return false;
  }

  out_id.set(SYMBOL_TO_JSID(iterator_symbol.toSymbol()));
  return true;
}
}  // namespace

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
  JS::RootedId id(context);
  if (!GetSymbolIteratorId(context, &id)) {
    DLOG(INFO);
    return false;
  }

  JS::RootedValue iterator_value(context);
  if (!Invoke0(context, object, id, &iterator_value)) {
    return false;
  }

  out_iterator.set(iterator_value.toObjectOrNull());
  return out_iterator;
}

bool IteratorNext(JSContext* context, JS::HandleObject iterator,
                  JS::MutableHandleObject out_iterator_result) {
  JS::RootedValue more(context);
  if (!js::IteratorMore(context, iterator, &more)) {
    MozjsExceptionState exception(context);
    exception.SetSimpleException(kNotIterableType);
    return false;
  }

  if (more.isMagic(JS_NO_ITER_VALUE)) {
    JS::RootedValue undefined(context, JS::UndefinedValue());
    out_iterator_result.set(
        js::CreateItrResultObject(context, undefined, true));
    return true;
  }

  out_iterator_result.set(js::CreateItrResultObject(context, more, false));
  return true;
}

bool IteratorComplete(JSContext* context, JS::HandleObject iterator_result) {
  JS::RootedValue done(context);
  if (!JS_GetProperty(context, iterator_result, "done", &done)) {
    return false;
  }

  return JS::ToBoolean(done);
}

bool IteratorValue(JSContext* context, JS::HandleObject iterator_result,
                   JS::MutableHandleValue out_result) {
  JS::RootedValue value(context);
  if (!JS_GetProperty(context, iterator_result, "value", &value)) {
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
  JS::AutoValueVector args(context);
  return JS::Call(context, value, function, args, out_result);
}

bool Invoke0(JSContext* context, JS::HandleObject value,
             const char* property_name, JS::MutableHandleValue out_result) {
  JS::RootedValue property(context);
  if (!JS_GetProperty(context, value, property_name, &property)) {
    return false;
  }

  if (!property.isObject() || !JS::IsCallable(property.toObjectOrNull())) {
    return false;
  }

  JS::RootedFunction function(context, JS_ValueToFunction(context, property));
  DCHECK(function);

  return util::Call0(context, function, value, out_result);
}

bool Invoke0(JSContext* context, JS::HandleObject value,
             JS::HandleId property_id, JS::MutableHandleValue out_result) {
  JS::RootedValue property(context);
  if (!JS_GetPropertyById(context, value, property_id, &property)) {
    return false;
  }

  if (!property.isObject() || !JS::IsCallable(property.toObjectOrNull())) {
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
