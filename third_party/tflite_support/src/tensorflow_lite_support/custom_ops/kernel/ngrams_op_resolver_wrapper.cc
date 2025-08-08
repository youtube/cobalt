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

#include "pybind11/pybind11.h"
#include "tensorflow/lite/mutable_op_resolver.h"
#include "tensorflow_lite_support/custom_ops/kernel/ngrams_op_resolver.h"

PYBIND11_MODULE(_pywrap_ngrams_op_resolver, m) {
  m.doc() = "_pywrap_ngrams_op_resolver";
  m.def(
      "AddNgramsCustomOp",
      [](uintptr_t resolver) {
        tflite::ops::custom::AddNgramsCustomOp(
            reinterpret_cast<tflite::MutableOpResolver*>(resolver));
      },
      "Op registerer function for the tftext:Ngrams custom op.");
}
