// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

class ChromeBrowserState;
enum class ServiceAccessType;

namespace password_manager {
class PasswordStoreInterface;
}

// Singleton that owns all Gaia-account-scoped PasswordStores and associates
// them with ChromeBrowserState.
class IOSChromeAccountPasswordStoreFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface>
  GetForBrowserState(ChromeBrowserState* browser_state,
                     ServiceAccessType access_type);

  static IOSChromeAccountPasswordStoreFactory* GetInstance();

  IOSChromeAccountPasswordStoreFactory(
      const IOSChromeAccountPasswordStoreFactory&) = delete;
  IOSChromeAccountPasswordStoreFactory& operator=(
      const IOSChromeAccountPasswordStoreFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromeAccountPasswordStoreFactory>;

  IOSChromeAccountPasswordStoreFactory();
  ~IOSChromeAccountPasswordStoreFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_ACCOUNT_PASSWORD_STORE_FACTORY_H_
