// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/indexed_db_client_state_checker_factory.h"

#include <memory>

#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom-shared.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {
namespace {

using IndexedDBDisallowActivationReason =
    storage::mojom::DisallowInactiveClientReason;

DisallowActivationReasonId ConvertToDisallowActivationReasonId(
    IndexedDBDisallowActivationReason reason) {
  switch (reason) {
    case IndexedDBDisallowActivationReason::kClientEventIsTriggered:
      return DisallowActivationReasonId::kIndexedDBEvent;
    case IndexedDBDisallowActivationReason::kTransactionIsAcquiringLocks:
      return DisallowActivationReasonId::kIndexedDBTransactionIsAcquiringLocks;
    case IndexedDBDisallowActivationReason::kTransactionIsBlockingOthers:
      return DisallowActivationReasonId::kIndexedDBTransactionIsBlockingOthers;
  }
}

// The class will only provide the default result and the client will be
// considered active. It should be used when the client doesn't have an
// associated RenderFrameHost, as is the case for shared worker or service
// worker.
class NoDocumentIndexedDBClientStateChecker
    : public storage::mojom::IndexedDBClientStateChecker {
 public:
  NoDocumentIndexedDBClientStateChecker() = default;
  ~NoDocumentIndexedDBClientStateChecker() override = default;
  NoDocumentIndexedDBClientStateChecker(
      const NoDocumentIndexedDBClientStateChecker&) = delete;
  NoDocumentIndexedDBClientStateChecker& operator=(
      const NoDocumentIndexedDBClientStateChecker&) = delete;

  // storage::mojom::IndexedDBClientStateChecker overrides:
  // Non-document clients are always active, since the inactive state such as
  // back/forward cache is not applicable to them.
  void DisallowInactiveClient(
      storage::mojom::DisallowInactiveClientReason reason,
      mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
          keep_active,
      DisallowInactiveClientCallback callback) override {
    std::move(callback).Run(/*was_active=*/true);
  }
};

// This class should be used when the client has a RenderFrameHost associated so
// the client checks are performed based on the document held by the
// RenderFrameHost.
// This class extends `DocumentUserData` because a document has one client per
// IndexedDB connection to a database.
class DocumentIndexedDBClientStateChecker final
    : public DocumentUserData<DocumentIndexedDBClientStateChecker>,
      public storage::mojom::IndexedDBClientStateChecker,
      public storage::mojom::IndexedDBClientKeepActive {
 public:
  ~DocumentIndexedDBClientStateChecker() final = default;

  void Bind(mojo::PendingAssociatedReceiver<
            storage::mojom::IndexedDBClientStateChecker> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  bool CheckIfClientWasActive(
      storage::mojom::DisallowInactiveClientReason reason) {
    bool was_active = false;

    if (render_frame_host().GetLifecycleState() ==
        RenderFrameHost::LifecycleState::kPrerendering) {
      // Page under prerendering is able to continue the JS execution so it
      // won't block the IndexedDB events. It shouldn't be deemed inactive for
      // the IndexedDB service.
      was_active = true;
    } else {
      // Call `IsInactiveAndDisallowActivation` to obtain the client state, this
      // also brings side effect like evicting the page if it's in back/forward
      // cache.
      was_active = !render_frame_host().IsInactiveAndDisallowActivation(
          ConvertToDisallowActivationReasonId(reason));
    }

    return was_active;
  }

  // storage::mojom::IndexedDBClientStateChecker overrides:
  void DisallowInactiveClient(
      storage::mojom::DisallowInactiveClientReason reason,
      mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
          keep_active,
      DisallowInactiveClientCallback callback) override {
    bool was_active = CheckIfClientWasActive(reason);
    if (was_active && keep_active.is_valid()) {
      // This is the only reason that we need to prevent the client from
      // inactive state.
      CHECK_EQ(reason, storage::mojom::DisallowInactiveClientReason::
                           kClientEventIsTriggered);
      // If the document is active, we need to register a non sticky feature to
      // prevent putting it into BFCache until the IndexedDB connection is
      // successfully closed and the context is automatically destroyed.
      // Since `kClientEventIsTriggered` is the only reason that should be
      // passed to this function, the non-sticky feature will always be
      // `kIndexedDBEvent`.
      KeepActiveReceiverContext context(
          static_cast<RenderFrameHostImpl&>(render_frame_host())
              .RegisterBackForwardCacheDisablingNonStickyFeature(
                  blink::scheduler::WebSchedulerTrackedFeature::
                      kIndexedDBEvent));
      keep_active_receivers_.Add(this, std::move(keep_active),
                                 std::move(context));
    }

    std::move(callback).Run(was_active);
  }

 private:
  // Keep the association between the receiver and the feature handle it
  // registered.
  class KeepActiveReceiverContext {
   public:
    KeepActiveReceiverContext() = default;
    explicit KeepActiveReceiverContext(
        RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle handle)
        : feature_handle(std::move(handle)) {}
    KeepActiveReceiverContext(KeepActiveReceiverContext&& context) noexcept
        : feature_handle(std::move(context.feature_handle)) {}

    ~KeepActiveReceiverContext() = default;

   private:
    RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle feature_handle;
  };

  explicit DocumentIndexedDBClientStateChecker(RenderFrameHost* rfh)
      : DocumentUserData(rfh) {}

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::AssociatedReceiverSet<storage::mojom::IndexedDBClientStateChecker>
      receivers_;
  mojo::ReceiverSet<storage::mojom::IndexedDBClientKeepActive,
                    KeepActiveReceiverContext>
      keep_active_receivers_;
};

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(DocumentIndexedDBClientStateChecker);

// static
mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
IndexedDBClientStateCheckerFactory::InitializePendingAssociatedRemote(
    const GlobalRenderFrameHostId& rfh_id) {
  mojo::PendingAssociatedRemote<storage::mojom::IndexedDBClientStateChecker>
      client_state_checker_remote;
  if (RenderFrameHost* rfh = RenderFrameHost::FromID(rfh_id)) {
    DocumentIndexedDBClientStateChecker::GetOrCreateForCurrentDocument(rfh)
        ->Bind(
            client_state_checker_remote.InitWithNewEndpointAndPassReceiver());
  } else {
    // If the `rfh` is null, it means there is actually no valid
    // `RenderFrameHost` associated with the client. We should use a default
    // checker instance for it.
    // See comments from `NoDocumentIndexedDBClientStateChecker`.
    mojo::MakeSelfOwnedAssociatedReceiver(
        std::make_unique<NoDocumentIndexedDBClientStateChecker>(),
        client_state_checker_remote.InitWithNewEndpointAndPassReceiver());
  }

  return client_state_checker_remote;
}

// static
storage::mojom::IndexedDBClientStateChecker*
IndexedDBClientStateCheckerFactory::
    GetOrCreateIndexedDBClientStateCheckerForTesting(
        const GlobalRenderFrameHostId& rfh_id) {
  CHECK_NE(rfh_id.frame_routing_id, MSG_ROUTING_NONE)
      << "RFH id should be valid when testing";
  return DocumentIndexedDBClientStateChecker::GetOrCreateForCurrentDocument(
      RenderFrameHost::FromID(rfh_id));
}

}  // namespace content
