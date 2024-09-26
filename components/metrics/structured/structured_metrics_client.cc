// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/structured_metrics_client.h"

#include <utility>

#include "base/no_destructor.h"
#include "components/metrics/structured/event.h"

namespace metrics::structured {

StructuredMetricsClient::StructuredMetricsClient() = default;
StructuredMetricsClient::~StructuredMetricsClient() = default;

// static
StructuredMetricsClient* StructuredMetricsClient::Get() {
  static base::NoDestructor<StructuredMetricsClient> client;
  return client.get();
}

void StructuredMetricsClient::Record(Event&& event) {
  if (delegate_ && delegate_->IsReadyToRecord()) {
    delegate_->RecordEvent(std::move(event));
  }
}

void StructuredMetricsClient::SetDelegate(RecordingDelegate* delegate) {
  delegate_ = delegate;
}

void StructuredMetricsClient::UnsetDelegate() {
  delegate_ = nullptr;
}

}  // namespace metrics::structured
