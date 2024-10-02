// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_validator.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_verifier.h"
#include "content/public/browser/isolated_web_apps_policy.h"

namespace web_app {

// static
IsolatedWebAppReaderRegistry*
IsolatedWebAppReaderRegistryFactory::GetForProfile(Profile* profile) {
  return static_cast<IsolatedWebAppReaderRegistry*>(
      IsolatedWebAppReaderRegistryFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
IsolatedWebAppReaderRegistryFactory*
IsolatedWebAppReaderRegistryFactory::GetInstance() {
  return base::Singleton<IsolatedWebAppReaderRegistryFactory>::get();
}

IsolatedWebAppReaderRegistryFactory::IsolatedWebAppReaderRegistryFactory()
    : BrowserContextKeyedServiceFactory(
          "IsolatedWebAppReaderRegistry",
          BrowserContextDependencyManager::GetInstance()) {}

IsolatedWebAppReaderRegistryFactory::~IsolatedWebAppReaderRegistryFactory() =
    default;

KeyedService* IsolatedWebAppReaderRegistryFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto isolated_web_app_trust_checker =
      std::make_unique<IsolatedWebAppTrustChecker>(
          CHECK_DEREF(profile->GetPrefs()));

  auto validator = std::make_unique<IsolatedWebAppValidator>(
      std::move(isolated_web_app_trust_checker));

  return new IsolatedWebAppReaderRegistry(
      std::move(validator), base::BindRepeating([]() {
        return std::make_unique<
            web_package::SignedWebBundleSignatureVerifier>();
      }));
}

content::BrowserContext*
IsolatedWebAppReaderRegistryFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(context)) {
    return nullptr;
  }

  return GetBrowserContextForWebApps(context);
}

}  // namespace web_app
