/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/lite/core/shims/cc/kernels/register.h"

namespace tflite {
namespace task {
// Default provider for OpResolver, provides BuiltinOpResolver.
std::unique_ptr<OpResolver> CreateOpResolver() {  // NOLINT
  return std::unique_ptr<tflite_shims::ops::builtin::BuiltinOpResolver>(
      new tflite_shims::ops::builtin::BuiltinOpResolver);
}

}  // namespace task
}  // namespace tflite
