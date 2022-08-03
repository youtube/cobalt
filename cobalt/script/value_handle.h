// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_VALUE_HANDLE_H_
#define COBALT_SCRIPT_VALUE_HANDLE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/ref_counted.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/script_value.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {

// A handle to a script value that is managed by the JavaScript engine. Note
// that this can be any JavaScript Value type (null, undefined, Boolean,
// Number, String, Object, and Symbol), rather than just Object.
class ValueHandle {
 protected:
  ValueHandle() {}
  virtual ~ValueHandle() {}
};

typedef ScriptValue<ValueHandle> ValueHandleHolder;

struct BufferDeleter {
  void operator()(uint8_t* buffer) { SbMemoryDeallocate(buffer); }
};
using DataBufferPtr = std::unique_ptr<uint8_t[], BufferDeleter>;

struct DataBuffer {
  DataBufferPtr ptr;
  size_t size;

  DataBuffer(uint8_t* ptr, size_t size) : ptr(DataBufferPtr(ptr)), size(size) {}
};

// Converts a "simple" object to a map of the object's properties. "Simple"
// means that the object's property names are strings and its property values
// must be a boolean, number or string. Note that this is implemented on a per
// engine basis. The use of this function should be avoided if possible.
// Eventually, JavaScript values will be exposed, making this function obsolete.
// Example "simple" object:
// {'countryCode': 'US'}
// Example non-"simple" object:
// {'countryCode': null}
std::unordered_map<std::string, std::string> ConvertSimpleObjectToMap(
    const ValueHandleHolder& value, ExceptionState* exception_state);

v8::Isolate* GetIsolate(const ValueHandleHolder& value);
v8::Local<v8::Value> GetV8Value(const ValueHandleHolder& value);
ValueHandleHolder* DeserializeScriptValue(v8::Isolate* isolate,
                                          const DataBuffer& data_buffer);
std::unique_ptr<DataBuffer> SerializeScriptValue(
    const ValueHandleHolder& value);

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_VALUE_HANDLE_H_
