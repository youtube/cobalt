/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/create_test_environment.h"

#include <memory>
#include <utility>
#include <variant>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/field_trials_view.h"
#include "rtc_base/checks.h"
#include "test/create_test_field_trials.h"

namespace webrtc {
namespace {

struct SetFieldTrials {
  void operator()(absl::string_view field_trials) {
    factory.Set(CreateTestFieldTrialsPtr(field_trials));
  }

  void operator()(FieldTrialsView* absl_nonnull field_trials) {
    RTC_CHECK(field_trials != nullptr);
    factory.Set(field_trials);
  }

  void operator()(absl_nonnull std::unique_ptr<FieldTrialsView> field_trials) {
    RTC_CHECK(field_trials != nullptr);
    factory.Set(std::move(field_trials));
  }

  void operator()(FieldTrials field_trials) {
    factory.Set(std::make_unique<FieldTrials>(std::move(field_trials)));
  }

  EnvironmentFactory& factory;
};

}  // namespace

Environment CreateTestEnvironment(CreateTestEnvironmentOptions o) {
  EnvironmentFactory factory;

  std::visit(SetFieldTrials{factory}, std::move(o.field_trials));

  if (o.time != nullptr) {
    factory.Set(o.time->GetClock());
    factory.Set(o.time->GetTaskQueueFactory());
  }
  return factory.Create();
}

}  // namespace webrtc
