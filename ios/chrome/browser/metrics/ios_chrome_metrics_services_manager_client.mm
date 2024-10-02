// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/ios_chrome_metrics_services_manager_client.h"

#import <string>

#import "base/check.h"
#import "base/command_line.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/path_service.h"
#import "components/metrics/enabled_state_provider.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#import "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"
#import "ios/chrome/browser/paths/paths.h"
#import "ios/chrome/browser/variations/ios_chrome_variations_service_client.h"
#import "ios/chrome/browser/variations/ios_ui_string_overrider_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class IOSChromeMetricsServicesManagerClient::IOSChromeEnabledStateProvider
    : public metrics::EnabledStateProvider {
 public:
  IOSChromeEnabledStateProvider() {}

  IOSChromeEnabledStateProvider(const IOSChromeEnabledStateProvider&) = delete;
  IOSChromeEnabledStateProvider& operator=(
      const IOSChromeEnabledStateProvider&) = delete;

  ~IOSChromeEnabledStateProvider() override {}

  bool IsConsentGiven() const override {
    return IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  }
};

IOSChromeMetricsServicesManagerClient::IOSChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : enabled_state_provider_(
          std::make_unique<IOSChromeEnabledStateProvider>()),
      local_state_(local_state) {
  DCHECK(local_state);
}

IOSChromeMetricsServicesManagerClient::
    ~IOSChromeMetricsServicesManagerClient() = default;

std::unique_ptr<variations::VariationsService>
IOSChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // NOTE: On iOS, disabling background networking is not supported, so pass in
  // a dummy value for the name of the switch that disables background
  // networking.
  return variations::VariationsService::Create(
      std::make_unique<IOSChromeVariationsServiceClient>(), local_state_,
      GetMetricsStateManager(), "dummy-disable-background-switch",
      ::CreateUIStringOverrider(),
      base::BindOnce(&ApplicationContext::GetNetworkConnectionTracker,
                     base::Unretained(GetApplicationContext())));
}

std::unique_ptr<metrics::MetricsServiceClient>
IOSChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return IOSChromeMetricsServiceClient::Create(GetMetricsStateManager());
}

metrics::MetricsStateManager*
IOSChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_state_manager_) {
    base::FilePath user_data_dir;
    base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_, enabled_state_provider_.get(), std::wstring(),
        user_data_dir, metrics::StartupVisibility::kUnknown);
  }
  return metrics_state_manager_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeMetricsServicesManagerClient::GetURLLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

bool IOSChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

bool IOSChromeMetricsServicesManagerClient::IsMetricsConsentGiven() {
  return enabled_state_provider_->IsConsentGiven();
}

bool IOSChromeMetricsServicesManagerClient::IsOffTheRecordSessionActive() {
  return AreIncognitoTabsPresent();
}

// static
bool IOSChromeMetricsServicesManagerClient::AreIncognitoTabsPresent() {
  std::vector<ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  for (ChromeBrowserState* browser_state : browser_states) {
    BrowserList* browser_list =
        BrowserListFactory::GetForBrowserState(browser_state);
    for (Browser* browser : browser_list->AllIncognitoBrowsers()) {
      if (!browser->GetWebStateList()->empty()) {
        return true;
      }
    }
  }
  return false;
}
