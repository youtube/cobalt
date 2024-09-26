// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace policy::psm {
class RlweDmserverClient;
}

namespace policy {

class DeviceManagementService;

// Indicates the current state of the auto-enrollment check.
enum class AutoEnrollmentState {
  // TODO(b/265923216): Deprecate kIdle and kPending states after fully
  // migrating to EnrollmentStateFetcher and removing AutoEnrollmentClient.
  // Not yet started.
  kIdle = 0,
  // Working, another event will be fired eventually.
  kPending = 1,
  // Failed to connect to DMServer or to synchronize the system clock.
  kConnectionError = 2,
  // Connection successful, but the server failed to generate a valid reply.
  kServerError = 3,
  // Check completed successfully, enrollment should be triggered.
  kEnrollment = 4,
  // Check completed successfully, enrollment not applicable.
  kNoEnrollment = 5,
  // Check completed successfully, device is disabled.
  kDisabled = 6,
};

// Interacts with the device management service and determines whether this
// machine should automatically enter the Enterprise Enrollment screen during
// OOBE.
class AutoEnrollmentClient {
 public:
  // The modulus value is sent in an int64_t field in the protobuf, whose
  // maximum value is 2^63-1. So 2^64 and 2^63 can't be represented as moduli
  // and the max is 2^62 (when the moduli are restricted to powers-of-2).
  static const int kMaximumPower = 62;

  // Used for signaling progress to a consumer.
  typedef base::RepeatingCallback<void(AutoEnrollmentState)> ProgressCallback;

  // Creates |AutoEnrollmentClient| instances.
  class Factory {
   public:
    virtual ~Factory() {}

    // |progress_callback| will be invoked whenever some significant event
    // happens as part of the protocol, after Start() is invoked. The result of
    // the protocol will be cached in |local_state|. |power_initial| and
    // |power_limit| are exponents of power-of-2 values which will be the
    // initial modulus and the maximum modulus used by this client.
    virtual std::unique_ptr<AutoEnrollmentClient> CreateForFRE(
        const ProgressCallback& progress_callback,
        DeviceManagementService* device_management_service,
        PrefService* local_state,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& server_backed_state_key,
        int power_initial,
        int power_limit) = 0;

    // |progress_callback| will be invoked whenever some significant event
    // happens as part of the protocol, after Start() is invoked. The result of
    // the protocol will be cached in |local_state|.
    virtual std::unique_ptr<AutoEnrollmentClient> CreateForInitialEnrollment(
        const ProgressCallback& progress_callback,
        DeviceManagementService* device_management_service,
        PrefService* local_state,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& device_serial_number,
        const std::string& device_brand_code,
        std::unique_ptr<psm::RlweDmserverClient> psm_rlwe_dmserver_client) = 0;
  };

  virtual ~AutoEnrollmentClient() {}

  // Starts the auto-enrollment check protocol with the device management
  // service. Subsequent calls drop any previous requests. Notice that this
  // call can invoke the |progress_callback_| if errors occur.
  virtual void Start() = 0;

  // Triggers a retry of the currently pending step. This is intended to be
  // called by consumers when they become aware of environment changes (such as
  // captive portal setup being complete).
  virtual void Retry() = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_H_
