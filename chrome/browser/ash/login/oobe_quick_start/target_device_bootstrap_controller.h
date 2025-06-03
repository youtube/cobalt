// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connections_manager.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash::quick_start {

class QuickStartConnectivityService;

class TargetDeviceBootstrapController
    : public TargetDeviceConnectionBroker::ConnectionLifecycleListener {
 public:
  enum class Step {
    NONE,
    ERROR,
    ADVERTISING_WITH_QR_CODE,
    ADVERTISING_WITHOUT_QR_CODE,
    PIN_VERIFICATION,
    CONNECTED,
    REQUESTING_WIFI_CREDENTIALS,
    EMPTY_WIFI_CREDENTIALS_RECEIVED,
    WIFI_CREDENTIALS_RECEIVED,
    REQUESTING_GOOGLE_ACCOUNT_INFO,
    GOOGLE_ACCOUNT_INFO_RECEIVED,
    TRANSFERRING_GOOGLE_ACCOUNT_DETAILS,
    TRANSFERRED_GOOGLE_ACCOUNT_DETAILS,
  };

  enum class ErrorCode {
    START_ADVERTISING_FAILED,
    CONNECTION_REJECTED,
    CONNECTION_CLOSED,
    USER_VERIFICATION_FAILED,
    GAIA_ASSERTION_NOT_RECEIVED,
    FETCHING_CHALLENGE_BYTES_FAILED,
  };

  using Payload = absl::
      variant<absl::monostate, ErrorCode, QRCode::PixelData, FidoAssertionInfo>;

  // TODO(b/288054370) - Consolidate fields.
  struct Status {
    Status();
    ~Status();
    Step step = Step::NONE;
    Payload payload;
    mojom::WifiCredentials wifi_credentials;
    std::string pin;
  };

  class AccessibilityManagerWrapper {
   public:
    AccessibilityManagerWrapper() = default;
    AccessibilityManagerWrapper(AccessibilityManagerWrapper&) = delete;
    AccessibilityManagerWrapper& operator=(AccessibilityManagerWrapper&) =
        delete;
    virtual ~AccessibilityManagerWrapper() = default;

    virtual bool IsSpokenFeedbackEnabled() const = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnStatusChanged(const Status& status) = 0;

   protected:
    ~Observer() override = default;
  };

  TargetDeviceBootstrapController(
      std::unique_ptr<SecondDeviceAuthBroker> auth_broker,
      std::unique_ptr<AccessibilityManagerWrapper>
          accessibility_manager_wrapper,
      QuickStartConnectivityService* quick_start_connectivity_service);
  TargetDeviceBootstrapController(TargetDeviceBootstrapController&) = delete;
  TargetDeviceBootstrapController& operator=(TargetDeviceBootstrapController&) =
      delete;
  ~TargetDeviceBootstrapController() override;

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  void GetFeatureSupportStatusAsync(
      TargetDeviceConnectionBroker::FeatureSupportStatusCallback callback);

  // Returns the CryptAuth Instance ID of the connected remote device. Returns
  // an empty string if no ID is available.
  std::string GetPhoneInstanceId();

  // This function would crash (if DCHECKs are on) in case there are existing
  // valid weakptrs.
  base::WeakPtr<TargetDeviceBootstrapController> GetAsWeakPtrForClient();

  // This method starts advertising and gets the QR code pixel data if using QR
  // Code verification. The QR Code is needed for the initial Quick Start UI, so
  // it's retrieved during this initial step when advertising begins.
  //  After the user scans the QR code with their source device, the source
  //  device accepts the connection, and a cryptographic handshake using the
  //  secret contained in the QR code is used to authenticate the connection.
  //  It's possible for a user to scan the QR Code before the connection is
  //  initiated between this target device and the remote source device.
  void StartAdvertisingAndMaybeGetQRCode();

  void StopAdvertising();
  void CloseOpenConnections();

  // A user may initiate Quick Start then have to download an update and reboot.
  // This function persists necessary data and notifies the source device so
  // Quick Start can resume where it left off after the reboot.
  void PrepareForUpdate();

  // TargetDeviceConnectionBroker::ConnectionLifecycleListener:
  void OnPinVerificationRequested(const std::string& pin) override;
  void OnConnectionAuthenticated(
      base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
          authenticated_connection) override;
  void OnConnectionRejected() override;
  void OnConnectionClosed(
      TargetDeviceConnectionBroker::ConnectionClosedReason reason) override;

  std::string GetDiscoverableName();
  void AttemptWifiCredentialTransfer();

  // The first step in the account transfer is to request basic account info via
  // the BootstrapConfigurations message, which will give us the account email
  // address among other info.
  void RequestGoogleAccountInfo();

  // Initiates the actual account transfer via a cryptographic handshake between
  // the two devices in conjunction with Google servers.
  void AttemptGoogleAccountTransfer();

  // Called when the flow is aborted due to an error, or cancelled by the user.
  void Cleanup();

 private:
  friend class TargetDeviceBootstrapControllerTest;

  void NotifyObservers();
  void OnStartAdvertisingResult(bool success);
  void OnStopAdvertising();

  void WaitForUserVerification(base::OnceClosure on_verification);
  void OnUserVerificationResult(base::OnceClosure on_verification,
                                absl::optional<mojom::UserVerificationResponse>
                                    user_verification_response);

  // If the target device successfully receives an ack message, it prepares to
  // automatically resume Quick Start after the update and closes the
  // connection. If ack_successful is 'false', it closes the connection without
  // preparing to automatically resume Quick Start after the update.
  void OnNotifySourceOfUpdateResponse(bool ack_successful);

  void OnWifiCredentialsReceived(
      absl::optional<mojom::WifiCredentials> credentials);
  void OnGoogleAccountInfoReceived();
  void OnFidoAssertionReceived(absl::optional<FidoAssertionInfo> assertion);

  void OnChallengeBytesReceived(
      quick_start::SecondDeviceAuthBroker::ChallengeBytesOrError);

  // If we're not advertising, connecting, or connected, perform cleanup.
  void CleanupIfNeeded();

  void set_connection_broker_for_testing(
      std::unique_ptr<TargetDeviceConnectionBroker> connection_broker) {
    connection_broker_ = std::move(connection_broker);
  }

  std::unique_ptr<TargetDeviceConnectionBroker> connection_broker_;

  std::string pin_;
  // TODO: Should we enforce one observer at a time here too?
  base::ObserverList<Observer> observers_;

  Status status_;

  base::WeakPtr<TargetDeviceConnectionBroker::AuthenticatedConnection>
      authenticated_connection_;

  // Challenge bytes to be sent to the Android device for the FIDO assertion.
  Base64UrlString challenge_bytes_;

  std::unique_ptr<quick_start::SecondDeviceAuthBroker> auth_broker_;
  // During this instantiation of SessionContext, if resuming Quick Start after
  // an update, the local state is cleared after fetching the session context
  // data from the previous connection. Re-instantiating the SessionContext
  // object overwrites the context with new data that won't match the previous
  // connection details.
  SessionContext session_context_;

  std::unique_ptr<AccessibilityManagerWrapper> accessibility_manager_wrapper_;

  raw_ptr<QuickStartConnectivityService> quick_start_connectivity_service_;

  base::WeakPtrFactory<TargetDeviceBootstrapController>
      weak_ptr_factory_for_clients_{this};

  base::WeakPtrFactory<TargetDeviceBootstrapController> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& stream,
                         const TargetDeviceBootstrapController::Step& step);

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_TARGET_DEVICE_BOOTSTRAP_CONTROLLER_H_
