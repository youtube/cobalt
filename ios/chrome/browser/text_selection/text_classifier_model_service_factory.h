// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class TextClassifierModelService;

// Singleton that owns all TextClassifierModelService(s) and associates them
// with ChromeBrowserState.
class TextClassifierModelServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static TextClassifierModelService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static TextClassifierModelServiceFactory* GetInstance();

  TextClassifierModelServiceFactory(const TextClassifierModelServiceFactory&) =
      delete;
  TextClassifierModelServiceFactory& operator=(
      const TextClassifierModelServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<TextClassifierModelServiceFactory>;

  TextClassifierModelServiceFactory();
  ~TextClassifierModelServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsCreatedWithBrowserState() const override;
};

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_TEXT_CLASSIFIER_MODEL_SERVICE_FACTORY_H_
