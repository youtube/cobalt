// Copyright 2019-2020 The MediaPipe Authors.
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

#include <vector>

#include "mediapipe/calculators/core/concatenate_vector_calculator.h"
#include "mediapipe/framework/formats/detection.pb.h"

namespace mediapipe {

// Example config:
//
// node {
//   calculator: "ConcatenateDetectionVectorCalculator"
//   input_stream: "detection_vector_1"
//   input_stream: "detection_vector_2"
//   output_stream: "concatenated_detection_vector"
// }
//
typedef ConcatenateVectorCalculator<::mediapipe::Detection>
    ConcatenateDetectionVectorCalculator;
MEDIAPIPE_REGISTER_NODE(ConcatenateDetectionVectorCalculator);

}  // namespace mediapipe
