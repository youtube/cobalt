// Copyright 2019 The MediaPipe Authors.
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

#ifndef MEDIAPIPE_UTIL_TRACKING_MOTION_MODELS_CV_H_
#define MEDIAPIPE_UTIL_TRACKING_MOTION_MODELS_CV_H_

#include "mediapipe/framework/port/opencv_core_inc.h"
#include "mediapipe/util/tracking/motion_models.h"
#include "mediapipe/util/tracking/motion_models.pb.h"  // NOLINT

namespace mediapipe {

template <class Model>
class ModelCvConvert {};

// Specialized implementations, with additional functionality if needed.
template <>
class ModelCvConvert<TranslationModel> {
 public:
  // Returns 2x3 floating point cv::Mat with model parameters.
  static void ToCvMat(const TranslationModel& model, cv::Mat* matrix);
};

template <>
class ModelCvConvert<LinearSimilarityModel> {
 public:
  // Returns 2x3 floating point cv::Mat with model parameters.
  static void ToCvMat(const LinearSimilarityModel& model, cv::Mat* matrix);
};

template <>
class ModelCvConvert<AffineModel> {
 public:
  // Returns 2x3 floating point cv::Mat with model parameters.
  static void ToCvMat(const AffineModel& model, cv::Mat* matrix);
};

template <>
class ModelCvConvert<Homography> {
 public:
  // Returns 3x3 floating point cv::Mat with model parameters.
  static void ToCvMat(const Homography& model, cv::Mat* matrix);
};

template <class Model>
void ModelToCvMat(const Model& model, cv::Mat* matrix) {
  ModelCvConvert<Model>::ToCvMat(model, matrix);
}

}  // namespace mediapipe

#endif  // MEDIAPIPE_UTIL_TRACKING_MOTION_MODELS_CV_H_
