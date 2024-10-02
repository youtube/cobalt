// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/delegating_events_processor.h"

namespace metrics::structured {

DelegatingEventsProcessor::DelegatingEventsProcessor() = default;
DelegatingEventsProcessor::~DelegatingEventsProcessor() = default;

// Each individual events_processor could be checked, but this will need to be
// checked during OnEventsRecord().
bool DelegatingEventsProcessor::ShouldProcessOnEventRecord(const Event& event) {
  return true;
}

void DelegatingEventsProcessor::OnEventsRecord(Event* event) {
  DCHECK(event);

  for (auto& events_processor : events_processors_) {
    if (events_processor->ShouldProcessOnEventRecord(*event)) {
      // Note that every |events_processor| is operating on the same |event|.
      // Race conditions should be mangaged by the client.
      events_processor->OnEventsRecord(event);
    }
  }
}

void DelegatingEventsProcessor::AddEventsProcessor(
    std::unique_ptr<EventsProcessorInterface> events_processor) {
  DCHECK(events_processor);

  events_processors_.push_back(std::move(events_processor));
}

}  // namespace metrics::structured
