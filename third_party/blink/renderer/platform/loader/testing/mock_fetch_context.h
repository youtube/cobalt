// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_MOCK_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_MOCK_FETCH_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_ref.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/weak_wrapper_resource_load_info_notifier.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

#include <memory>

namespace blink {

namespace mojom {
class ResourceLoadInfoNotifier;
}  // namespace mojom

class KURL;
class ResourceRequest;
struct ResourceLoaderOptions;

// Mocked FetchContext for testing.
class MockFetchContext : public FetchContext {
 public:
  MockFetchContext() = default;
  ~MockFetchContext() override = default;

  void set_blocked_urls(Vector<String> blocked_urls) {
    blocked_urls_ = std::move(blocked_urls);
  }
  void set_tagged_urls(Vector<String> tagged_urls) {
    tagged_urls_ = std::move(tagged_urls);
  }

  bool AllowImage(bool images_enabled, const KURL&) const override {
    return true;
  }
  absl::optional<ResourceRequestBlockedReason> CanRequest(
      ResourceType,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      ReportingDisposition,
      base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info)
      const override {
    return absl::nullopt;
  }
  absl::optional<ResourceRequestBlockedReason>
  CanRequestBasedOnSubresourceFilterOnly(
      ResourceType type,
      const ResourceRequest& resource_request,
      const KURL& url,
      const ResourceLoaderOptions& options,
      ReportingDisposition reporting_disposition,
      base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info)
      const override {
    if (blocked_urls_.Contains(url.GetString())) {
      return ResourceRequestBlockedReason::kSubresourceFilter;
    }

    return absl::nullopt;
  }
  absl::optional<ResourceRequestBlockedReason> CheckCSPForRequest(
      mojom::blink::RequestContextType,
      network::mojom::RequestDestination request_destination,
      const KURL& url,
      const ResourceLoaderOptions& options,
      ReportingDisposition reporting_disposition,
      const KURL& url_before_redirects,
      ResourceRequest::RedirectStatus redirect_status) const override {
    return absl::nullopt;
  }
  void AddResourceTiming(
      mojom::blink::ResourceTimingInfoPtr resource_timing_info,
      const AtomicString& initiator_type) override {}
  std::unique_ptr<ResourceLoadInfoNotifierWrapper>
  CreateResourceLoadInfoNotifierWrapper() override {
    if (!resource_load_info_notifier_)
      return nullptr;

    if (!weak_wrapper_resource_load_info_notifier_) {
      weak_wrapper_resource_load_info_notifier_ =
          std::make_unique<WeakWrapperResourceLoadInfoNotifier>(
              resource_load_info_notifier_);
    }
    return std::make_unique<ResourceLoadInfoNotifierWrapper>(
        weak_wrapper_resource_load_info_notifier_->AsWeakPtr());
  }

  bool CalculateIfAdSubresource(
      const ResourceRequestHead& resource_request,
      base::optional_ref<const KURL> alias_url,
      ResourceType type,
      const FetchInitiatorInfo& initiator_info) override {
    const KURL url =
        alias_url.has_value() ? alias_url.value() : resource_request.Url();
    return tagged_urls_.Contains(url.GetString());
  }

  void SetResourceLoadInfoNotifier(
      mojom::ResourceLoadInfoNotifier* resource_load_info_notifier) {
    resource_load_info_notifier_ = resource_load_info_notifier;
  }

 private:
  raw_ptr<mojom::ResourceLoadInfoNotifier, ExperimentalRenderer>
      resource_load_info_notifier_ = nullptr;
  std::unique_ptr<WeakWrapperResourceLoadInfoNotifier>
      weak_wrapper_resource_load_info_notifier_;
  Vector<String> blocked_urls_;
  Vector<String> tagged_urls_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_MOCK_FETCH_CONTEXT_H_
