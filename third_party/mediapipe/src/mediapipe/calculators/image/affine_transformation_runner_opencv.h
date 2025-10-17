// Copyright 2021 The MediaPipe Authors.
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

#ifndef MEDIAPIPE_CALCULATORS_IMAGE_AFFINE_TRANSFORMATION_RUNNER_OPENCV_H_
#define MEDIAPIPE_CALCULATORS_IMAGE_AFFINE_TRANSFORMATION_RUNNER_OPENCV_H_

#include <memory>

#include "absl/status/statusor.h"
#include "mediapipe/calculators/image/affine_transformation.h"
#include "mediapipe/framework/formats/image_frame.h"

namespace mediapipe {

absl::StatusOr<
    std::unique_ptr<AffineTransformation::Runner<ImageFrame, ImageFrame>>>
CreateAffineTransformationOpenCvRunner(
    AffineTransformation::Interpolation interpolation);

}  // namespace mediapipe

#endif  // MEDIAPIPE_CALCULATORS_IMAGE_AFFINE_TRANSFORMATION_RUNNER_OPENCV_H_
