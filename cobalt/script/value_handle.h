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
#include <vector>

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


class StructuredClone : public v8::ValueSerializer::Delegate,
                        public v8::ValueDeserializer::Delegate {
 public:
  explicit StructuredClone(const ValueHandleHolder& value);

  // v8::ValueSerializer::Delegate
  void ThrowDataCloneError(v8::Local<v8::String> message) override {
    isolate_->ThrowException(v8::Exception::Error(message));
  }
  v8::Maybe<uint32_t> GetSharedArrayBufferId(
      v8::Isolate* isolate,
      v8::Local<v8::SharedArrayBuffer> shared_array_buffer) override;

  // v8::ValueDeserializer::Delegate
  v8::MaybeLocal<v8::SharedArrayBuffer> GetSharedArrayBufferFromId(
      v8::Isolate* isolate, uint32_t clone_id) override;

  Handle<ValueHandle> Deserialize(v8::Isolate* isolate);

  bool failed() const { return serialize_failed_ || deserialize_failed_; }

 private:
  struct BufferDeleter {
    void operator()(uint8_t* buffer) { free(buffer); }
  };
  using DataBufferPtr = std::unique_ptr<uint8_t[], BufferDeleter>;

  struct DataBuffer {
    DataBufferPtr ptr;
    size_t size;

    DataBuffer(uint8_t* ptr, size_t size)
        : ptr(DataBufferPtr(ptr)), size(size) {}
  };

  v8::Isolate* isolate_;
  Handle<ValueHandle> deserialized_;
  std::unique_ptr<DataBuffer> data_buffer_;
  bool serialize_failed_ = false;
  bool deserialize_failed_ = false;
  std::vector<std::shared_ptr<v8::BackingStore>> backing_stores_;
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

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_VALUE_HANDLE_H_
