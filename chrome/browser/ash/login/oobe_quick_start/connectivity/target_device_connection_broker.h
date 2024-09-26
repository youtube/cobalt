// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/random_session_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::quick_start {

struct FidoAssertionInfo;
struct WifiCredentials;

// TargetDeviceConnectionBroker is the entrypoint for consuming the Quick Start
// connectivity component. Calling code is expected to get an instance of this
// class using the TargetDeviceConnectionBrokerFactory and interact with the
// component using the public interface of this class.
//
// All references to "target device" imply this device (Chromebook). All
// references to "source device" imply the remote Android phone, which is the
// source for Gaia and WiFi credentials.
class TargetDeviceConnectionBroker {
 public:
  using ResultCallback = base::OnceCallback<void(bool success)>;
  using SharedSecret = std::array<uint8_t, 32>;

  enum class FeatureSupportStatus {
    kUndetermined = 0,
    kNotSupported,
    kSupported
  };

  enum class ConnectionClosedReason {
    kComplete,
    kUserAborted,
    kAuthenticationFailed,
    kConnectionLost,
    kRequestTimedOut,
    kUnknownError,
  };

  class AuthenticatedConnection {
   public:
    using RequestWifiCredentialsCallback =
        base::OnceCallback<void(absl::optional<WifiCredentials>)>;
    using RequestAccountTransferAssertionCallback =
        base::OnceCallback<void(absl::optional<FidoAssertionInfo>)>;

    // Request wifi credentials from target Android device. The session_id is
    // used to identify this QuickStart session and is distinct from the
    // RandomSessionId.
    virtual void RequestWifiCredentials(
        int32_t session_id,
        RequestWifiCredentialsCallback callback) = 0;

    // Notify Android device that the Chromebook will download an update and
    // reboot.
    virtual void NotifySourceOfUpdate(int32_t session_id) = 0;

    // Begin the account transfer process and retrieve
    // an Assertion from the source device. The user will be asked to confirm
    // their lock screen PIN/pattern/etc. on the source device.
    // This object's client must provide a "challenge" to be sent to the remote
    // source device.
    virtual void RequestAccountTransferAssertion(
        const std::string& challenge_b64url,
        RequestAccountTransferAssertionCallback callback) = 0;

   protected:
    AuthenticatedConnection() = default;
    virtual ~AuthenticatedConnection() = default;
  };

  // Clients of TargetDeviceConnectionBroker should implement this interface,
  // and provide a self-reference when calling TargetDeviceConnectionBroker::
  // StartAdvertising().
  //
  // This interface is a simplification of
  // nearby::connections::mojom::ConnectionLifecycleListener, for ease
  // of client use.
  class ConnectionLifecycleListener {
   public:
    ConnectionLifecycleListener() = default;
    virtual ~ConnectionLifecycleListener() = default;

    // A connection has been initiated between this target device and the remote
    // source device, but needs to be authenticated before messages can be
    // exchanged. The source device has requested that the pin be displayed so
    // that the user can check that the codes match, thereby authenticating the
    // connection.
    virtual void OnPinVerificationRequested(const std::string& pin) = 0;

    // A connection has been initiated between this target device and the remote
    // source device, but needs to be authenticated before messages can be
    // exchanged. The source device has requested that the QR code be displayed
    // so that the user can scan the code. After scanning, the source device
    // will accept the connection, and a cryptographic handshake using a secret
    // contained in the QR code will be used to authenticate the connection.
    virtual void OnQRCodeVerificationRequested(
        const std::vector<uint8_t>& qr_code_data) = 0;

    // Called after both sides have accepted the connection.
    //
    // This connection may be a "resumed" connection that was previously
    // "paused" before this target device performed a Critical Update and
    // rebooted.
    //
    // The AuthenticatedConnection pointer may be cached, but will become
    // invalid after OnConnectionClosed() is called.
    //
    // Use source_device_id to understand which connection
    // OnConnectionClosed() refers to.
    virtual void OnConnectionAuthenticated(
        base::WeakPtr<AuthenticatedConnection> authenticated_connection) = 0;

    // Called if the source device rejected the connection.
    virtual void OnConnectionRejected() = 0;

    // Called when the source device is disconnected or has become unreachable.
    virtual void OnConnectionClosed(ConnectionClosedReason reason) = 0;
  };

  TargetDeviceConnectionBroker();
  virtual ~TargetDeviceConnectionBroker();

  // Checks to see whether the feature can be supported on the device's
  // hardware. The feature is supported if Bluetooth is supported and an adapter
  // is present.
  virtual FeatureSupportStatus GetFeatureSupportStatus() const = 0;

  using FeatureSupportStatusCallback =
      base::OnceCallback<void(FeatureSupportStatus status)>;
  void GetFeatureSupportStatusAsync(FeatureSupportStatusCallback callback);

  // Will kick off Fast Pair and Nearby Connections advertising.
  // Clients can use the result of |on_start_advertising_callback| to
  // immediately understand if advertising succeeded, and can then wait for the
  // source device to connect and request authentication via
  // |ConnectionLifecycleListener::OnPinVerificationRequested()| or
  // |ConnectionLifecycleListener::OnQRCodeVerificationRequested()|.
  //
  // If the caller paused a connection previously, the connection to the
  // source device will resume via OnConnectionAuthenticated().
  // Clients should check  GetFeatureSupportStatus()  before calling
  // StartAdvertising().
  //
  // If |use_pin_authentication| is true, then the target device will
  // advertise its preference to use pin authentication instead of QR code
  // authentication. This should be false unless the user would benefit from
  // using pin for, e.g. accessibility reasons.
  virtual void StartAdvertising(
      ConnectionLifecycleListener* listener,
      bool use_pin_authentication,
      ResultCallback on_start_advertising_callback) = 0;

  // Clients are responsible for calling this once they have accepted their
  // desired connection, or in error/edge cases, e.g., the user exits the UI.
  virtual void StopAdvertising(
      base::OnceClosure on_stop_advertising_callback) = 0;

  // Returns Dict that can be persisted to a local state Dict pref if the target
  // device is going to update. This Dict contains the RandomSessionId and
  // secondary SharedSecret represented as base64-encoded strings. These values
  // are needed to resume the Quick Start connection after the target device
  // reboots.
  virtual base::Value::Dict GetPrepareForUpdateInfo() = 0;

 protected:
  void MaybeNotifyFeatureStatus();
  void OnConnectionAuthenticated(
      base::WeakPtr<AuthenticatedConnection> authenticated_connection);

  // Returns a deep link URL as a vector of bytes that will form the QR code
  // used to authenticate the connection.
  std::vector<uint8_t> GetQrCodeData(const RandomSessionId& random_session_id,
                                     const SharedSecret shared_secret) const;

  // Derive a 4-digit decimal pin code from the authentication token. This is
  // meant to match the Android implementation found here:
  // http://google3/java/com/google/android/gmscore/integ/modules/smartdevice/src/com/google/android/gms/smartdevice/d2d/nearby/advertisement/VerificationUtils.java;l=37;rcl=511361463
  std::string DerivePin(const std::string& authentication_token) const;

  // Determines whether the advertisement info sent to the source device will
  // request pin verification or QR code verification.
  bool use_pin_authentication_ = false;

  ConnectionLifecycleListener* connection_lifecycle_listener_ = nullptr;

 private:
  std::vector<FeatureSupportStatusCallback> feature_status_callbacks_;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_TARGET_DEVICE_CONNECTION_BROKER_H_
