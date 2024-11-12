// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

namespace autofill {

class AutocompleteHistoryManager;

// Singleton that owns all AutocompleteHistoryManagers and associates them with
// profiles.
class AutocompleteHistoryManagerFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static AutocompleteHistoryManager* GetForProfile(ProfileIOS* profile);
  static AutocompleteHistoryManagerFactory* GetInstance();

  AutocompleteHistoryManagerFactory(const AutocompleteHistoryManagerFactory&) =
      delete;
  AutocompleteHistoryManagerFactory& operator=(
      const AutocompleteHistoryManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<AutocompleteHistoryManagerFactory>;

  AutocompleteHistoryManagerFactory();
  ~AutocompleteHistoryManagerFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_
