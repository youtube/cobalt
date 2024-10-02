// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/signals/system_signals_service_host_factory.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_signals/core/browser/file_system_signals_collector.h"
#include "components/device_signals/core/browser/settings_signals_collector.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/device_signals/core/browser/signals_aggregator_impl.h"
#include "components/device_signals/core/browser/signals_collector.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_MAC)
#include "components/device_signals/core/browser/mac/plist_settings_client.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "components/device_signals/core/browser/win/registry_settings_client.h"
#include "components/device_signals/core/browser/win/win_signals_collector.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "components/device_signals/core/browser/agent_signals_collector.h"
#include "components/device_signals/core/browser/crowdstrike_client.h"
#include "components/device_signals/core/browser/settings_client.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace enterprise_signals {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
std::unique_ptr<device_signals::SettingsClient> CreateSettingsClient() {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<device_signals::RegistrySettingsClient>();
#else
  return std::make_unique<device_signals::PlistSettingsClient>();
#endif  // BUILDFLAG(IS_WIN)
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// static
SignalsAggregatorFactory* SignalsAggregatorFactory::GetInstance() {
  return base::Singleton<SignalsAggregatorFactory>::get();
}

// static
device_signals::SignalsAggregator* SignalsAggregatorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<device_signals::SignalsAggregator*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SignalsAggregatorFactory::SignalsAggregatorFactory()
    : ProfileKeyedServiceFactory(
          "SignalsAggregator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(SystemSignalsServiceHostFactory::GetInstance());
  DependsOn(UserPermissionServiceFactory::GetInstance());
}

SignalsAggregatorFactory::~SignalsAggregatorFactory() = default;

KeyedService* SignalsAggregatorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  auto* user_permission_service =
      UserPermissionServiceFactory::GetForProfile(profile);

  std::vector<std::unique_ptr<device_signals::SignalsCollector>> collectors;
  auto* service_host = SystemSignalsServiceHostFactory::GetForProfile(profile);
  collectors.push_back(
      std::make_unique<device_signals::FileSystemSignalsCollector>(
          service_host));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  collectors.push_back(
      std::make_unique<device_signals::SettingsSignalsCollector>(
          CreateSettingsClient()));
  collectors.push_back(std::make_unique<device_signals::AgentSignalsCollector>(
      device_signals::CrowdStrikeClient::Create()));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  collectors.push_back(
      std::make_unique<device_signals::WinSignalsCollector>(service_host));
#endif  // BUILDFLAG(IS_WIN)

  return new device_signals::SignalsAggregatorImpl(user_permission_service,
                                                   std::move(collectors));
}

}  // namespace enterprise_signals
