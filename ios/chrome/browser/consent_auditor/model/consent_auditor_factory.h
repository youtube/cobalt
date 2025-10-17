// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_
#define IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

// Singleton that owns all ConsentAuditors and associates them with
// ProfileIOS.
class ConsentAuditorFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static consent_auditor::ConsentAuditor* GetForProfile(ProfileIOS* profile);
  static ConsentAuditorFactory* GetInstance();

 private:
  friend class base::NoDestructor<ConsentAuditorFactory>;

  ConsentAuditorFactory();
  ~ConsentAuditorFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_
