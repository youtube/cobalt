// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/system_signals_service_host_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/system_signals/public/cpp/browser/system_signals_service_host_impl.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace enterprise_signals {

// static
SystemSignalsServiceHostFactory*
SystemSignalsServiceHostFactory::GetInstance() {
  return base::Singleton<SystemSignalsServiceHostFactory>::get();
}

// static
device_signals::SystemSignalsServiceHost*
SystemSignalsServiceHostFactory::GetForProfile(Profile* profile) {
  return static_cast<device_signals::SystemSignalsServiceHost*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SystemSignalsServiceHostFactory::SystemSignalsServiceHostFactory()
    : ProfileKeyedServiceFactory(
          "SystemSignalsServiceHost",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

SystemSignalsServiceHostFactory::~SystemSignalsServiceHostFactory() = default;

KeyedService* SystemSignalsServiceHostFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new system_signals::SystemSignalsServiceHostImpl();
}

}  // namespace enterprise_signals
