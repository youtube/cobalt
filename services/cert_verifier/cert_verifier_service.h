// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_
#define SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/log/net_log_with_source.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace net {
class ChromeRootStoreData;
}

// Defines an implementation of a Cert Verifier Service to be queried by network
// service or others.
namespace cert_verifier {

class CertVerifierServiceFactoryImpl;

namespace internal {

// This class will delete itself upon disconnection of its Mojo receiver.
class CertVerifierServiceImpl : public mojom::CertVerifierService,
                                public net::CertVerifier::Observer {
 public:
  explicit CertVerifierServiceImpl(
      std::unique_ptr<net::CertVerifierWithUpdatableProc> verifier,
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
      scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher);

  // mojom::CertVerifierService implementation:
  void Verify(const net::CertVerifier::RequestParams& params,
              const net::NetLogSource& net_log_source,
              mojo::PendingRemote<mojom::CertVerifierRequest>
                  cert_verifier_request) override;
  void SetConfig(const net::CertVerifier::Config& config) override;
  void EnableNetworkAccess(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>,
      mojo::PendingRemote<mojom::URLLoaderFactoryConnector> reconnector)
      override;

  // Set a pointer to the CertVerifierServiceFactory so that it may be notified
  // when we are deleted.
  void SetCertVerifierServiceFactory(
      base::WeakPtr<cert_verifier::CertVerifierServiceFactoryImpl>
          service_factory_impl);

  // Update the wrapped verifier with CRLSet and ChromeRootStoreData.
  void UpdateVerifierData(
      const net::CertVerifyProcFactory::ImplParams& impl_params);

 private:
  ~CertVerifierServiceImpl() override;

  // CertVerifier::Observer methods:
  void OnCertVerifierChanged() override;

  void OnDisconnectFromService();

  std::unique_ptr<net::CertVerifierWithUpdatableProc> verifier_;
  mojo::Receiver<mojom::CertVerifierService> receiver_;
  mojo::Remote<mojom::CertVerifierServiceClient> client_;
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher_;
  base::WeakPtr<cert_verifier::CertVerifierServiceFactoryImpl>
      service_factory_impl_;
};

}  // namespace internal
}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_H_
