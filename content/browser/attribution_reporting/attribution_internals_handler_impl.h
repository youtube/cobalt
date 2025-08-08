// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class WebUI;

// Implements the mojo endpoint for the attribution internals WebUI which
// proxies calls to the `AttributionManager` to get information about stored
// attribution data. Also observes the manager in order to push events, e.g.
// reports being sent or dropped, to the internals WebUI. Owned by
// `AttributionInternalsUI`.
class AttributionInternalsHandlerImpl
    : public attribution_internals::mojom::Handler,
      public AttributionObserver {
 public:
  AttributionInternalsHandlerImpl(
      WebUI* web_ui,
      mojo::PendingRemote<attribution_internals::mojom::Observer>,
      mojo::PendingReceiver<attribution_internals::mojom::Handler>);
  AttributionInternalsHandlerImpl(const AttributionInternalsHandlerImpl&) =
      delete;
  AttributionInternalsHandlerImpl& operator=(
      const AttributionInternalsHandlerImpl&) = delete;
  AttributionInternalsHandlerImpl(AttributionInternalsHandlerImpl&&) = delete;
  AttributionInternalsHandlerImpl& operator=(
      AttributionInternalsHandlerImpl&&) = delete;
  ~AttributionInternalsHandlerImpl() override;

  // mojom::AttributionInternalsHandler:
  void IsAttributionReportingEnabled(
      attribution_internals::mojom::Handler::
          IsAttributionReportingEnabledCallback callback) override;
  void GetActiveSources(
      attribution_internals::mojom::Handler::GetActiveSourcesCallback callback)
      override;
  void GetReports(attribution_internals::mojom::Handler::GetReportsCallback
                      callback) override;
  void SendReports(const std::vector<AttributionReport::Id>& ids,
                   attribution_internals::mojom::Handler::SendReportsCallback
                       callback) override;
  void ClearStorage(attribution_internals::mojom::Handler::ClearStorageCallback
                        callback) override;

 private:
  // AttributionObserver:
  void OnSourcesChanged() override;
  void OnReportsChanged() override;
  void OnSourceHandled(
      const StorableSource& source,
      base::Time source_time,
      absl::optional<uint64_t> cleared_debug_key,
      attribution_reporting::mojom::StoreSourceResult) override;
  void OnReportSent(const AttributionReport& report,
                    bool is_debug_report,
                    const SendResult& info) override;
  void OnDebugReportSent(const AttributionDebugReport&,
                         int status,
                         base::Time) override;
  void OnTriggerHandled(const AttributionTrigger& trigger,
                        absl::optional<uint64_t> cleared_debug_key,
                        const CreateReportResult& result) override;
  void OnOsRegistration(
      base::Time time,
      const OsRegistration&,
      bool is_debug_key_allowed,
      attribution_reporting::mojom::OsRegistrationResult) override;

  void OnObserverDisconnected();

  const raw_ref<WebUI> web_ui_;

  mojo::Remote<attribution_internals::mojom::Observer> observer_;

  mojo::Receiver<attribution_internals::mojom::Handler> handler_;

  base::ScopedObservation<AttributionManager, AttributionObserver>
      manager_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
