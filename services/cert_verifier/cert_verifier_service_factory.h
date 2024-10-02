// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_
#define SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/cert_verifier_creation.h"
#include "services/cert_verifier/cert_verifier_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "net/cert/internal/trust_store_chrome.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif

namespace cert_verifier {

// Implements mojom::CertVerifierServiceFactory, and calls
// network::CreateCertVerifier to instantiate the concrete net::CertVerifier
// used to service requests.
class CertVerifierServiceFactoryImpl
    : public mojom::CertVerifierServiceFactory {
 public:
  // Creates a CertVerifierServiceFactoryImpl.
  explicit CertVerifierServiceFactoryImpl(
      mojo::PendingReceiver<mojom::CertVerifierServiceFactory> receiver);
  ~CertVerifierServiceFactoryImpl() override;

  // mojom::CertVerifierServiceFactory implementation:
  void GetNewCertVerifier(
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
      mojom::CertVerifierCreationParamsPtr creation_params) override;

  // Performs the same function as above, but stores a ref to the new
  // CertNetFetcherURLLoader in |*cert_net_fetcher_ptr|, if the
  // CertNetFetcherURLLoader is in use.
  void GetNewCertVerifierForTesting(
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
      mojom::CertVerifierCreationParamsPtr creation_params,
      scoped_refptr<CertNetFetcherURLLoader>* cert_net_fetcher_ptr);

  // mojom::CertVerifierServiceFactory implementation:
  void UpdateCRLSet(mojo_base::BigBuffer crl_set,
                    mojom::CertVerifierServiceFactory::UpdateCRLSetCallback
                        callback) override;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  void UpdateChromeRootStore(mojom::ChromeRootStorePtr new_root_store,
                             UpdateChromeRootStoreCallback callback) override;
  void GetChromeRootStoreInfo(GetChromeRootStoreInfoCallback callback) override;
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  void SetUseChromeRootStore(bool use_crs,
                             SetUseChromeRootStoreCallback callback) override;
#endif

  // Remove a CertVerifyService from needing updates to the Chrome Root Store.
  void RemoveService(internal::CertVerifierServiceImpl* service_impl);

 private:
  // Update all the `verifier_services_` with the current data.
  void UpdateVerifierServices();

  void OnCRLSetParsed(scoped_refptr<net::CRLSet> parsed_crl_set);

  net::CertVerifyProcFactory::ImplParams proc_params_;

  mojo::Receiver<mojom::CertVerifierServiceFactory> receiver_;

  // Services that we might need to send updates to.
  std::set<raw_ptr<internal::CertVerifierServiceImpl>> verifier_services_;
  base::WeakPtrFactory<CertVerifierServiceFactoryImpl> weak_factory_{this};
};

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_VERIFIER_SERVICE_FACTORY_H_
