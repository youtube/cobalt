// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/ml/navigator_ml.h"  // nogncheck
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"

namespace blink {

static const WrapperTypeInfo g_dummy_wrapper_type_info = {
    gin::kEmbedderBlink,
    nullptr,
    nullptr,
    "Dummy",
    nullptr,
    v8::CppHeapPointerTag::kDefaultTag,
    v8::CppHeapPointerTag::kDefaultTag,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kIdlInterface,
    false,
};

// --- V8 WrapperTypeInfo Stubs ---

#define STUB_V8_WRAPPER(ClassName) \
  class ClassName { public: static const WrapperTypeInfo wrapper_type_info_; }; \
  const WrapperTypeInfo ClassName::wrapper_type_info_ = g_dummy_wrapper_type_info;

// WebNN V8 Wrappers
STUB_V8_WRAPPER(V8ML)
STUB_V8_WRAPPER(V8MLContext)
STUB_V8_WRAPPER(V8MLTensor)
STUB_V8_WRAPPER(V8MLGraph)
STUB_V8_WRAPPER(V8MLGraphBuilder)
STUB_V8_WRAPPER(V8MLOperand)

#undef STUB_V8_WRAPPER

// --- WebNN C++ Stubs ---

ML* NavigatorML::ml(NavigatorBase& navigator) {
  return nullptr;
}

}  // namespace blink
