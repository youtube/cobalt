// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/model_type_store_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

base::LazyInstance<SyncedPrintersManagerFactory>::DestructorAtExit
    g_printers_manager = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
SyncedPrintersManager* SyncedPrintersManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SyncedPrintersManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SyncedPrintersManagerFactory* SyncedPrintersManagerFactory::GetInstance() {
  return g_printers_manager.Pointer();
}

SyncedPrintersManagerFactory::SyncedPrintersManagerFactory()
    : ProfileKeyedServiceFactory(
          "SyncedPrintersManager",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

SyncedPrintersManagerFactory::~SyncedPrintersManagerFactory() = default;

SyncedPrintersManager* SyncedPrintersManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();

  std::unique_ptr<PrintersSyncBridge> sync_bridge =
      std::make_unique<PrintersSyncBridge>(
          std::move(store_factory),
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel()));

  return SyncedPrintersManager::Create(std::move(sync_bridge)).release();
}

}  // namespace ash
