// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_
#define CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_

#include <memory>

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

  Request* GetActiveRequestForTesting() { return active_request_.get(); }

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

  std::unique_ptr<Request> active_request_;

  // Number of navigator.credentials.get() requests made for metrics purposes.
  // Requests made when there is a pending FedCM request or for the purpose of
  // Wallets or multi-IDP are not counted.
  int num_requests_{0};

  mojo::ReceiverSet<blink::mojom::FederatedRequestService> receivers_;
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_REQUEST_SERVICE_H_
