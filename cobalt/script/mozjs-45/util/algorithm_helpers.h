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
#ifndef COBALT_SCRIPT_MOZJS_45_UTIL_ALGORITHM_HELPERS_H_
#define COBALT_SCRIPT_MOZJS_45_UTIL_ALGORITHM_HELPERS_H_

#include "third_party/mozjs-45/js/src/jsapi.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace util {
// Whether |value1| and |value2| are both GC things and are the same GC thing.
// i.e. Returns |false| if either value is not a GC thing.
bool IsSameGcThing(JSContext* context, JS::HandleValue value1,
                   JS::HandleValue value2);

// https://tc39.github.io/ecma262/#sec-getiterator
bool GetIterator(JSContext* context, JS::HandleObject object,
                 JS::MutableHandleObject out_iterator);

// https://tc39.github.io/ecma262/#sec-iteratornext
bool IteratorNext(JSContext* context, JS::HandleObject iterator,
                  JS::MutableHandleObject out_iterator_result);

// https://tc39.github.io/ecma262/#sec-iteratorcomplete
bool IteratorComplete(JSContext* context, JS::HandleObject iterator_result);

// https://tc39.github.io/ecma262/#sec-iteratorvalue
bool IteratorValue(JSContext* context, JS::HandleObject iterator_result,
                   JS::MutableHandleValue out_result);

// https://tc39.github.io/ecma262/#sec-iteratorstep
//
// TODO: out_iterator_result should probably be a Value so it can return false
// or the next iterator record. Right now this and IteratorComplete are the only
// functions where a false return value doesn't necessarily mean an error.
bool IteratorStep(JSContext* context, JS::HandleObject iterator,
                  JS::MutableHandleObject out_iterator_result);

// https://tc39.github.io/ecma262/#sec-iteratorclose
bool IteratorClose(JSContext* context, JS::HandleObject iterator);

// https://tc39.github.io/ecma262/#sec-call
// Call |function| on object |value| with 0 arguments.
bool Call0(JSContext* context, JS::HandleFunction function,
           JS::HandleObject value, JS::MutableHandleValue out_result);

// https://tc39.github.io/ecma262/#sec-invoke
// Invoke a method named |property_name| on object |value| with 0 arguments.
bool Invoke0(JSContext* context, JS::HandleObject value,
             const char* property_name, JS::MutableHandleValue out_result);

bool Invoke0(JSContext* context, JS::HandleObject value,
             JS::HandleId property_id, JS::MutableHandleValue out_result);
}  // namespace util
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
#endif  // COBALT_SCRIPT_MOZJS_45_UTIL_ALGORITHM_HELPERS_H_
