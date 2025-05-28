// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/test_shared_storage_runtime_manager.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/test_shared_storage_worklet_host.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"

namespace content {

TestSharedStorageRuntimeManager::~TestSharedStorageRuntimeManager() = default;

std::unique_ptr<SharedStorageWorkletHost>
TestSharedStorageRuntimeManager::CreateWorkletHostHelper(
    SharedStorageDocumentServiceImpl& document_service,
    const url::Origin& frame_origin,
    const url::Origin& data_origin,
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::SharedStorageWorkletCreationMethod creation_method,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
        callback) {
  return std::make_unique<TestSharedStorageWorkletHost>(
      document_service, frame_origin, data_origin, script_source_url,
      credentials_mode, creation_method, origin_trial_features,
      std::move(worklet_host), std::move(callback),
      should_defer_worklet_messages_);
}

// Precondition: there's only one eligible worklet host.
TestSharedStorageWorkletHost*
TestSharedStorageRuntimeManager::GetAttachedWorkletHost() {
  std::vector<TestSharedStorageWorkletHost*> worklet_hosts =
      GetAttachedWorkletHosts();

  DCHECK_EQ(worklet_hosts.size(), 1u);
  return worklet_hosts[0];
}

std::vector<TestSharedStorageWorkletHost*>
TestSharedStorageRuntimeManager::GetAttachedWorkletHosts() {
  std::vector<TestSharedStorageWorkletHost*> results;
  for (auto& [document_service, worklet_hosts] :
       GetAttachedWorkletHostsForTesting()) {
    for (auto& [raw_worklet_host, worklet_host] : worklet_hosts) {
      results.push_back(
          static_cast<TestSharedStorageWorkletHost*>(raw_worklet_host));
    }
  }

  return results;
}

TestSharedStorageWorkletHost*
TestSharedStorageRuntimeManager::GetKeepAliveWorkletHost() {
  DCHECK_EQ(1u, GetKeepAliveWorkletHostsCount());
  return static_cast<TestSharedStorageWorkletHost*>(
      GetKeepAliveWorkletHostsForTesting().begin()->second.get());
}

TestSharedStorageWorkletHost*
TestSharedStorageRuntimeManager::GetAttachedWorkletHostForFrame(
    RenderFrameHost* frame) {
  std::vector<TestSharedStorageWorkletHost*> worklet_hosts =
      GetAttachedWorkletHostsForFrame(frame);

  DCHECK_EQ(worklet_hosts.size(), 1u);
  return worklet_hosts[0];
}

std::vector<TestSharedStorageWorkletHost*>
TestSharedStorageRuntimeManager::GetAttachedWorkletHostsForFrame(
    RenderFrameHost* frame) {
  SharedStorageDocumentServiceImpl* document_service =
      DocumentUserData<SharedStorageDocumentServiceImpl>::GetForCurrentDocument(
          frame);
  DCHECK(document_service);

  WorkletHosts& worklet_hosts =
      GetAttachedWorkletHostsForTesting().at(document_service);

  std::vector<TestSharedStorageWorkletHost*> results;
  for (auto& [raw_worklet_host, worklet_host] : worklet_hosts) {
    results.push_back(
        static_cast<TestSharedStorageWorkletHost*>(raw_worklet_host));
  }

  return results;
}

void TestSharedStorageRuntimeManager::
    ConfigureShouldDeferWorkletMessagesOnWorkletHostCreation(
        bool should_defer_worklet_messages) {
  should_defer_worklet_messages_ = should_defer_worklet_messages;
}

size_t TestSharedStorageRuntimeManager::GetAttachedWorkletHostsCount() {
  size_t count = 0;
  for (const auto& [document_service, worklet_hosts] :
       GetAttachedWorkletHostsForTesting()) {
    count += worklet_hosts.size();
  }

  return count;
}

size_t TestSharedStorageRuntimeManager::GetKeepAliveWorkletHostsCount() {
  return GetKeepAliveWorkletHostsForTesting().size();
}

}  // namespace content
