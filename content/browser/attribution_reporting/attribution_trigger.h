// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_

#include <vector>

#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_result.mojom.h"
#include "content/browser/attribution_reporting/event_level_result.mojom.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/trigger_verification.h"

namespace content {

// Struct which represents a conversion registration event that was observed in
// the renderer and is now being used by the browser process.
class CONTENT_EXPORT AttributionTrigger {
 public:
  using AggregatableResult = attribution_reporting::mojom::AggregatableResult;
  using EventLevelResult = attribution_reporting::mojom::EventLevelResult;

  AttributionTrigger(attribution_reporting::SuitableOrigin reporting_origin,
                     attribution_reporting::TriggerRegistration registration,
                     attribution_reporting::SuitableOrigin destination_origin,
                     std::vector<network::TriggerVerification> verifications,
                     bool is_within_fenced_frame);

  AttributionTrigger(const AttributionTrigger&);
  AttributionTrigger& operator=(const AttributionTrigger&);
  AttributionTrigger(AttributionTrigger&&);
  AttributionTrigger& operator=(AttributionTrigger&&);
  ~AttributionTrigger();

  const attribution_reporting::SuitableOrigin& reporting_origin() const {
    return reporting_origin_;
  }

  const attribution_reporting::TriggerRegistration& registration() const {
    return registration_;
  }

  attribution_reporting::TriggerRegistration& registration() {
    return registration_;
  }

  const attribution_reporting::SuitableOrigin& destination_origin() const {
    return destination_origin_;
  }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  const std::vector<network::TriggerVerification>& verifications() const {
    return verifications_;
  }

 private:
  attribution_reporting::SuitableOrigin reporting_origin_;

  attribution_reporting::TriggerRegistration registration_;

  // Origin on which this trigger was registered.
  attribution_reporting::SuitableOrigin destination_origin_;

  // Optional tokens attesting to the veracity of the trigger.
  std::vector<network::TriggerVerification> verifications_;

  // Whether the trigger is registered within a fenced frame tree.
  bool is_within_fenced_frame_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
