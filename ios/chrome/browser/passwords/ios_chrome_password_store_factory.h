// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_STORE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_STORE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

class ChromeBrowserState;
enum class ServiceAccessType;

namespace password_manager {
class PasswordStoreInterface;
}

// Singleton that owns all PasswordStores and associates them with
// ChromeBrowserState.
class IOSChromePasswordStoreFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface>
  GetForBrowserState(ChromeBrowserState* browser_state,
                     ServiceAccessType access_type);

  static IOSChromePasswordStoreFactory* GetInstance();

  IOSChromePasswordStoreFactory(const IOSChromePasswordStoreFactory&) = delete;
  IOSChromePasswordStoreFactory& operator=(
      const IOSChromePasswordStoreFactory&) = delete;

 private:
  friend class base::NoDestructor<IOSChromePasswordStoreFactory>;

  IOSChromePasswordStoreFactory();
  ~IOSChromePasswordStoreFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_STORE_FACTORY_H_
