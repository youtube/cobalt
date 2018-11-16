// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_collector.h"

#include <memory>
#include <utility>

#include "components/metrics/call_stack_profile_encoding.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace metrics {

CallStackProfileCollector::CallStackProfileCollector(
    CallStackProfileParams::Process expected_process)
    : expected_process_(expected_process) {}

CallStackProfileCollector::~CallStackProfileCollector() {}

// static
void CallStackProfileCollector::Create(
    CallStackProfileParams::Process expected_process,
    mojom::CallStackProfileCollectorRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<CallStackProfileCollector>(expected_process),
      std::move(request));
}

void CallStackProfileCollector::Collect(base::TimeTicks start_timestamp,
                                        SampledProfile profile) {
  if (profile.process() != ToExecutionContextProcess(expected_process_))
    return;

  CallStackProfileMetricsProvider::ReceiveCompletedProfile(start_timestamp,
                                                           std::move(profile));
}

}  // namespace metrics
