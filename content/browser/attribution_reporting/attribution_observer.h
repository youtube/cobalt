// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/browser/attribution_reporting/store_source_result.mojom-forward.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#endif

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace content {

class AttributionDebugReport;
class AttributionReport;
class AttributionTrigger;
class CreateReportResult;
class StorableSource;

struct SendResult;

#if BUILDFLAG(IS_ANDROID)
struct OsRegistration;
#endif

// Observes events in the Attribution Reporting API. Observers are registered on
// `AttributionManager`.
class AttributionObserver : public base::CheckedObserver {
 public:
  ~AttributionObserver() override = default;

  // Called when sources in storage change.
  virtual void OnSourcesChanged() {}

  // Called when reports in storage change.
  virtual void OnReportsChanged() {}

  // Called when a source is registered, regardless of success.
  virtual void OnSourceHandled(
      const StorableSource& source,
      absl::optional<uint64_t> cleared_debug_key,
      attribution_reporting::mojom::StoreSourceResult) {}

  // Called when a report is sent, regardless of success, but not for attempts
  // that will be retried.
  virtual void OnReportSent(const AttributionReport& report,
                            bool is_debug_report,
                            const SendResult& info) {}

  // Called when a verbose debug report is sent, regardless of success.
  // If `status` is positive, it is the HTTP response code. Otherwise, it is the
  // network error.
  virtual void OnDebugReportSent(const AttributionDebugReport&,
                                 int status,
                                 base::Time) {}

  // Called when a trigger is registered, regardless of success.
  virtual void OnTriggerHandled(const AttributionTrigger& trigger,
                                absl::optional<uint64_t> cleared_debug_key,
                                const CreateReportResult& result) {}

  // Called when the source header registration json parser fails.
  virtual void OnFailedSourceRegistration(
      const std::string& header_value,
      base::Time source_time,
      const attribution_reporting::SuitableOrigin& source_origin,
      const attribution_reporting::SuitableOrigin& reporting_origin,
      attribution_reporting::mojom::SourceType,
      attribution_reporting::mojom::SourceRegistrationError) {}

#if BUILDFLAG(IS_ANDROID)
  // Called when an OS source or trigger registration is handled, regardless of
  // success.
  virtual void OnOsRegistration(
      base::Time time,
      const OsRegistration&,
      bool is_debug_key_allowed,
      attribution_reporting::mojom::OsRegistrationResult) {}
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_OBSERVER_H_
