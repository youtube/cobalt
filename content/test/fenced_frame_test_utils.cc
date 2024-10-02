// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fenced_frame_test_utils.h"

#include "base/memory/ref_counted.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "third_party/blink/public/common/features.h"

namespace content {

using SharedStorageReportingMap = base::flat_map<std::string, ::GURL>;

FrameTreeNode* GetFencedFrameRootNode(FrameTreeNode* node) {
  int inner_node_id =
      node->current_frame_host()->inner_tree_main_frame_tree_node_id();
  return FrameTreeNode::GloballyFindByID(inner_node_id);
}

void SimulateSharedStorageURNMappingComplete(
    FencedFrameURLMapping& fenced_frame_url_mapping,
    const GURL& urn_uuid,
    const GURL& mapped_url,
    const url::Origin& shared_storage_origin,
    double budget_to_charge,
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter) {
  SharedStorageBudgetMetadata budget_metadata = {
      .origin = shared_storage_origin, .budget_to_charge = budget_to_charge};

  fenced_frame_url_mapping.OnSharedStorageURNMappingResultDetermined(
      urn_uuid,
      FencedFrameURLMapping::SharedStorageURNMappingResult(
          mapped_url, budget_metadata, std::move(fenced_frame_reporter)));
}

TestFencedFrameURLMappingResultObserver::
    TestFencedFrameURLMappingResultObserver() = default;

TestFencedFrameURLMappingResultObserver::
    ~TestFencedFrameURLMappingResultObserver() = default;

void TestFencedFrameURLMappingResultObserver::OnFencedFrameURLMappingComplete(
    const absl::optional<FencedFrameProperties>& properties) {
  mapping_complete_observed_ = true;
  observed_fenced_frame_properties_ = properties;
}

bool FencedFrameURLMappingTestPeer::HasObserver(
    const GURL& urn_uuid,
    FencedFrameURLMapping::MappingResultObserver* observer) {
  return fenced_frame_url_mapping_->IsPendingMapped(urn_uuid) &&
         fenced_frame_url_mapping_->pending_urn_uuid_to_url_map_.at(urn_uuid)
             .count(observer);
}

void FencedFrameURLMappingTestPeer::GetSharedStorageReportingMap(
    const GURL& urn_uuid,
    SharedStorageReportingMap* out_reporting_map) {
  DCHECK(out_reporting_map);

  auto urn_it = fenced_frame_url_mapping_->urn_uuid_to_url_map_.find(urn_uuid);
  DCHECK(urn_it != fenced_frame_url_mapping_->urn_uuid_to_url_map_.end());

  scoped_refptr<FencedFrameReporter> fenced_frame_reporter =
      urn_it->second.fenced_frame_reporter_;
  if (!fenced_frame_reporter) {
    return;
  }

  const auto& metadata = fenced_frame_reporter->reporting_metadata();
  auto data_it = metadata.find(
      blink::FencedFrame::ReportingDestination::kSharedStorageSelectUrl);

  if (data_it != metadata.end()) {
    // No need to check if `reporting_url_map` is null - it never is for
    // kSharedStorageSelectUrl reporting destinations.
    *out_reporting_map = *data_it->second.reporting_url_map;
  }
}

void FencedFrameURLMappingTestPeer::FillMap(const GURL& url) {
  while (!fenced_frame_url_mapping_->IsFull()) {
    auto it = fenced_frame_url_mapping_->AddMappingForUrl(url);
    DCHECK(it.has_value());
  }

  DCHECK(fenced_frame_url_mapping_->IsFull());
}

bool WaitForFencedFrameSizeFreeze(RenderFrameHost* rfh) {
  // Currently we observed that the size freezing requires two
  // `requestAnimationFrame` calls to make sure it is completed. If only calling
  // `requestAnimationFrame` once, the test can still be flaky.
  return EvalJsAfterLifecycleUpdate(rfh, "", "").error.empty() &&
         EvalJsAfterLifecycleUpdate(rfh, "", "").error.empty();
}

}  // namespace content
