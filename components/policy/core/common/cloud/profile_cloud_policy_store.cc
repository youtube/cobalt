// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/profile_cloud_policy_store.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/proto/policy_signing_key.pb.h"

namespace em = enterprise_management;

namespace policy {
namespace {

const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");
const base::FilePath::CharType kPolicyCache[] =
    FILE_PATH_LITERAL("Profile Cloud Policy");
const base::FilePath::CharType kKeyCache[] =
    FILE_PATH_LITERAL("Profile Cloud Policy Signing Key");

}  // namespace

ProfileCloudPolicyStore::ProfileCloudPolicyStore(
    const base::FilePath& policy_path,
    const base::FilePath& key_path,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : DesktopCloudPolicyStore(policy_path,
                              key_path,
                              PolicyLoadFilter(),
                              background_task_runner,
                              PolicyScope::POLICY_SCOPE_USER) {}

ProfileCloudPolicyStore::~ProfileCloudPolicyStore() = default;

// static
std::unique_ptr<ProfileCloudPolicyStore> ProfileCloudPolicyStore::Create(
    const base::FilePath& profile_dir,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  base::FilePath policy_dir = profile_dir.Append(kPolicy);
  base::FilePath policy_cache_file = policy_dir.Append(kPolicyCache);
  base::FilePath key_cache_file = policy_dir.Append(kKeyCache);
  return std::make_unique<ProfileCloudPolicyStore>(
      policy_cache_file, key_cache_file, background_task_runner);
}

std::unique_ptr<UserCloudPolicyValidator>
ProfileCloudPolicyStore::CreateValidator(
    std::unique_ptr<enterprise_management::PolicyFetchResponse>
        policy_fetch_response,
    CloudPolicyValidatorBase::ValidateTimestampOption option) {
  auto validator = std::make_unique<UserCloudPolicyValidator>(
      std::move(policy_fetch_response), background_task_runner());
  // TODO (crbug/1421330): Once the real policy type is available, replace this
  // validation.
  validator->ValidatePolicyType(
      dm_protocol::kChromeMachineLevelUserCloudPolicyType);
  validator->ValidateAgainstCurrentPolicy(
      policy(), option, CloudPolicyValidatorBase::DM_TOKEN_REQUIRED,
      CloudPolicyValidatorBase::DEVICE_ID_REQUIRED);
  validator->ValidatePayload();
  if (has_policy()) {
    validator->ValidateTimestamp(
        base::Time::FromMillisecondsSinceUnixEpoch(policy()->timestamp()),
        option);
  }
  validator->ValidatePayload();
  return validator;
}

void ProfileCloudPolicyStore::Validate(
    std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
    std::unique_ptr<enterprise_management::PolicySigningKey> key,
    bool validate_in_background,
    UserCloudPolicyValidator::CompletionCallback callback) {
  std::unique_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      std::move(policy), CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);

  ValidateKeyAndSignature(validator.get(), key.get(), std::string());

  if (validate_in_background) {
    UserCloudPolicyValidator::StartValidation(std::move(validator),
                                              std::move(callback));
  } else {
    validator->RunValidation();
    std::move(callback).Run(validator.get());
  }
}

}  // namespace policy
