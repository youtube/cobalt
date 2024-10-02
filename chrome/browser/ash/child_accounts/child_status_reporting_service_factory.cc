// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service.h"

namespace ash {

// static
ChildStatusReportingService*
ChildStatusReportingServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChildStatusReportingService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChildStatusReportingServiceFactory*
ChildStatusReportingServiceFactory::GetInstance() {
  static base::NoDestructor<ChildStatusReportingServiceFactory> factory;
  return factory.get();
}

ChildStatusReportingServiceFactory::ChildStatusReportingServiceFactory()
    : ProfileKeyedServiceFactory(
          "ChildStatusReportingServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

ChildStatusReportingServiceFactory::~ChildStatusReportingServiceFactory() =
    default;

KeyedService* ChildStatusReportingServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChildStatusReportingService(context);
}

}  // namespace ash
