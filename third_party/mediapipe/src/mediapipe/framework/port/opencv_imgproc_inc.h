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

#ifndef MEDIAPIPE_PORT_OPENCV_IMGPROC_INC_H_
#define MEDIAPIPE_PORT_OPENCV_IMGPROC_INC_H_

#include <opencv2/core/version.hpp>

#include "mediapipe/framework/port/opencv_core_inc.h"

#ifdef CV_VERSION_EPOCH  // for OpenCV 2.x
#include <opencv2/imgproc/imgproc.hpp>
#else
#include <opencv2/imgproc.hpp>
#if CV_VERSION_MAJOR == 4
#include <opencv2/imgproc/types_c.h>
#endif
#endif

#endif  // MEDIAPIPE_PORT_OPENCV_IMGPROC_INC_H_
