/* Copyright 2022 The MediaPipe Authors.

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

#include "mediapipe/calculators/core/merge_to_vector_calculator.h"

#include "mediapipe/framework/formats/detection.pb.h"
#include "mediapipe/framework/formats/image.h"

namespace mediapipe {
namespace api2 {

typedef MergeToVectorCalculator<mediapipe::Image> MergeImagesToVectorCalculator;
MEDIAPIPE_REGISTER_NODE(MergeImagesToVectorCalculator);

typedef MergeToVectorCalculator<mediapipe::GpuBuffer>
    MergeGpuBuffersToVectorCalculator;
MEDIAPIPE_REGISTER_NODE(MergeGpuBuffersToVectorCalculator);

typedef MergeToVectorCalculator<mediapipe::Detection>
    MergeDetectionsToVectorCalculator;
MEDIAPIPE_REGISTER_NODE(MergeDetectionsToVectorCalculator);

}  // namespace api2
}  // namespace mediapipe
