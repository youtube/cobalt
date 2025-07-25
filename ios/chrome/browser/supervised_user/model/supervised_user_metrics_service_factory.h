// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace supervised_user {
class SupervisedUserMetricsService;
}  // namespace supervised_user

class ChromeBrowserState;

// Singleton that owns SupervisedUserMetricsService object and associates
// them with ChromeBrowserState.
class SupervisedUserMetricsServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static supervised_user::SupervisedUserMetricsService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static SupervisedUserMetricsServiceFactory* GetInstance();

  SupervisedUserMetricsServiceFactory(
      const SupervisedUserMetricsServiceFactory&) = delete;
  SupervisedUserMetricsServiceFactory& operator=(
      const SupervisedUserMetricsServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<SupervisedUserMetricsServiceFactory>;

  SupervisedUserMetricsServiceFactory();
  ~SupervisedUserMetricsServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_MODEL_SUPERVISED_USER_METRICS_SERVICE_FACTORY_H_
