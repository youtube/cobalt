// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/declarative_performance_observer/declarative_performance_observer.h"

#include "base/check.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/load_timing_info.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace content {

namespace {
constexpr char kDeclarativePerformanceObserverReportType[] =
    "performance-observer";
}  // namespace

DeclarativePerformanceObserver::DeclarativePerformanceObserver(
    RenderFrameHost* rfh,
    NavigationHandle* navigation_handle)
    : DocumentUserData<DeclarativePerformanceObserver>(rfh) {
  const network::mojom::DeclarativePerformanceObserverPolicy* policy =
      navigation_handle->GetDeclarativePerformanceObserverPolicy();
  CHECK(policy);
  reporting_endpoint_ = *policy->reporting_endpoint;
  enabled_types_ = base::flat_set<network::mojom::PerformanceEntryType>(
      policy->entry_types.begin(), policy->entry_types.end());
  if (policy->include_user_timing) {
    include_user_timing_ =
        base::flat_set<std::string>(policy->include_user_timing->begin(),
                                    policy->include_user_timing->end());
  } else {
    include_user_timing_ = std::nullopt;
  }

  navigation_start_ = navigation_handle->NavigationStart();
  committed_url_ = navigation_handle->GetURL();

  network_anonymization_key_ =
      rfh->GetIsolationInfoForSubresources().network_anonymization_key();
  reporting_source_ = rfh->GetReportingSource();

  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  started_in_foreground_ =
      rfh->GetLifecycleState() == RenderFrameHost::LifecycleState::kActive &&
      web_contents && web_contents->GetVisibility() == Visibility::VISIBLE;

  if (enabled_types_.contains(
          network::mojom::PerformanceEntryType::kVisibilityState)) {
    base::DictValue entry;
    entry.Set("name", started_in_foreground_ ? "visible" : "hidden");
    entry.Set("entryType", "visibility-state");
    entry.Set("startTime", 0.0);
    entry.Set("duration", 0.0);
    AddEntryToBuffer(std::move(entry));
  }

  if (enabled_types_.contains(
          network::mojom::PerformanceEntryType::kNavigation)) {
    base::DictValue entry;
    entry.Set("name", committed_url_.spec());
    entry.Set("entryType", "navigation");
    entry.Set("startTime", 0.0);

    const NavigationHandleTiming& timing =
        navigation_handle->GetNavigationHandleTiming();

    auto to_relative_ms = [&](base::TimeTicks t) {
      return t.is_null() ? 0.0 : (t - navigation_start_).InMillisecondsF();
    };

    entry.Set("responseStart",
              to_relative_ms(timing.final_response_start_time));
    entry.Set("requestStart", to_relative_ms(timing.final_request_start_time));
    entry.Set("connectStart",
              to_relative_ms(timing.final_request_connect_start_time));
    entry.Set("connectEnd",
              to_relative_ms(timing.final_request_connect_end_time));
    entry.Set("domainLookupStart",
              to_relative_ms(timing.final_request_domain_lookup_start_time));
    entry.Set("domainLookupEnd",
              to_relative_ms(timing.final_request_domain_lookup_end_time));
    entry.Set("secureConnectionStart",
              to_relative_ms(timing.final_request_ssl_start_time));
    entry.Set("activationStart", 0.0);

    std::string delivery_type = "";
    if (navigation_handle->WasResponseCached()) {
      delivery_type = "cache";
    } else if (navigation_handle->IsPrerenderedPageActivation()) {
      delivery_type = "navigational-prefetch";
    }
    entry.Set("deliveryType", delivery_type);

    AddEntryToBuffer(std::move(entry));
  }
}

DeclarativePerformanceObserver::~DeclarativePerformanceObserver() {
  if (render_frame_host().IsActive()) {
    AppendSessionEndEntry();
    FlushMetrics();
  }
}

void DeclarativePerformanceObserver::OnDidFinishNavigation(
    NavigationHandle* navigation_handle) {
  DCHECK_EQ(navigation_handle->GetRenderFrameHost(), &render_frame_host());
  DCHECK(navigation_handle->IsServedFromBackForwardCache());

  navigation_start_ = navigation_handle->NavigationStart();
  buffered_entries_.clear();

  // Reset `is_session_ended_` so that a new session-end entry can be appended
  // when this restored document eventually enters BFCache or is closed.
  is_session_ended_ = false;

  if (enabled_types_.contains(
          network::mojom::PerformanceEntryType::kNavigation)) {
    base::DictValue nav_entry;
    nav_entry.Set("name", committed_url_.spec());
    nav_entry.Set("entryType", "navigation");
    nav_entry.Set("type", "back_forward");
    nav_entry.Set("startTime", 0.0);
    AddEntryToBuffer(std::move(nav_entry));
  }

  if (enabled_types_.contains(
          network::mojom::PerformanceEntryType::kVisibilityState)) {
    base::DictValue visibility_entry;
    visibility_entry.Set("name", "visible");
    visibility_entry.Set("entryType", "visibility-state");
    visibility_entry.Set("startTime", 0.0);
    visibility_entry.Set("duration", 0.0);
    AddEntryToBuffer(std::move(visibility_entry));
  }
}

void DeclarativePerformanceObserver::OnPrerenderActivation(
    NavigationHandle* navigation_handle) {
  DCHECK_EQ(navigation_handle->GetRenderFrameHost(), &render_frame_host());
  DCHECK(navigation_handle->IsPrerenderedPageActivation());

  for (auto& entry : buffered_entries_) {
    if (entry.is_dict()) {
      auto& dict = entry.GetDict();
      const std::string* entry_type = dict.FindString("entryType");
      if (entry_type && *entry_type == "navigation") {
        base::TimeDelta activation_start =
            navigation_handle->NavigationStart() - navigation_start_;
        dict.Set("activationStart", activation_start.InMillisecondsF());
        dict.Set("deliveryType", "navigational-prefetch");
        break;
      }
    }
  }

  // During prerender activation, the tab itself does not change visibility, so
  // WebContentsObserver::OnVisibilityChanged does not fire. We must explicitly
  // log a visibility entry here to reflect that the page is now visible.
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (web_contents && web_contents->GetVisibility() == Visibility::VISIBLE) {
    OnVisibilityChanged(Visibility::VISIBLE);
  }
}

void DeclarativePerformanceObserver::OnVisibilityChanged(
    Visibility visibility) {
  CHECK(render_frame_host().IsActive());
  if (enabled_types_.contains(
          network::mojom::PerformanceEntryType::kVisibilityState)) {
    base::DictValue entry;
    entry.Set("name", visibility == Visibility::HIDDEN ? "hidden" : "visible");
    entry.Set("entryType", "visibility-state");
    base::TimeDelta relative_time = base::TimeTicks::Now() - navigation_start_;
    entry.Set("startTime", relative_time.InMillisecondsF());
    entry.Set("duration", 0.0);
    AddEntryToBuffer(std::move(entry));
  }

  if (visibility == Visibility::HIDDEN) {
    FlushMetrics();
  }
}

void DeclarativePerformanceObserver::OnFrameDeleted() {
  AppendSessionEndEntry();
  FlushMetrics();
}

void DeclarativePerformanceObserver::OnEnterBFCache() {
  AppendSessionEndEntry();
  FlushMetrics();
}

void DeclarativePerformanceObserver::SetStoragePartitionForTesting(  // IN-TEST
    StoragePartition* storage_partition) {
  storage_partition_for_testing_ = storage_partition;
}

void DeclarativePerformanceObserver::FlushMetrics() {
  if (buffered_entries_.empty()) {
    return;
  }

  base::DictValue body;
  body.Set("entries", std::move(buffered_entries_));
  buffered_entries_.clear();

  StoragePartition* storage_partition =
      storage_partition_for_testing_
          ? storage_partition_for_testing_.get()
          : render_frame_host().GetStoragePartition();

  if (storage_partition) {
    storage_partition->GetNetworkContext()->QueueReport(
        kDeclarativePerformanceObserverReportType, reporting_endpoint_,
        committed_url_, reporting_source_, network_anonymization_key_,
        std::move(body));
  }
}

void DeclarativePerformanceObserver::AppendSessionEndEntry() {
  if (is_session_ended_) {
    return;
  }
  is_session_ended_ = true;

  if (enabled_types_.contains(
          network::mojom::PerformanceEntryType::kVisibilityState) ||
      enabled_types_.contains(
          network::mojom::PerformanceEntryType::kNavigation)) {
    base::DictValue entry;
    entry.Set("name", "session-end-event");
    entry.Set("entryType", "session-end");
    base::TimeDelta relative_time = base::TimeTicks::Now() - navigation_start_;
    entry.Set("startTime", relative_time.InMillisecondsF());
    entry.Set("duration", 0.0);
    AddEntryToBuffer(std::move(entry));
  }
}

void DeclarativePerformanceObserver::AddEntryToBuffer(base::DictValue entry) {
  buffered_entries_.Append(std::move(entry));
}

// static
void DeclarativePerformanceObserver::Bind(
    RenderFrameHost* rfh,
    mojo::PendingReceiver<blink::mojom::DeclarativePerformanceObserverHost>
        receiver) {
  auto* observer = DeclarativePerformanceObserver::GetForCurrentDocument(rfh);
  if (observer) {
    observer->BindReceiver(std::move(receiver));
  }
}

void DeclarativePerformanceObserver::BindReceiver(
    mojo::PendingReceiver<blink::mojom::DeclarativePerformanceObserverHost>
        receiver) {
  if (receiver_.is_bound()) {
    mojo::ReportBadMessage(
        "DeclarativePerformanceObserver rebinding is prohibited.");
    return;
  }
  receiver_.Bind(std::move(receiver));
}

void DeclarativePerformanceObserver::DidObservePerformanceEntries(
    std::vector<blink::mojom::DeclarativePerformanceEntryPtr> entries) {
  for (auto& entry : entries) {
    if (!enabled_types_.contains(network::mojom::PerformanceEntryType::kMark)) {
      continue;
    }
    if (include_user_timing_ && !include_user_timing_->contains(entry->name)) {
      continue;
    }

    base::DictValue dict;
    dict.Set("name", entry->name);
    dict.Set("entryType", "mark");
    dict.Set("startTime", entry->start_time.InMillisecondsF());
    dict.Set("duration", 0.0);

    if (entry->detail.has_value()) {
      dict.Set("detail", std::move(entry->detail.value()));
    }
    AddEntryToBuffer(std::move(dict));
  }
}

DOCUMENT_USER_DATA_KEY_IMPL(DeclarativePerformanceObserver);

}  // namespace content
