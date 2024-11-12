// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/invalidations/invalidation_service_impl.h"

#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/upload_token_request.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"

namespace ash::boca {

const net::BackoffEntry::Policy kBackoffPolicy = {
    0,  // Number of initial errors to ignore before applying
        // exponential back-off rules.
    base::Seconds(2).InMilliseconds(),  // Initial delay in ms.
    2,    // Factor by which the waiting time will be multiplied.
    0.2,  // Fuzzing percentage.
    base::Hours(1)
        .InMilliseconds(),  // Maximum amount of time to delay th request in ms.
    -1,                     // Never discard the entry.
    false                   // Do not always use initial delay.
};

InvalidationServiceImpl::InvalidationServiceImpl(
    gcm::GCMDriver* gcm_driver,
    instance_id::InstanceIDDriver* instance_id_driver,
    AccountId account_id,
    BocaSessionManager* boca_session_manager,
    SessionClientImpl* session_client_impl)
    : upload_retry_backoff_{&kBackoffPolicy},
      account_id_(account_id),
      boca_session_manager_(boca_session_manager),
      session_client_impl_(session_client_impl) {
  fcm_handler_ = std::make_unique<FCMHandler>(gcm_driver, instance_id_driver,
                                              kSenderId, kApplicationId);
  // Add token refresh observer.
  fcm_handler_->AddTokenObserver(this);
  // Add invalidation message observer.
  fcm_handler_->AddListener(this);
  // Register app handler and start token fetch.
  fcm_handler_->StartListening();
}

InvalidationServiceImpl::~InvalidationServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fcm_handler_->RemoveTokenObserver(this);
  fcm_handler_->RemoveListener(this);
}

void InvalidationServiceImpl::OnInvalidationReceived(
    const std::string& payload) {
  // TODO(b/354769102): Potentially validate FCM payload before dispatching. And
  // implement a thread-safe approach to skip loading when there is already
  // active loading in progress.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // FCM message will be delivered even user not logged in. But
  // LoadCurrentSession has a validation to skip load if the current active user
  // doesn't match the profile user.
  boca_session_manager_->LoadCurrentSession();
}

void InvalidationServiceImpl::OnFCMRegistrationTokenChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!fcm_handler_->GetFCMRegistrationToken().has_value()) {
    return;
  } else {
    upload_retry_backoff_.Reset();
    UploadToken();
  }
}

void InvalidationServiceImpl::UploadToken() {
  auto request = std::make_unique<UploadTokenRequest>(
      session_client_impl_->sender(), account_id_.GetGaiaId(),
      fcm_handler_->GetFCMRegistrationToken().value(),
      base::BindOnce(&InvalidationServiceImpl::OnTokenUploaded,
                     weak_factory_.GetWeakPtr()));
  session_client_impl_->UploadToken(std::move(request));
}

void InvalidationServiceImpl::OnTokenUploaded(
    base::expected<bool, google_apis::ApiErrorCode> result) {
  if (!result.has_value()) {
    // TODO(b/366316261):Add metrics for token failure.
    LOG(WARNING) << "[Boca]Failed to upload token, retrying";
    upload_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    base::TimeDelta backoff_delay = upload_retry_backoff_.GetTimeUntilRelease();
    token_refresh_timer_.Start(
        FROM_HERE, backoff_delay,
        base::BindOnce(&InvalidationServiceImpl::UploadToken,
                       base::Unretained(this)));
    return;
  }
  LOG(WARNING) << "[Boca]Uploaded invalidation token.";
  upload_retry_backoff_.Reset();
}

void InvalidationServiceImpl::ShutDown() {
  if (fcm_handler_->IsListening()) {
    fcm_handler_->StopListeningPermanently();
  }
}
}  // namespace ash::boca
