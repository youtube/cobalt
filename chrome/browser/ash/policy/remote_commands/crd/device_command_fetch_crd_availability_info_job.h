// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_DEVICE_COMMAND_FETCH_CRD_AVAILABILITY_INFO_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_DEVICE_COMMAND_FETCH_CRD_AVAILABILITY_INFO_JOB_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

class PrefService;

namespace policy {

// Remote command that fetches the information that determines what Chrome
// Remote Desktop session types are available (if any).
class DeviceCommandFetchCrdAvailabilityInfoJob : public RemoteCommandJob {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit DeviceCommandFetchCrdAvailabilityInfoJob(PrefService* local_state);
  DeviceCommandFetchCrdAvailabilityInfoJob(
      const DeviceCommandFetchCrdAvailabilityInfoJob&) = delete;
  DeviceCommandFetchCrdAvailabilityInfoJob& operator=(
      const DeviceCommandFetchCrdAvailabilityInfoJob&) = delete;
  ~DeviceCommandFetchCrdAvailabilityInfoJob() override;

  // `RemoteCommandJob` implementation:
  enterprise_management::RemoteCommand_Type GetType() const override;
  void RunImpl(CallbackWithResult result_callback) override;

 private:
  void SendPayload(CallbackWithResult callback, bool is_in_managed_network);

  const raw_ref<PrefService> local_state_;

  base::WeakPtrFactory<DeviceCommandFetchCrdAvailabilityInfoJob>
      weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_DEVICE_COMMAND_FETCH_CRD_AVAILABILITY_INFO_JOB_H_
