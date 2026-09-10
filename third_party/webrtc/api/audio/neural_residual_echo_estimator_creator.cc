/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/neural_residual_echo_estimator_creator.h"

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/audio/neural_residual_echo_estimator.h"
#include "modules/audio_processing/aec3/neural_residual_echo_estimator_impl.h"
#include "rtc_base/checks.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"

namespace webrtc {

absl_nullable std::unique_ptr<NeuralResidualEchoEstimator>
CreateNeuralResidualEchoEstimator(absl::string_view tflite_model_path,
                                  const tflite::OpResolver* absl_nonnull
                                      op_resolver) {
  RTC_CHECK(op_resolver);
  return NeuralResidualEchoEstimatorImpl::Create(tflite_model_path,
                                                 *op_resolver);
}

}  // namespace webrtc
