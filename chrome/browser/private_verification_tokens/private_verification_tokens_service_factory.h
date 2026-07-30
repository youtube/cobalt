// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class PrivateVerificationTokensService;

class PrivateVerificationTokensServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static PrivateVerificationTokensService* GetForProfile(Profile* profile);
  static PrivateVerificationTokensServiceFactory* GetInstance();

  PrivateVerificationTokensServiceFactory(
      const PrivateVerificationTokensServiceFactory&) = delete;
  PrivateVerificationTokensServiceFactory& operator=(
      const PrivateVerificationTokensServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PrivateVerificationTokensServiceFactory>;

  static ProfileSelections CreateProfileSelections();

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  PrivateVerificationTokensServiceFactory();
  ~PrivateVerificationTokensServiceFactory() override;
};

#endif  // CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_SERVICE_FACTORY_H_
