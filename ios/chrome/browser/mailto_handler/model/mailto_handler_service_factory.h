// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class MailtoHandlerService;
class ProfileIOS;

// Singleton that owns all MailtoHandlerServices and associates them with
// profiles.
class MailtoHandlerServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static MailtoHandlerService* GetForProfile(ProfileIOS* profile);
  static MailtoHandlerServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<MailtoHandlerServiceFactory>;

  MailtoHandlerServiceFactory();
  ~MailtoHandlerServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_MAILTO_HANDLER_MODEL_MAILTO_HANDLER_SERVICE_FACTORY_H_
