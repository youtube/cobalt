// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "device/fido/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/trusted_vault/trusted_vault_client_android.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#else
#include "base/files/file_path.h"
#include "components/trusted_vault/standalone_trusted_vault_client.h"
#include "content/public/browser/storage_partition.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/trusted_vault/crosapi_trusted_vault_client.h"
#include "chromeos/crosapi/mojom/trusted_vault.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/trusted_vault/features.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<trusted_vault::TrustedVaultClient>
CreateChromeSyncStandaloneTrustedVaultClient(Profile* profile) {
  return std::make_unique<trusted_vault::StandaloneTrustedVaultClient>(
      trusted_vault::SecurityDomainId::kChromeSync,
      /*base_dir=*/profile->GetPath(),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
#endif

std::unique_ptr<trusted_vault::TrustedVaultClient>
CreateChromeSyncTrustedVaultClient(Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<
      TrustedVaultClientAndroid>(/*gaia_account_info_by_gaia_id_cb=*/
                                 base::BindRepeating(
                                     [](signin::IdentityManager*
                                            identity_manager,
                                        const std::string& gaia_id)
                                         -> CoreAccountInfo {
                                       return identity_manager
                                           ->FindExtendedAccountInfoByGaiaId(
                                               gaia_id);
                                     },
                                     IdentityManagerFactory::GetForProfile(
                                         profile)));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!base::FeatureList::IsEnabled(
          trusted_vault::kChromeOSTrustedVaultClientShared)) {
    return CreateChromeSyncStandaloneTrustedVaultClient(profile);
  }
  if (!profile->IsMainProfile()) {
    // Secondary Lacros profiles use standalone implementation.
    return CreateChromeSyncStandaloneTrustedVaultClient(profile);
  }

  auto* lacros_service = chromeos::LacrosService::Get();
  CHECK(lacros_service);
  if (!lacros_service->IsAvailable<crosapi::mojom::TrustedVaultBackend>()) {
    // TrustedVault Crosapi is not available, fallback to standalone
    // implementation.
    // TODO(crbug.com/1434667): this should be replaced CHECK() once it is not
    // possible to have Ash-side TrustedVault Crosapi disabled (two milestones
    // after kChromeOSTrustedVaultClientShared is guaranteed to be enabled in
    // Ash).
    return CreateChromeSyncStandaloneTrustedVaultClient(profile);
  }
  return std::make_unique<CrosapiTrustedVaultClient>(
      &lacros_service->GetRemote<crosapi::mojom::TrustedVaultBackend>());
#else
  return CreateChromeSyncStandaloneTrustedVaultClient(profile);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<trusted_vault::TrustedVaultClient>
CreatePasskeyTrustedVaultClient(Profile* profile) {
  return std::make_unique<trusted_vault::StandaloneTrustedVaultClient>(
      trusted_vault::SecurityDomainId::kPasskeys,
      /*base_dir=*/profile->GetPath(),
      IdentityManagerFactory::GetForProfile(profile),
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<KeyedService> BuildTrustedVaultService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(!profile->IsOffTheRecord());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(device::kChromeOsPasskeys)) {
    return std::make_unique<trusted_vault::TrustedVaultService>(
        CreateChromeSyncTrustedVaultClient(profile),
        CreatePasskeyTrustedVaultClient(profile));
  }
#endif

  return std::make_unique<trusted_vault::TrustedVaultService>(
      CreateChromeSyncTrustedVaultClient(profile));
}

}  // namespace

// static
trusted_vault::TrustedVaultService* TrustedVaultServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<trusted_vault::TrustedVaultService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
TrustedVaultServiceFactory* TrustedVaultServiceFactory::GetInstance() {
  static base::NoDestructor<TrustedVaultServiceFactory> instance;
  return instance.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
TrustedVaultServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildTrustedVaultService);
}

TrustedVaultServiceFactory::TrustedVaultServiceFactory()
    : ProfileKeyedServiceFactory(
          "TrustedVaultService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode. Currently it is required due to dependant services
              // (e.g. SyncService) that have similar TODO, if they stop being
              // used in Guest mode, this service could stop to be used as well.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

TrustedVaultServiceFactory::~TrustedVaultServiceFactory() = default;

KeyedService* TrustedVaultServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildTrustedVaultService(context).release();
}

bool TrustedVaultServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
