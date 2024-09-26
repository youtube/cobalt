// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/storage_manager/arc_storage_manager.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"

namespace arc {
namespace {

// Singleton factory for ArcStorageManager.
class ArcStorageManagerFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcStorageManager,
          ArcStorageManagerFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcStorageManagerFactory";

  static ArcStorageManagerFactory* GetInstance() {
    return base::Singleton<ArcStorageManagerFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcStorageManagerFactory>;
  ArcStorageManagerFactory() = default;
  ~ArcStorageManagerFactory() override = default;
};

}  // namespace

// static
ArcStorageManager* ArcStorageManager::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcStorageManagerFactory::GetForBrowserContext(context);
}

// static
ArcStorageManager* ArcStorageManager::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcStorageManagerFactory::GetForBrowserContextForTesting(context);
}

ArcStorageManager::ArcStorageManager(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {}

ArcStorageManager::~ArcStorageManager() = default;

bool ArcStorageManager::OpenPrivateVolumeSettings() {
  auto* storage_manager_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->storage_manager(), OpenPrivateVolumeSettings);
  if (!storage_manager_instance)
    return false;
  storage_manager_instance->OpenPrivateVolumeSettings();
  return true;
}

bool ArcStorageManager::GetApplicationsSize(
    GetApplicationsSizeCallback callback) {
  auto* storage_manager_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->storage_manager(), GetApplicationsSize);
  if (!storage_manager_instance)
    return false;
  storage_manager_instance->GetApplicationsSize(std::move(callback));
  return true;
}

// static
void ArcStorageManager::EnsureFactoryBuilt() {
  ArcStorageManagerFactory::GetInstance();
}

}  // namespace arc
