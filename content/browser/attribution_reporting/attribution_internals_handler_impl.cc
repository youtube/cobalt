// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_handler_impl.h"

#include <stdint.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/parsing_utils.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_debug_report.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/os_registration.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_errors.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using Attributability =
    ::attribution_internals::mojom::WebUISource::Attributability;

using Empty = ::attribution_internals::mojom::Empty;
using ReportStatus = ::attribution_internals::mojom::ReportStatus;
using ReportStatusPtr = ::attribution_internals::mojom::ReportStatusPtr;

using ::attribution_internals::mojom::WebUIDebugReport;

attribution_internals::mojom::WebUISourcePtr WebUISource(
    const StoredSource& source,
    Attributability attributability) {
  const CommonSourceInfo& common_info = source.common_info();
  return attribution_internals::mojom::WebUISource::New(
      source.source_event_id(), common_info.source_origin(),
      source.destination_sites(), common_info.reporting_origin(),
      source.source_time().InMillisecondsFSinceUnixEpoch(),
      source.expiry_time().InMillisecondsFSinceUnixEpoch(),
      source.event_report_windows(),
      source.aggregatable_report_window_time().InMillisecondsFSinceUnixEpoch(),
      source.max_event_level_reports(), common_info.source_type(),
      source.priority(), source.debug_key(), source.dedup_keys(),
      source.filter_data(),
      base::MakeFlatMap<std::string, std::string>(
          source.aggregation_keys().keys(), {},
          [](const auto& key) {
            return std::make_pair(
                key.first,
                attribution_reporting::HexEncodeAggregationKey(key.second));
          }),
      source.aggregatable_budget_consumed(), source.aggregatable_dedup_keys(),
      source.trigger_config(), source.debug_cookie_set(), attributability);
}

void ForwardSourcesToWebUI(
    attribution_internals::mojom::Handler::GetActiveSourcesCallback
        web_ui_callback,
    std::vector<StoredSource> active_sources) {
  std::vector<attribution_internals::mojom::WebUISourcePtr> web_ui_sources;
  web_ui_sources.reserve(active_sources.size());

  for (const StoredSource& source : active_sources) {
    Attributability attributability;
    switch (source.attribution_logic()) {
      case StoredSource::AttributionLogic::kTruthfully:
        attributability = Attributability::kAttributable;
        break;
      case StoredSource::AttributionLogic::kNever:
        attributability = Attributability::kNoisedNever;
        break;
      case StoredSource::AttributionLogic::kFalsely:
        attributability = Attributability::kNoisedFalsely;
        break;
    }

    if (attributability == Attributability::kAttributable) {
      switch (source.active_state()) {
        case StoredSource::ActiveState::kActive:
          attributability = Attributability::kAttributable;
          break;
        case StoredSource::ActiveState::kReachedEventLevelAttributionLimit:
          attributability = Attributability::kReachedEventLevelAttributionLimit;
          break;
        case StoredSource::ActiveState::kInactive:
          NOTREACHED_NORETURN();
      }
    }

    web_ui_sources.push_back(WebUISource(source, attributability));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_sources));
}

attribution_internals::mojom::WebUIReportPtr WebUIReport(
    const AttributionReport& report,
    bool is_debug_report,
    ReportStatusPtr status) {
  namespace ai_mojom = attribution_internals::mojom;

  const AttributionInfo& attribution_info = report.attribution_info();

  ai_mojom::WebUIReportDataPtr data = absl::visit(
      base::Overloaded{
          [](const AttributionReport::EventLevelData& event_level_data) {
            return ai_mojom::WebUIReportData::NewEventLevelData(
                ai_mojom::WebUIReportEventLevelData::New(
                    event_level_data.priority,
                    event_level_data.source.attribution_logic() ==
                        StoredSource::AttributionLogic::kTruthfully));
          },

          [](const AttributionReport::AggregatableAttributionData&
                 aggregatable_data) {
            std::vector<ai_mojom::AggregatableHistogramContributionPtr>
                contributions;
            base::ranges::transform(
                aggregatable_data.contributions,
                std::back_inserter(contributions),
                [](const auto& contribution) {
                  return ai_mojom::AggregatableHistogramContribution::New(
                      attribution_reporting::HexEncodeAggregationKey(
                          contribution.key()),
                      contribution.value());
                });

            return ai_mojom::WebUIReportData::NewAggregatableAttributionData(
                ai_mojom::WebUIReportAggregatableAttributionData::New(
                    std::move(contributions),
                    aggregatable_data.common_data.verification_token,
                    aggregatable_data.common_data.aggregation_coordinator_origin
                        ? aggregatable_data.common_data
                              .aggregation_coordinator_origin->Serialize()
                        : "",
                    /*is_null_report=*/false));
          },

          [](const AttributionReport::NullAggregatableData& null_data)
              -> ai_mojom::WebUIReportDataPtr {
            std::vector<ai_mojom::AggregatableHistogramContributionPtr>
                contributions;
            contributions.push_back(
                ai_mojom::AggregatableHistogramContribution::New(
                    attribution_reporting::HexEncodeAggregationKey(0),
                    /*value=*/0));
            return ai_mojom::WebUIReportData::NewAggregatableAttributionData(
                ai_mojom::WebUIReportAggregatableAttributionData::New(
                    std::move(contributions),
                    null_data.common_data.verification_token,
                    null_data.common_data.aggregation_coordinator_origin
                        ? null_data.common_data.aggregation_coordinator_origin
                              ->Serialize()
                        : "",
                    /*is_null_report=*/true));
          },
      },
      report.data());

  return attribution_internals::mojom::WebUIReport::New(
      report.id(), report.ReportURL(is_debug_report),
      /*trigger_time=*/attribution_info.time.InMillisecondsFSinceUnixEpoch(),
      /*report_time=*/report.report_time().InMillisecondsFSinceUnixEpoch(),
      SerializeAttributionJson(report.ReportBody(), /*pretty_print=*/true),
      std::move(status), std::move(data));
}

void ForwardReportsToWebUI(
    attribution_internals::mojom::Handler::GetReportsCallback web_ui_callback,
    std::vector<AttributionReport> pending_reports) {
  std::vector<attribution_internals::mojom::WebUIReportPtr> web_ui_reports;
  web_ui_reports.reserve(pending_reports.size());
  for (const AttributionReport& report : pending_reports) {
    web_ui_reports.push_back(
        WebUIReport(report, /*is_debug_report=*/false,
                    ReportStatus::NewPending(Empty::New())));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_reports));
}

}  // namespace

AttributionInternalsHandlerImpl::AttributionInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingRemote<attribution_internals::mojom::Observer> observer,
    mojo::PendingReceiver<attribution_internals::mojom::Handler> handler)
    : web_ui_(raw_ref<WebUI>::from_ptr(web_ui)),
      observer_(std::move(observer)),
      handler_(this, std::move(handler)) {
  if (auto* manager =
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
    manager_observation_.Observe(manager);
    observer_.set_disconnect_handler(
        base::BindOnce(&AttributionInternalsHandlerImpl::OnObserverDisconnected,
                       base::Unretained(this)));
  }
}

AttributionInternalsHandlerImpl::~AttributionInternalsHandlerImpl() = default;

void AttributionInternalsHandlerImpl::IsAttributionReportingEnabled(
    attribution_internals::mojom::Handler::IsAttributionReportingEnabledCallback
        callback) {
  content::WebContents* contents = web_ui_->GetWebContents();
  bool attribution_reporting_enabled =
      AttributionManager::FromWebContents(contents) &&
      GetContentClient()->browser()->IsAttributionReportingOperationAllowed(
          contents->GetBrowserContext(),
          ContentBrowserClient::AttributionReportingOperation::kAny,
          /*rfh=*/nullptr, /*source_origin=*/nullptr,
          /*destination_origin=*/nullptr, /*reporting_origin=*/nullptr,
          /*can_bypass=*/nullptr);

  // TODO(apaseltiner): This is a layering violation: The internals handler
  // should query the manager for its configuration, not the command line,
  // especially since `AttributionManager::SetDebugMode()` can cause its value
  // to change after initialization.
  bool debug_mode = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAttributionReportingDebugMode);

  std::move(callback).Run(attribution_reporting_enabled, debug_mode,
                          AttributionManager::GetAttributionSupport(contents));
}

void AttributionInternalsHandlerImpl::GetActiveSources(
    attribution_internals::mojom::Handler::GetActiveSourcesCallback callback) {
  if (AttributionManager* manager =
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
    manager->GetActiveSourcesForWebUI(
        base::BindOnce(&ForwardSourcesToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AttributionInternalsHandlerImpl::GetReports(
    attribution_internals::mojom::Handler::GetReportsCallback callback) {
  if (AttributionManager* manager =
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
    manager->GetPendingReportsForInternalUse(
        /*limit=*/1000,
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AttributionInternalsHandlerImpl::SendReports(
    const std::vector<AttributionReport::Id>& ids,
    attribution_internals::mojom::Handler::SendReportsCallback callback) {
  if (AttributionManager* manager =
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
    manager->SendReportsForWebUI(ids, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::ClearStorage(
    attribution_internals::mojom::Handler::ClearStorageCallback callback) {
  if (AttributionManager* manager =
          AttributionManager::FromWebContents(web_ui_->GetWebContents())) {
    manager->ClearData(base::Time::Min(), base::Time::Max(),
                       /*filter=*/base::NullCallback(),
                       /*filter_builder=*/nullptr,
                       /*delete_rate_limit_data=*/true, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::OnSourcesChanged() {
  observer_->OnSourcesChanged();
}

void AttributionInternalsHandlerImpl::OnReportsChanged() {
  observer_->OnReportsChanged();
}

namespace {

using WebUISourceRegistration =
    ::attribution_internals::mojom::WebUISourceRegistration;

attribution_internals::mojom::WebUIRegistrationPtr GetRegistration(
    base::Time time,
    const attribution_reporting::SuitableOrigin& context_origin,
    const attribution_reporting::SuitableOrigin& reporting_origin,
    std::string registration_json,
    absl::optional<uint64_t> cleared_debug_key) {
  auto reg = attribution_internals::mojom::WebUIRegistration::New();
  reg->time = time.InMillisecondsFSinceUnixEpoch();
  reg->context_origin = context_origin;
  reg->reporting_origin = reporting_origin;
  reg->registration_json = std::move(registration_json);
  reg->cleared_debug_key = cleared_debug_key;
  return reg;
}

}  // namespace

void AttributionInternalsHandlerImpl::OnSourceHandled(
    const StorableSource& source,
    base::Time source_time,
    absl::optional<uint64_t> cleared_debug_key,
    attribution_reporting::mojom::StoreSourceResult result) {
  auto web_ui_source = WebUISourceRegistration::New();
  web_ui_source->registration =
      GetRegistration(source_time, source.common_info().source_origin(),
                      source.common_info().reporting_origin(),
                      SerializeAttributionJson(source.registration().ToJson(),
                                               /*pretty_print=*/true),
                      cleared_debug_key);
  web_ui_source->type = source.common_info().source_type();
  web_ui_source->status = std::move(result);

  observer_->OnSourceHandled(std::move(web_ui_source));
}

void AttributionInternalsHandlerImpl::OnReportSent(
    const AttributionReport& report,
    bool is_debug_report,
    const SendResult& info) {
  ReportStatusPtr status;
  switch (info.status) {
    case SendResult::Status::kSent:
      status = ReportStatus::NewSent(info.http_response_code);
      break;
    case SendResult::Status::kDropped:
      status = ReportStatus::NewProhibitedByBrowserPolicy(Empty::New());
      break;
    case SendResult::Status::kFailure:
    case SendResult::Status::kTransientFailure:
      status = ReportStatus::NewNetworkError(
          net::ErrorToShortString(info.network_error));
      break;
    case SendResult::Status::kAssemblyFailure:
    case SendResult::Status::kTransientAssemblyFailure:
      status = ReportStatus::NewFailedToAssemble(Empty::New());
      break;
  }

  observer_->OnReportSent(
      WebUIReport(report, is_debug_report, std::move(status)));
}

void AttributionInternalsHandlerImpl::OnDebugReportSent(
    const AttributionDebugReport& report,
    int status,
    base::Time time) {
  auto web_report = WebUIDebugReport::New();
  web_report->url = report.ReportUrl();
  web_report->time = time.InMillisecondsFSinceUnixEpoch();
  web_report->body =
      SerializeAttributionJson(report.ReportBody(), /*pretty_print=*/true);

  web_report->status =
      status > 0
          ? attribution_internals::mojom::DebugReportStatus::
                NewHttpResponseCode(status)
          : attribution_internals::mojom::DebugReportStatus::NewNetworkError(
                net::ErrorToShortString(status));

  observer_->OnDebugReportSent(std::move(web_report));
}

void AttributionInternalsHandlerImpl::OnOsRegistration(
    base::Time time,
    const OsRegistration& registration,
    bool is_debug_key_allowed,
    attribution_reporting::mojom::OsRegistrationResult result) {
  auto web_ui_os_registration =
      attribution_internals::mojom::WebUIOsRegistration::New();
  web_ui_os_registration->time =
      time.InMillisecondsFSinceUnixEpochIgnoringNull();
  web_ui_os_registration->registration_url = registration.registration_url;
  web_ui_os_registration->top_level_origin = registration.top_level_origin;
  web_ui_os_registration->is_debug_key_allowed = is_debug_key_allowed;
  web_ui_os_registration->debug_reporting = registration.debug_reporting;
  web_ui_os_registration->type = registration.GetType();
  web_ui_os_registration->result = result;

  observer_->OnOsRegistration(std::move(web_ui_os_registration));
}

void AttributionInternalsHandlerImpl::OnTriggerHandled(
    const AttributionTrigger& trigger,
    const absl::optional<uint64_t> cleared_debug_key,
    const CreateReportResult& result) {
  const attribution_reporting::TriggerRegistration& registration =
      trigger.registration();

  auto web_ui_trigger = attribution_internals::mojom::WebUITrigger::New();
  web_ui_trigger->registration =
      GetRegistration(result.trigger_time(), trigger.destination_origin(),
                      trigger.reporting_origin(),
                      SerializeAttributionJson(registration.ToJson(),
                                               /*pretty_print=*/true),
                      cleared_debug_key);
  web_ui_trigger->event_level_result = result.event_level_status();
  web_ui_trigger->aggregatable_result = result.aggregatable_status();
  web_ui_trigger->verifications = trigger.verifications();

  observer_->OnTriggerHandled(std::move(web_ui_trigger));

  if (const absl::optional<AttributionReport>& report =
          result.replaced_event_level_report()) {
    DCHECK_EQ(
        result.event_level_status(),
        AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority);
    DCHECK(result.new_event_level_report().has_value());

    observer_->OnReportDropped(
        WebUIReport(*report, /*is_debug_report=*/false,
                    ReportStatus::NewReplacedByHigherPriorityReport(
                        result.new_event_level_report()
                            ->external_report_id()
                            .AsLowercaseString())));
  }
}

void AttributionInternalsHandlerImpl::OnObserverDisconnected() {
  manager_observation_.Reset();
}

}  // namespace content
