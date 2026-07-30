// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_
#define CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webid/config_fetcher.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

class RenderFrameHost;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityAutoReauthnPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;

namespace webid {

class Request;
class IdentityRegistry;
class IdpRegistrationHandler;
class IdpNetworkRequestManager;
class UserInfoRequest;
class DisconnectRequest;

// RequestService is a document-scoped manager class that coordinates
// Federated Credential Management (FedCM) requests for a given RenderFrameHost.
// It owns the active Request session.
class CONTENT_EXPORT RequestService
    : public DocumentUserData<RequestService>,
      public blink::mojom::FederatedRequestService {
 public:
  DOCUMENT_USER_DATA_KEY_DECL();

  explicit RequestService(RenderFrameHost* rfh);
  ~RequestService() override;

  RequestService(const RequestService&) = delete;
  RequestService& operator=(const RequestService&) = delete;

  // Binds a new receiver to a request session.
  void BindFederatedAuthRequest(
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver);

  void BindFederatedRequestService(
      mojo::PendingReceiver<blink::mojom::FederatedRequestService> receiver);

  // blink::mojom::FederatedRequestService:
  void RequestUserInfo(blink::mojom::IdentityProviderConfigPtr provider,
                       RequestUserInfoCallback callback) override;
  void RegisterIdP(const GURL& idp, RegisterIdPCallback callback) override;
  void UnregisterIdP(const GURL& idp, UnregisterIdPCallback callback) override;
  void PreventSilentAccess(PreventSilentAccessCallback callback) override;
  void Disconnect(blink::mojom::IdentityCredentialDisconnectOptionsPtr options,
                  DisconnectCallback callback) override;

  Request* GetActiveRequestForTesting() { return active_request_.get(); }

  base::WeakPtr<RequestService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void SetNetworkManagerForTests(
      std::unique_ptr<IdpNetworkRequestManager> manager);
  std::unique_ptr<IdpNetworkRequestManager> CreateNetworkManager();

  // Returns the active Request if one exists, or instantiates a new one if not.
  Request* GetOrCreateActiveRequest();

  // Creates a Request for testing.
  Request& CreateRequestForTesting(
      mojo::PendingReceiver<blink::mojom::FederatedAuthRequest> receiver,
      FederatedIdentityApiPermissionContextDelegate* api_permission_delegate,
      FederatedIdentityAutoReauthnPermissionContextDelegate*
          auto_reauthn_permission_delegate,
      FederatedIdentityPermissionContextDelegate* permission_delegate,
      IdentityRegistry* identity_registry);

  // Called by Request when it has completed or finished.
  void OnRequestDestroyed(Request* request);

  void IncrementNumRequests() { ++num_requests_; }

 private:
  friend class DocumentUserData<RequestService>;
  friend class Request;
  friend class RequestTest;
  friend class RequestRegistryTest;

  std::unique_ptr<Request> active_request_;

  // Number of navigator.credentials.get() requests made for metrics purposes.
  // Requests made when there is a pending FedCM request or for the purpose of
  // Wallets or multi-IDP are not counted.
  int num_requests_{0};

  mojo::ReceiverSet<blink::mojom::FederatedRequestService> receivers_;

  void SetRequiresUserMediation(bool requires_user_mediation,
                                base::OnceClosure callback);
  void OnIdpRegistrationConfigFetched(
      RegisterIdPCallback callback,
      const GURL& idp,
      std::vector<ConfigFetcher::FetchResult> fetch_results);
  void CompleteUserInfoRequest(
      UserInfoRequest* request,
      RequestUserInfoCallback callback,
      blink::mojom::RequestUserInfoStatus status,
      std::optional<std::vector<blink::mojom::IdentityUserInfoPtr>> user_info);
  void CompleteDisconnectRequest(DisconnectCallback callback,
                                 blink::mojom::DisconnectStatus status);
  std::unique_ptr<Metrics> CreateFedCmMetrics();

  std::unique_ptr<IdpNetworkRequestManager> registration_network_manager_;
  std::unique_ptr<IdpRegistrationHandler> fedcm_idp_registration_handler_;
  std::unique_ptr<IdpNetworkRequestManager> mock_network_manager_;
  base::flat_set<std::unique_ptr<UserInfoRequest>, base::UniquePtrComparator>
      user_info_requests_;

  raw_ptr<FederatedIdentityApiPermissionContextDelegate>
      api_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityAutoReauthnPermissionContextDelegate>
      auto_reauthn_permission_delegate_ = nullptr;
  raw_ptr<FederatedIdentityPermissionContextDelegate> permission_delegate_ =
      nullptr;
  std::unique_ptr<DisconnectRequest> disconnect_request_;

  base::WeakPtrFactory<RequestService> weak_ptr_factory_{this};
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_
