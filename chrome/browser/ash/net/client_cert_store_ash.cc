// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/client_cert_store_ash.h"

#include <cert.h>
#include <algorithm>
#include <iterator>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/net/client_cert_filter.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"

namespace ash {

ClientCertStoreAsh::ClientCertStoreAsh(
    std::unique_ptr<chromeos::CertificateProvider> cert_provider,
    bool use_system_slot,
    const std::string& username_hash,
    const PasswordDelegateFactory& password_delegate_factory)
    : cert_provider_(std::move(cert_provider)),
      cert_filter_(use_system_slot, username_hash) {}

ClientCertStoreAsh::~ClientCertStoreAsh() {}

void ClientCertStoreAsh::GetClientCerts(
    const net::SSLCertRequestInfo& cert_request_info,
    ClientCertListCallback callback) {
  // Caller is responsible for keeping the ClientCertStore alive until the
  // callback is run.
  base::OnceCallback<void(net::ClientCertIdentityList)>
      get_platform_certs_and_filter = base::BindOnce(
          &ClientCertStoreAsh::GotAdditionalCerts, base::Unretained(this),
          base::Unretained(&cert_request_info), std::move(callback));

  base::OnceClosure get_additional_certs_and_continue;
  if (cert_provider_) {
    get_additional_certs_and_continue =
        base::BindOnce(&chromeos::CertificateProvider::GetCertificates,
                       base::Unretained(cert_provider_.get()),
                       std::move(get_platform_certs_and_filter));
  } else {
    get_additional_certs_and_continue =
        base::BindOnce(std::move(get_platform_certs_and_filter),
                       net::ClientCertIdentityList());
  }

  auto split_callback =
      base::SplitOnceCallback(std::move(get_additional_certs_and_continue));
  if (cert_filter_.Init(std::move(split_callback.first))) {
    std::move(split_callback.second).Run();
  }
}

void ClientCertStoreAsh::GotAdditionalCerts(
    const net::SSLCertRequestInfo* request,
    ClientCertListCallback callback,
    net::ClientCertIdentityList additional_certs) {
  scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate> password_delegate;
  if (!password_delegate_factory_.is_null())
    password_delegate = password_delegate_factory_.Run(request->host_and_port);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ClientCertStoreAsh::GetAndFilterCertsOnWorkerThread,
                     base::Unretained(this), password_delegate,
                     base::Unretained(request), std::move(additional_certs)),
      std::move(callback));
}

net::ClientCertIdentityList ClientCertStoreAsh::GetAndFilterCertsOnWorkerThread(
    scoped_refptr<crypto::CryptoModuleBlockingPasswordDelegate>
        password_delegate,
    const net::SSLCertRequestInfo* request,
    net::ClientCertIdentityList additional_certs) {
  // This method may acquire the NSS lock or reenter this code via extension
  // hooks (such as smart card UI). To ensure threads are not starved or
  // deadlocked, the base::ScopedBlockingCall below increments the thread pool
  // capacity if this method takes too much time to run.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  net::ClientCertIdentityList client_certs;
  net::ClientCertStoreNSS::GetPlatformCertsOnWorkerThread(
      std::move(password_delegate),
      base::BindRepeating(&ClientCertFilter::IsCertAllowed,
                          base::Unretained(&cert_filter_)),
      &client_certs);

  client_certs.reserve(client_certs.size() + additional_certs.size());
  for (std::unique_ptr<net::ClientCertIdentity>& cert : additional_certs)
    client_certs.push_back(std::move(cert));
  net::ClientCertStoreNSS::FilterCertsOnWorkerThread(&client_certs, *request);
  return client_certs;
}

}  // namespace ash
