// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/browser_state.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/guid.h"
#import "base/location.h"
#import "base/memory/ref_counted.h"
#import "base/metrics/histogram_functions.h"
#import "base/process/process_handle.h"
#import "base/token.h"
#import "components/leveldb_proto/public/proto_database_provider.h"
#import "ios/web/public/init/network_context_owner.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/ui/wk_content_rule_list_provider.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "ios/web/webui/url_data_manager_ios_backend.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "net/url_request/url_request_context_getter.h"
#import "net/url_request/url_request_context_getter_observer.h"
#import "services/network/network_context.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {

// Private key used for safe conversion of base::SupportsUserData to
// web::BrowserState in web::BrowserState::FromSupportsUserData.
const char kBrowserStateIdentifierKey[] = "BrowserStateIdentifierKey";

// Data key names.
const char kCertificatePolicyCacheKeyName[] = "cert_policy_cache";

// Wraps a CertificatePolicyCache as a SupportsUserData::Data; this is necessary
// since reference counted objects can't be user data.
struct CertificatePolicyCacheHandle : public base::SupportsUserData::Data {
  explicit CertificatePolicyCacheHandle(CertificatePolicyCache* cache)
      : policy_cache(cache) {}

  scoped_refptr<CertificatePolicyCache> policy_cache;
};

}  // namespace

// static
scoped_refptr<CertificatePolicyCache> BrowserState::GetCertificatePolicyCache(
    BrowserState* browser_state) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  if (!browser_state->GetUserData(kCertificatePolicyCacheKeyName)) {
    browser_state->SetUserData(kCertificatePolicyCacheKeyName,
                               std::make_unique<CertificatePolicyCacheHandle>(
                                   new CertificatePolicyCache()));
  }

  CertificatePolicyCacheHandle* handle =
      static_cast<CertificatePolicyCacheHandle*>(
          browser_state->GetUserData(kCertificatePolicyCacheKeyName));
  return handle->policy_cache;
}

BrowserState::BrowserState() : url_data_manager_ios_backend_(nullptr) {
  // (Refcounted)?BrowserStateKeyedServiceFactories needs to be able to convert
  // a base::SupportsUserData to a BrowserState. Moreover, since the factories
  // may be passed a content::BrowserContext instead of a BrowserState, attach
  // an empty object to this via a private key.
  SetUserData(kBrowserStateIdentifierKey,
              std::make_unique<SupportsUserData::Data>());

  // Set up shared_url_loader_factory_ for lazy creation.
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          base::BindOnce(&BrowserState::GetURLLoaderFactory,
                         base::Unretained(this) /* safe due to Detach call */));
}

BrowserState::~BrowserState() {
  shared_url_loader_factory_->Detach();

  if (network_context_) {
    web::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, network_context_owner_.release());
  }

  // Delete the URLDataManagerIOSBackend instance on the IO thread if it has
  // been created. Note that while this check can theoretically race with a
  // call to `GetURLDataManagerIOSBackendOnIOThread()`, if any clients of this
  // BrowserState are still accessing it on the IO thread at this point,
  // they're going to have a bad time anyway.
  if (url_data_manager_ios_backend_) {
    bool posted = web::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, url_data_manager_ios_backend_);
    if (!posted)
      delete url_data_manager_ios_backend_;
  }
}

network::mojom::URLLoaderFactory* BrowserState::GetURLLoaderFactory() {
  if (!url_loader_factory_) {
    CreateNetworkContextIfNecessary();
    auto url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_corb_enabled = false;
    url_loader_factory_params->is_trusted = true;
    network_context_->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory_params));
  }

  return url_loader_factory_.get();
}

network::mojom::CookieManager* BrowserState::GetCookieManager() {
  if (!cookie_manager_) {
    CreateNetworkContextIfNecessary();
    network_context_->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());
  }
  return cookie_manager_.get();
}

leveldb_proto::ProtoDatabaseProvider* BrowserState::GetProtoDatabaseProvider() {
  if (!proto_database_provider_) {
    proto_database_provider_ =
        std::make_unique<leveldb_proto::ProtoDatabaseProvider>(GetStatePath());
  }
  return proto_database_provider_.get();
}

void BrowserState::GetProxyResolvingSocketFactory(
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  CreateNetworkContextIfNecessary();

  network_context_->CreateProxyResolvingSocketFactory(std::move(receiver));
}

scoped_refptr<network::SharedURLLoaderFactory>
BrowserState::GetSharedURLLoaderFactory() {
  return shared_url_loader_factory_;
}

URLDataManagerIOSBackend*
BrowserState::GetURLDataManagerIOSBackendOnIOThread() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (!url_data_manager_ios_backend_)
    url_data_manager_ios_backend_ = new URLDataManagerIOSBackend();
  return url_data_manager_ios_backend_;
}

void BrowserState::CreateNetworkContextIfNecessary() {
  if (network_context_owner_)
    return;

  DCHECK(!network_context_);

  net::URLRequestContextGetter* request_context = GetRequestContext();
  DCHECK(request_context);
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  UpdateCorsExemptHeader(network_context_params.get());
  network_context_owner_ = std::make_unique<NetworkContextOwner>(
      request_context, network_context_params->cors_exempt_header_list,
      &network_context_);
}

// static
BrowserState* BrowserState::FromSupportsUserData(
    base::SupportsUserData* supports_user_data) {
  if (!supports_user_data ||
      !supports_user_data->GetUserData(kBrowserStateIdentifierKey)) {
    return nullptr;
  }
  return static_cast<BrowserState*>(supports_user_data);
}

}  // namespace web
