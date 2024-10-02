// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/security_events/security_event_recorder_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/time/default_clock.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/security_events/security_event_recorder_impl.h"
#include "chrome/browser/security_events/security_event_sync_bridge.h"
#include "chrome/browser/security_events/security_event_sync_bridge_impl.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"

// static
SecurityEventRecorderFactory* SecurityEventRecorderFactory::GetInstance() {
  return base::Singleton<SecurityEventRecorderFactory>::get();
}

// static
SecurityEventRecorder* SecurityEventRecorderFactory::GetForProfile(
    Profile* profile) {
  DCHECK(!profile->IsOffTheRecord());
  return static_cast<SecurityEventRecorder*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}
SecurityEventRecorderFactory::SecurityEventRecorderFactory()
    : ProfileKeyedServiceFactory(
          "SecurityEventRecorder",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

SecurityEventRecorderFactory::~SecurityEventRecorderFactory() {}

KeyedService* SecurityEventRecorderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  auto change_processor =
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::SECURITY_EVENTS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));
  auto security_event_sync_bridge =
      std::make_unique<SecurityEventSyncBridgeImpl>(
          std::move(store_factory), std::move(change_processor));
  return new SecurityEventRecorderImpl(std::move(security_event_sync_bridge),
                                       base::DefaultClock::GetInstance());
}
