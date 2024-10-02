// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_OBSERVER_H_

#include <memory>
#include <string>

namespace proximity_auth {

struct RemoteStatusUpdate;

// An interface for observing events that happen on a Messenger.
class MessengerObserver {
 public:
  // Called when sending an "Easy Unlock used"  local event message completes.
  // |success| is true iff the event was sent successfully.
  virtual void OnUnlockEventSent(bool success) {}

  // Called when a RemoteStatusUpdate is received.
  virtual void OnRemoteStatusUpdate(const RemoteStatusUpdate& status_update) {}

  // Called when a response to a 'decrypt_request' is received, with the
  // |decrypted_bytes| that were returned by the remote device. An empty string
  // indicates failure.
  // TODO(b/227674947): Delete this method since it is only used for deprecated
  // sign in with Smart Lock.
  virtual void OnDecryptResponse(const std::string& decrypted_bytes) {}

  // Called when a response to a 'unlock_request' is received.
  // |success| is true iff the request was made successfully.
  virtual void OnUnlockResponse(bool success) {}

  // Called when the underlying secure channel disconnects.
  virtual void OnDisconnected() {}
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_OBSERVER_H_
