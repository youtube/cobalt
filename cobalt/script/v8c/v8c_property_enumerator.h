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

#ifndef COBALT_SCRIPT_V8C_V8C_PROPERTY_ENUMERATOR_H_
#define COBALT_SCRIPT_V8C_V8C_PROPERTY_ENUMERATOR_H_

#include <string>

#include "base/compiler_specific.h"
#include "cobalt/script/property_enumerator.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cPropertyEnumerator : public cobalt::script::PropertyEnumerator {
 public:
  V8cPropertyEnumerator(v8::Isolate* isolate, v8::Local<v8::Array>* array)
      : isolate_(isolate), array_(array), next_index_(0) {
    DCHECK(array);
  }

  void AddProperty(const std::string& property_name) override {
    int i = next_index_;
    next_index_++;

    v8::Local<v8::String> property_name_as_string =
        v8::String::NewFromUtf8(isolate_, property_name.c_str(),
                                v8::NewStringType::kNormal)
            .ToLocalChecked();
    (*array_)->Set(i, property_name_as_string);
  }

 private:
  v8::Isolate* isolate_;
  v8::Local<v8::Array>* const array_;
  int next_index_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_PROPERTY_ENUMERATOR_H_
