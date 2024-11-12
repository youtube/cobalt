// Copyright 2020 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MEDIAPIPE_CALCULATORS_TENSOR_IMAGE_TO_TENSOR_CONVERTER_GL_BUFFER_H_
#define MEDIAPIPE_CALCULATORS_TENSOR_IMAGE_TO_TENSOR_CONVERTER_GL_BUFFER_H_

#include "mediapipe/framework/port.h"

#if MEDIAPIPE_OPENGL_ES_VERSION >= MEDIAPIPE_OPENGL_ES_31

#include <memory>

#include "mediapipe/calculators/tensor/image_to_tensor_converter.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/port/statusor.h"

namespace mediapipe {

// Creates image to tensor (represented as OpenGL buffer) converter.
// NOTE: mediapipe::GlCalculatorHelper::UpdateContract invocation must precede
// converter creation.
absl::StatusOr<std::unique_ptr<ImageToTensorConverter>>
CreateImageToGlBufferTensorConverter(CalculatorContext* cc,
                                     bool input_starts_at_bottom,
                                     BorderMode border_mode);

}  // namespace mediapipe

#endif  // MEDIAPIPE_OPENGL_ES_VERSION >= MEDIAPIPE_OPENGL_ES_31

#endif  // MEDIAPIPE_CALCULATORS_TENSOR_IMAGE_TO_TENSOR_CONVERTER_GL_BUFFER_H_
