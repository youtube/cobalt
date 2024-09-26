// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_crd_availability_info_job.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd_logging.h"
#include "chrome/browser/ash/policy/remote_commands/crd_remote_command_utils.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace policy {

namespace {

using enterprise_management::CrdSessionAvailability;
using enterprise_management::RemoteCommand;

constexpr char kIdleTime[] = "deviceIdleTimeInSeconds";
constexpr char kUserSessionType[] = "userSessionType";
constexpr char kSupportedCrdSessionTypes[] = "supportedCrdSessionTypes";
constexpr char kRemoteSupportAvailability[] = "remoteSupportAvailability";
constexpr char kRemoteAccessAvailability[] = "remoteAccessAvailability";
constexpr char kIsInManagedEnvironment[] = "isInManagedEnvironment";

base::Value::List GetSupportedSessionTypes(bool is_in_managed_environment) {
  base::Value::List result;

  if (UserSessionSupportsRemoteSupport(GetCurrentUserSessionType())) {
    result.Append(static_cast<int>(CrdSessionType::REMOTE_SUPPORT_SESSION));
  }

  if (UserSessionSupportsRemoteAccess(GetCurrentUserSessionType()) &&
      is_in_managed_environment) {
    result.Append(static_cast<int>(CrdSessionType::REMOTE_ACCESS_SESSION));
  }

  return result;
}

CrdSessionAvailability GetRemoteSupportAvailability(
    UserSessionType current_user_session) {
  if (!UserSessionSupportsRemoteSupport(current_user_session)) {
    return CrdSessionAvailability::UNAVAILABLE_UNSUPPORTED_USER_SESSION_TYPE;
  }
  return CrdSessionAvailability::AVAILABLE;
}

CrdSessionAvailability GetRemoteAccessAvailability(
    bool is_in_managed_environment,
    UserSessionType current_user_session) {
  if (!is_in_managed_environment) {
    return CrdSessionAvailability::UNAVAILABLE_UNMANAGED_ENVIRONMENT;
  }
  if (!UserSessionSupportsRemoteAccess(current_user_session)) {
    return CrdSessionAvailability::UNAVAILABLE_UNSUPPORTED_USER_SESSION_TYPE;
  }
  return CrdSessionAvailability::AVAILABLE;
}

}  // namespace

DeviceCommandFetchCrdAvailabilityInfoJob::
    DeviceCommandFetchCrdAvailabilityInfoJob() = default;
DeviceCommandFetchCrdAvailabilityInfoJob::
    ~DeviceCommandFetchCrdAvailabilityInfoJob() = default;

enterprise_management::RemoteCommand_Type
DeviceCommandFetchCrdAvailabilityInfoJob::GetType() const {
  return RemoteCommand::FETCH_CRD_AVAILABILITY_INFO;
}

void DeviceCommandFetchCrdAvailabilityInfoJob::RunImpl(
    CallbackWithResult result_callback) {
  CalculateIsInManagedEnvironmentAsync(base::BindOnce(
      &DeviceCommandFetchCrdAvailabilityInfoJob::SendPayload,
      weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
}

void DeviceCommandFetchCrdAvailabilityInfoJob::SendPayload(
    CallbackWithResult callback,
    bool is_in_managed_environment) {
  std::string payload =
      base::WriteJson(
          base::Value::Dict()
              .Set(kIdleTime, static_cast<int>(GetDeviceIdleTime().InSeconds()))
              .Set(kUserSessionType, GetCurrentUserSessionType())
              .Set(kIsInManagedEnvironment, is_in_managed_environment)
              .Set(kSupportedCrdSessionTypes,
                   GetSupportedSessionTypes(is_in_managed_environment))
              .Set(kRemoteSupportAvailability,
                   GetRemoteSupportAvailability(GetCurrentUserSessionType()))
              .Set(kRemoteAccessAvailability,
                   GetRemoteAccessAvailability(is_in_managed_environment,
                                               GetCurrentUserSessionType())))
          .value();

  CRD_DVLOG(1) << "Finished FETCH_CRD_AVAILABILITY_INFO remote command: "
               << payload;
  std::move(callback).Run(ResultType::kSuccess, std::move(payload));
}

}  // namespace policy
