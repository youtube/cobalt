// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_STUB_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_STUB_H_

#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_export.h"

namespace policy {

// A stub implementation, that is used when ENABLE_CONFIGURATION_POLICY is not
// set. This allows client code to compile without requiring #ifdefs.
class POLICY_EXPORT PolicyServiceStub : public PolicyService {
 public:
  PolicyServiceStub();
  PolicyServiceStub(const PolicyServiceStub&) = delete;
  PolicyServiceStub& operator=(const PolicyServiceStub&) = delete;
  ~PolicyServiceStub() override;

  void AddObserver(PolicyDomain domain,
                   Observer* observer) override;

  void RemoveObserver(PolicyDomain domain,
                      Observer* observer) override;

  const PolicyMap& GetPolicies(
      const PolicyNamespace& ns) const override;

  bool IsInitializationComplete(PolicyDomain domain) const override;

  void RefreshPolicies(base::OnceClosure callback) override;

 private:
  const PolicyMap kEmpty_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_SERVICE_STUB_H_
