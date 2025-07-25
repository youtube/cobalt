// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_service_factory.h"

#import <utility>

#import "base/functional/callback_helpers.h"
#import "base/no_destructor.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/base/model_type.h"
#import "components/sync/base/sync_util.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_impl.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_profile_password_store_factory.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_device_info_sync_service_factory.h"
#import "ios/web_view/internal/sync/web_view_gcm_profile_service_factory.h"
#import "ios/web_view/internal/sync/web_view_model_type_store_service_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_client.h"
#import "ios/web_view/internal/sync/web_view_sync_invalidations_service_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/webdata_services/web_view_web_data_service_wrapper_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "url/gurl.h"

namespace ios_web_view {

// static
WebViewSyncServiceFactory* WebViewSyncServiceFactory::GetInstance() {
  static base::NoDestructor<WebViewSyncServiceFactory> instance;
  return instance.get();
}

// static
syncer::SyncService* WebViewSyncServiceFactory::GetForBrowserState(
    WebViewBrowserState* browser_state) {
  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

WebViewSyncServiceFactory::WebViewSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SyncService",
          BrowserStateDependencyManager::GetInstance()) {
  // The SyncService depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order.
  DependsOn(WebViewDeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(WebViewIdentityManagerFactory::GetInstance());
  DependsOn(WebViewWebDataServiceWrapperFactory::GetInstance());
  DependsOn(WebViewAccountPasswordStoreFactory::GetInstance());
  DependsOn(WebViewProfilePasswordStoreFactory::GetInstance());
  DependsOn(WebViewGCMProfileServiceFactory::GetInstance());
  DependsOn(WebViewModelTypeStoreServiceFactory::GetInstance());
  DependsOn(WebViewSyncInvalidationsServiceFactory::GetInstance());
}

WebViewSyncServiceFactory::~WebViewSyncServiceFactory() {}

std::unique_ptr<KeyedService>
WebViewSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  WebViewBrowserState* browser_state =
      WebViewBrowserState::FromBrowserState(context);

  signin::IdentityManager* identity_manager =
      WebViewIdentityManagerFactory::GetForBrowserState(browser_state);
  WebViewGCMProfileServiceFactory::GetForBrowserState(browser_state);

  syncer::SyncServiceImpl::InitParams init_params;
  init_params.identity_manager = identity_manager;
  init_params.sync_client = WebViewSyncClient::Create(browser_state);
  init_params.url_loader_factory = browser_state->GetSharedURLLoaderFactory();
  init_params.network_connection_tracker =
      ApplicationContext::GetInstance()->GetNetworkConnectionTracker();
  init_params.channel = version_info::Channel::STABLE;

  auto sync_service =
      std::make_unique<syncer::SyncServiceImpl>(std::move(init_params));
  sync_service->Initialize();

  return sync_service;
}

}  // namespace ios_web_view
