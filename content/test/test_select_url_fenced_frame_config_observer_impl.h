// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_IMPL_H_
#define CONTENT_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_IMPL_H_

#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"

class GURL;

namespace content {

class TestSelectURLFencedFrameConfigObserverImpl
    : public SharedStorageRuntimeManager::SharedStorageObserverInterface {
 public:
  using AccessScope = blink::SharedStorageAccessScope;

  TestSelectURLFencedFrameConfigObserverImpl();
  ~TestSelectURLFencedFrameConfigObserverImpl() override;

  GlobalRenderFrameHostId AssociatedFrameHostId() const override;
  bool ShouldReceiveAllSharedStorageReports() const override;

  void OnSharedStorageAccessed(base::Time access_time,
                               AccessScope scope,
                               AccessMethod method,
                               GlobalRenderFrameHostId main_frame_id,
                               const std::string& owner_origin,
                               const SharedStorageEventParams& params) override;
  void OnSharedStorageSelectUrlUrnUuidGenerated(const GURL& urn_uuid) override;
  void OnSharedStorageSelectUrlConfigPopulated(
      const std::optional<FencedFrameConfig>& config) override;

  void OnSharedStorageWorkletOperationExecutionFinished(
      base::Time finished_time,
      base::TimeDelta execution_time,
      AccessMethod method,
      int operation_id,
      int worklet_ordinal_id,
      const base::UnguessableToken& worklet_devtools_token,
      GlobalRenderFrameHostId main_frame_id,
      const std::string& owner_origin) override;

  const std::optional<GURL>& GetUrnUuid() const;
  const std::optional<FencedFrameConfig>& GetConfig() const;
  bool ConfigObserved() const;

 private:
  std::optional<GURL> urn_uuid_;
  std::optional<FencedFrameConfig> config_;
  bool config_observed_ = false;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_SELECT_URL_FENCED_FRAME_CONFIG_OBSERVER_IMPL_H_
