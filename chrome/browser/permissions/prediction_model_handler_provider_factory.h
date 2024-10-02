// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_PROVIDER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}
namespace permissions {
class PredictionModelHandlerProvider;
}

class PredictionModelHandlerProviderFactory
    : public ProfileKeyedServiceFactory {
 public:
  PredictionModelHandlerProviderFactory(
      const PredictionModelHandlerProviderFactory&) = delete;
  PredictionModelHandlerProviderFactory& operator=(
      const PredictionModelHandlerProviderFactory&) = delete;

  static PredictionModelHandlerProviderFactory* GetInstance();
  static permissions::PredictionModelHandlerProvider* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  PredictionModelHandlerProviderFactory();
  ~PredictionModelHandlerProviderFactory() override;
  friend struct base::DefaultSingletonTraits<
      PredictionModelHandlerProviderFactory>;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_MODEL_HANDLER_PROVIDER_FACTORY_H_
