// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_rotation_launcher_impl.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/key_rotation_command_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/util.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

using SynchronizationCallback = KeyRotationLauncher::SynchronizationCallback;

namespace {

// Creating the request object involves generating a signature which may be
// resource intensive. It is, therefore, on a background thread.
absl::optional<const KeyUploadRequest> CreateRequest(
    const GURL& dm_server_url,
    const std::string& dm_token,
    const SigningKeyPair& key_pair) {
  return KeyUploadRequest::Create(dm_server_url, dm_token, key_pair);
}

}  // namespace

KeyRotationLauncherImpl::KeyRotationLauncherImpl(
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : dm_token_storage_(dm_token_storage),
      device_management_service_(device_management_service),
      url_loader_factory_(std::move(url_loader_factory)),
      network_delegate_(url_loader_factory_) {
  DCHECK(dm_token_storage_);
  DCHECK(device_management_service_);
  DCHECK(url_loader_factory_);
}
KeyRotationLauncherImpl::~KeyRotationLauncherImpl() = default;

void KeyRotationLauncherImpl::LaunchKeyRotation(
    const std::string& nonce,
    KeyRotationCommand::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!dm_token_storage_ || !device_management_service_) {
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

  auto dm_token = dm_token_storage_->RetrieveDMToken();
  auto dm_server_url = GetDmServerUrl();
  if (!dm_token.is_valid() || !dm_server_url) {
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

  KeyRotationCommand::Params params{dm_token.value(), dm_server_url.value(),
                                    nonce};
  command_ = KeyRotationCommandFactory::GetInstance()->CreateCommand(
      url_loader_factory_);
  if (!command_) {
    // Command can be nullptr if trying to create a key on a unsupported
    // platform.
    std::move(callback).Run(KeyRotationCommand::Status::FAILED);
    return;
  }

  command_->Trigger(params, std::move(callback));
}

void KeyRotationLauncherImpl::SynchronizePublicKey(
    const SigningKeyPair& key_pair,
    SynchronizationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (key_pair.is_empty()) {
    LogSynchronizationError(DTSynchronizationError::kMissingKeyPair);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto dm_token = dm_token_storage_->RetrieveDMToken();
  if (!dm_token.is_valid()) {
    LogSynchronizationError(DTSynchronizationError::kInvalidDmToken);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  auto dm_server_url = GetDmServerUrl();
  if (!dm_server_url) {
    LogSynchronizationError(DTSynchronizationError::kInvalidServerUrl);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  // Passing the key pair by reference is fine in this case as it is owned by
  // a browser-level object (DeviceTrustKeyManager) and will outlive any task
  // running on the ThreadPool.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CreateRequest, GURL(dm_server_url.value()),
                     dm_token.value(), std::ref(key_pair)),
      base::BindOnce(&KeyRotationLauncherImpl::OnUploadRequestCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

absl::optional<std::string> KeyRotationLauncherImpl::GetDmServerUrl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto client_id = dm_token_storage_->RetrieveClientId();
  auto dm_token = dm_token_storage_->RetrieveDMToken();

  if (!dm_token.is_valid()) {
    return absl::nullopt;
  }

  // Get the DM server URL to upload the public key.  Reuse
  // DMServerJobConfiguration to reuse the URL building steps.
  policy::DMServerJobConfiguration config(
      device_management_service_,
      policy::DeviceManagementService::JobConfiguration::
          TYPE_BROWSER_UPLOAD_PUBLIC_KEY,
      client_id, true, policy::DMAuth::FromDMToken(dm_token.value()),
      absl::nullopt, nullptr, base::DoNothing());
  return config.GetResourceRequest(false, 0)->url.spec();
}

void KeyRotationLauncherImpl::OnUploadRequestCreated(
    SynchronizationCallback callback,
    absl::optional<const KeyUploadRequest> upload_request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!upload_request) {
    LogSynchronizationError(DTSynchronizationError::kCannotBuildRequest);
    std::move(callback).Run(absl::nullopt);
    return;
  }

  network_delegate_.SendPublicKeyToDmServer(
      upload_request->dm_server_url(), upload_request->dm_token(),
      upload_request->request_body(),
      base::BindOnce(&KeyRotationLauncherImpl::OnUploadCompleted,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KeyRotationLauncherImpl::OnUploadCompleted(
    SynchronizationCallback callback,
    int status_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static constexpr char kUploadCodeHistogram[] =
      "Enterprise.DeviceTrust.SyncSigningKey.UploadCode";
  base::UmaHistogramSparse(kUploadCodeHistogram, status_code);
  std::move(callback).Run(status_code);
}

}  // namespace enterprise_connectors
