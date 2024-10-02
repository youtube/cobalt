// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_FCM_HANDLER_H_
#define COMPONENTS_SYNC_INVALIDATIONS_FCM_HANDLER_H_

#include <string>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/keyed_service/core/keyed_service.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {

class FCMRegistrationTokenObserver;
class InvalidationsListener;

// This handler is used to register with FCM and to process incoming messages.
class FCMHandler : public gcm::GCMAppHandler {
 public:
  FCMHandler(gcm::GCMDriver* gcm_driver,
             instance_id::InstanceIDDriver* instance_id_driver,
             const std::string& sender_id,
             const std::string& app_id);
  ~FCMHandler() override;
  FCMHandler(const FCMHandler&) = delete;
  FCMHandler& operator=(const FCMHandler&) = delete;

  // Used to start handling incoming invalidations from the server and to obtain
  // an FCM token. This method gets called after data types are configured.
  // Before StartListening() is called for the first time, the FCM registration
  // token will be null.
  void StartListening();

  // Stop handling incoming invalidations. It doesn't cleanup the FCM
  // registration token and doesn't unsubscribe from FCM. All incoming
  // invalidations will be dropped. This method gets called during browser
  // shutdown.
  void StopListening();

  // Stop handling incoming invalidations and delete Instance ID. It clears the
  // FCM registration token. This method gets called during sign-out.
  void StopListeningPermanently();

  // Returns if the handler is listening for incoming invalidations.
  bool IsListening() const;

  // Add a new |listener| which will be notified on each new incoming
  // invalidation. |listener| must not be nullptr. Does nothing if the
  // |listener| has already been added before. When a new |listener| is added,
  // previously received messages will be immediately replayed.
  void AddListener(InvalidationsListener* listener);

  // Removes |listener|, does nothing if it wasn't added before. |listener| must
  // not be nullptr.
  void RemoveListener(InvalidationsListener* listener);

  // Add or remove an FCM token change observer. |observer| must not be nullptr.
  void AddTokenObserver(FCMRegistrationTokenObserver* observer);
  void RemoveTokenObserver(FCMRegistrationTokenObserver* observer);

  // Used to get an obtained FCM token. Returns null if it doesn't have a token.
  const absl::optional<std::string>& GetFCMRegistrationToken() const;

  // GCMAppHandler overrides.
  void ShutdownHandler() override;
  void OnStoreReset() override;
  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override;
  void OnMessagesDeleted(const std::string& app_id) override;
  void OnSendError(const std::string& app_id,
                   const gcm::GCMClient::SendErrorDetails& details) override;
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override;

 private:
  // Called when a subscription token is obtained from the GCM server.
  void DidRetrieveToken(const std::string& subscription_token,
                        instance_id::InstanceID::Result result);
  void ScheduleNextTokenValidation();
  void StartTokenValidation();
  void DidReceiveTokenForValidation(const std::string& new_token,
                                    instance_id::InstanceID::Result result);

  void StartTokenFetch(instance_id::InstanceID::GetTokenCallback callback);

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<gcm::GCMDriver> gcm_driver_ = nullptr;
  raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_ = nullptr;
  const std::string sender_id_;
  const std::string app_id_;

  // Contains an FCM registration token. Token is null if the experiment is off
  // or we don't have a valid token yet and contains valid token otherwise.
  absl::optional<std::string> fcm_registration_token_;

  base::OneShotTimer token_validation_timer_;

  // A list of the latest incoming messages, used to replay incoming messages
  // whenever a new listener is added.
  base::circular_deque<std::string> last_received_messages_;

  // Contains all listeners to notify about each incoming message in OnMessage
  // method.
  base::ObserverList<InvalidationsListener,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      listeners_;

  // Contains all FCM token observers to notify about each token change.
  base::ObserverList<FCMRegistrationTokenObserver,
                     /*check_empty=*/true,
                     /*allow_reentrancy=*/false>
      token_observers_;

  base::WeakPtrFactory<FCMHandler> weak_ptr_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_FCM_HANDLER_H_
