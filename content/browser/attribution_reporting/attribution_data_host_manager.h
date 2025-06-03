// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_

#include <stdint.h>

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom-forward.h"

class GURL;

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

struct AttributionInputEvent;
struct GlobalRenderFrameHostId;

// Interface responsible for coordinating `AttributionDataHost`s received from
// the renderer.
class AttributionDataHostManager
    : public base::SupportsWeakPtr<AttributionDataHostManager> {
 public:
  virtual ~AttributionDataHostManager() = default;

  // Registers a new data host with the browser process for the given context
  // origin. This is only called for events which are not associated with a
  // navigation. Passes the topmost ancestor of the initiator render frame for
  // obtaining the page access report. Passes the id  `last_navigation_id` of
  // the navigation to the frame from which this call originates.
  virtual void RegisterDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      attribution_reporting::SuitableOrigin context_origin,
      bool is_within_fenced_frame,
      attribution_reporting::mojom::RegistrationEligibility,
      GlobalRenderFrameHostId render_frame_id,
      int64_t last_navigation_id) = 0;

  // Registers a new data host which is associated with a navigation. The
  // context origin will be provided at a later time in
  // `NotifyNavigationRegistrationStarted()` called with the same
  // `attribution_src_token`. Returns `false` if `attribution_src_token` was
  // already registered.
  virtual bool RegisterNavigationDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) = 0;

  // Notifies the manager that an attribution-enabled navigation has started.
  // This may arrive before or after the attribution configuration is available
  // for a given data host. Passes the topmost ancestor of the initiator render
  // frame, `render_fame_id`, for obtaining the page access report. Every call
  // to `NotifyNavigationRegistrationStarted` must be eventually
  // followed by a call to `NotifyNavigationRegistrationCompleted`
  // with the same `attribution_src_token`.
  virtual void NotifyNavigationRegistrationStarted(
      const blink::AttributionSrcToken& attribution_src_token,
      AttributionInputEvent input_event,
      const attribution_reporting::SuitableOrigin& source_origin,
      bool is_within_fenced_frame,
      GlobalRenderFrameHostId render_frame_id,
      int64_t navigation_id,
      std::string devtools_request_id) = 0;

  // Notifies the manager that an attribution request tied to an
  // attribution-enabled navigation with token `attribution_src_token` has sent
  // a response. It might be called multiple times for the same navigation; for
  // redirects and a final response. Important: `headers` is untrusted. Must
  // only be called for `attribution_src_token` for which
  // `NotifyNavigationRegistrationStarted` was previously called.
  //
  // Returns true if there was Attribution Reporting relevant response headers.
  virtual bool NotifyNavigationRegistrationData(
      const blink::AttributionSrcToken& attribution_src_token,
      const net::HttpResponseHeaders* headers,
      GURL reporting_url,
      network::AttributionReportingRuntimeFeatures) = 0;

  // Notifies the manager whenever an attribution-enabled navigation request
  // completes. Should be called even for navigations when
  // `NotifyNavigationRegistrationStarted` did not get call for the token as
  // `RegisterNavigationDataHost` might have been called with the token.
  virtual void NotifyNavigationRegistrationCompleted(
      const blink::AttributionSrcToken& attribution_src_token) = 0;

  // Notifies the manager that a fenced frame reporting beacon was initiated
  // for reportEvent or for an automatic beacon and should be tracked.
  // The actual beacon may be sent after the navigation finished or after the
  // RFHI was destroyed, therefore we need to store the information for later
  // use. Passes the topmost ancestor of the initiator render frame for
  // obtaining the page access report.
  // `navigation_id` is the id of the navigation for automatic beacons and
  // `absl::nullopt` for event beacons.
  virtual void NotifyFencedFrameReportingBeaconStarted(
      BeaconId beacon_id,
      absl::optional<int64_t> navigation_id,
      attribution_reporting::SuitableOrigin source_origin,
      bool is_within_fenced_frame,
      AttributionInputEvent input_event,
      GlobalRenderFrameHostId render_frame_id,
      std::string devtools_request_id) = 0;

  // Notifies the manager whenever a response has been received to a beacon HTTP
  // request. Must be invoked for each redirect received, as well as the final
  // response. `reporting_origin` is the origin that sent `headers` that may
  // contain attribution source registration. `is_final_response` indicates
  // whether this is a redirect or a final response.
  // An opaque origin will be set for `reporting_origin` if the beacon failed to
  // be sent.
  virtual void NotifyFencedFrameReportingBeaconData(
      BeaconId beacon_id,
      network::AttributionReportingRuntimeFeatures,
      GURL reporting_url,
      const net::HttpResponseHeaders* headers,
      bool is_final_response) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
