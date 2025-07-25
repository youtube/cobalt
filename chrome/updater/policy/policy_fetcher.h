// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_POLICY_POLICY_FETCHER_H_
#define CHROME_UPDATER_POLICY_POLICY_FETCHER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/device_management/dm_client.h"
#include "chrome/updater/device_management/dm_response_validator.h"
#include "chrome/updater/policy/manager.h"
#include "chrome/updater/policy/service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace updater {

// The PolicyFetcher handles registration and DM policy refreshes.
class PolicyFetcher : public base::RefCountedThreadSafe<PolicyFetcher> {
 public:
  PolicyFetcher(const GURL& server_url,
                const absl::optional<PolicyServiceProxyConfiguration>&
                    proxy_configuration,
                const absl::optional<bool>& override_is_managed_device);
  void FetchPolicies(
      base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>
          callback);

 private:
  friend class base::RefCountedThreadSafe<PolicyFetcher>;
  virtual ~PolicyFetcher();

  void RegisterDevice(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      base::OnceCallback<void(bool, DMClient::RequestResult)> callback);
  void OnRegisterDeviceRequestComplete(
      base::OnceCallback<void(int, scoped_refptr<PolicyManagerInterface>)>
          callback,
      bool is_enrollment_mandatory,
      DMClient::RequestResult result);

  void FetchPolicy(
      base::OnceCallback<void(scoped_refptr<PolicyManagerInterface>)> callback);
  scoped_refptr<PolicyManagerInterface> OnFetchPolicyRequestComplete(
      DMClient::RequestResult result,
      const std::vector<PolicyValidationResult>& validation_results);

  SEQUENCE_CHECKER(sequence_checker_);
  const GURL server_url_;
  const absl::optional<PolicyServiceProxyConfiguration>
      policy_service_proxy_configuration_;
  const absl::optional<bool> override_is_managed_device_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_POLICY_POLICY_FETCHER_H_
