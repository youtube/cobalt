// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_DEPENDENCY_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_DEPENDENCY_FACTORY_H_

#include "chrome/browser/enterprise/connectors/device_trust/signals/dependency_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors::test {

class MockDependencyFactory : public DependencyFactory {
 public:
  MockDependencyFactory();
  ~MockDependencyFactory() override;

  MOCK_METHOD(policy::CloudPolicyManager*,
              GetUserCloudPolicyManager,
              (),
              (const, override));
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_MOCK_DEPENDENCY_FACTORY_H_
