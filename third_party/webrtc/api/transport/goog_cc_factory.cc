/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/goog_cc_factory.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "api/transport/network_control.h"
#include "api/units/time_delta.h"
#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "modules/congestion_controller/goog_cc_scream_network_controller/goog_cc_scream_network_controller.h"

namespace webrtc {

GoogCcNetworkControllerFactory::GoogCcNetworkControllerFactory(
    GoogCcFactoryConfig config)
    : factory_config_(std::move(config)) {}

std::unique_ptr<NetworkControllerInterface>
GoogCcNetworkControllerFactory::Create(NetworkControllerConfig config) {
  GoogCcConfig goog_cc_config;
  if (factory_config_.network_state_estimator_factory) {
    goog_cc_config.network_state_estimator =
        factory_config_.network_state_estimator_factory->Create(
            &config.env.field_trials());
  }
  if (factory_config_.network_state_predictor_factory) {
    goog_cc_config.network_state_predictor =
        factory_config_.network_state_predictor_factory
            ->CreateNetworkStatePredictor();
  }
  if (factory_config_.rfc_8888_feedback_negotiated &&
      !config.env.field_trials().IsDisabled("WebRTC-Bwe-ScreamV2") &&
      config.env.field_trials().Lookup("WebRTC-Bwe-ScreamV2") != "") {
    return std::make_unique<GoogCcScreamNetworkController>(
        config, std::move(goog_cc_config));
  }
  return std::make_unique<GoogCcNetworkController>(config,
                                                   std::move(goog_cc_config));
}

TimeDelta GoogCcNetworkControllerFactory::GetProcessInterval() const {
  const int64_t kUpdateIntervalMs = 25;
  return TimeDelta::Millis(kUpdateIntervalMs);
}

}  // namespace webrtc
