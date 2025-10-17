// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_

#import <memory>

#import "base/memory/ref_counted.h"
#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class KeywordWebDataService;
class ProfileIOS;
enum class ServiceAccessType;
class TokenWebData;
class WebDataServiceWrapper;

namespace autofill {
class AutofillWebDataService;
}

namespace plus_addresses {
class PlusAddressWebDataService;
}

namespace ios {
// Singleton that owns all WebDataServiceWrappers and associates them with
// ProfileIOS.
class WebDataServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  // Returns the AutofillWebDataService associated with `profile`.
  static WebDataServiceWrapper* GetForProfile(ProfileIOS* profile,
                                              ServiceAccessType access_type);
  static WebDataServiceWrapper* GetForProfileIfExists(
      ProfileIOS* profile,
      ServiceAccessType access_type);

  // Returns the AutofillWebDataService associated with `profile`.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForProfile(ProfileIOS* profile,
                               ServiceAccessType access_type);

  // Returns the account-scoped AutofillWebDataService associated with the
  // `profile`.
  static scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForAccount(ProfileIOS* profile,
                               ServiceAccessType access_type);

  // Returns the KeywordWebDataService associated with `profile`.
  static scoped_refptr<KeywordWebDataService> GetKeywordWebDataForProfile(
      ProfileIOS* profile,
      ServiceAccessType access_type);

  // Returns the PlusAddressWebDataService associated with `profile`.
  static scoped_refptr<plus_addresses::PlusAddressWebDataService>
  GetPlusAddressWebDataForProfile(ProfileIOS* profile,
                                  ServiceAccessType access_type);

  // Returns the TokenWebData associated with `profile`.
  static scoped_refptr<TokenWebData> GetTokenWebDataForProfile(
      ProfileIOS* profile,
      ServiceAccessType access_type);

  static WebDataServiceFactory* GetInstance();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend class base::NoDestructor<WebDataServiceFactory>;

  WebDataServiceFactory();
  ~WebDataServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_WEBDATA_SERVICES_MODEL_WEB_DATA_SERVICE_FACTORY_H_
