// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_FACTORY_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace reporting {

// This class is only used for manual testing purpose. Do not depend on it in
// other parts of the production code.
class ManualTestHeartbeatEventFactory : public ProfileKeyedServiceFactory {
 public:
  static ManualTestHeartbeatEventFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ManualTestHeartbeatEventFactory>;

  ManualTestHeartbeatEventFactory();
  ~ManualTestHeartbeatEventFactory() override;

  // BrowserContextKeyedServiceFactyory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_MANUAL_TEST_HEARTBEAT_EVENT_FACTORY_H_
