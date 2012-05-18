// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"

namespace {

// Maps a request's reason to the handle and requests count.
typedef std::map<std::string, std::pair<HANDLE, int> > HandleMap;

#if _WIN32_WINNT <= _WIN32_WINNT_WIN7
POWER_REQUEST_TYPE PowerRequestExecutionRequired =
    static_cast<POWER_REQUEST_TYPE>(PowerRequestAwayModeRequired + 1);
#endif

POWER_REQUEST_TYPE PowerRequestTestRequired =
    static_cast<POWER_REQUEST_TYPE>(PowerRequestExecutionRequired + 10);

POWER_REQUEST_TYPE PowerRequirementToType(
    base::SystemMonitor::PowerRequirement requirement) {
  switch (requirement) {
    case base::SystemMonitor::DISPLAY_REQUIRED:
      return PowerRequestDisplayRequired;
    case base::SystemMonitor::SYSTEM_REQUIRED:
      return PowerRequestSystemRequired;
    case base::SystemMonitor::CPU_REQUIRED:
      return PowerRequestExecutionRequired;
    case base::SystemMonitor::TEST_REQUIRED:
      return PowerRequestTestRequired;
  }
  NOTREACHED();
  return PowerRequestTestRequired;
}

HANDLE CreatePowerRequest(POWER_REQUEST_TYPE type, const std::string& reason) {
  typedef HANDLE (WINAPI* PowerCreateRequestPtr)(PREASON_CONTEXT);
  typedef BOOL (WINAPI* PowerSetRequestPtr)(HANDLE, POWER_REQUEST_TYPE);

  if (type == PowerRequestTestRequired)
    return NULL;

  if (type == PowerRequestExecutionRequired &&
      base::win::GetVersion() < base::win::VERSION_WIN8) {
    return INVALID_HANDLE_VALUE;
  }

  static PowerCreateRequestPtr PowerCreateRequestFn = NULL;
  static PowerSetRequestPtr PowerSetRequestFn = NULL;

  if (!PowerCreateRequestFn || !PowerSetRequestFn) {
    HMODULE module = GetModuleHandle(L"kernel32.dll");
    PowerCreateRequestFn = reinterpret_cast<PowerCreateRequestPtr>(
        GetProcAddress(module, "PowerCreateRequest"));
    PowerSetRequestFn = reinterpret_cast<PowerSetRequestPtr>(
        GetProcAddress(module, "PowerSetRequest"));

    if (!PowerCreateRequestFn || !PowerSetRequestFn)
      return INVALID_HANDLE_VALUE;
  }
  string16 wide_reason = ASCIIToUTF16(reason);
  REASON_CONTEXT context = {0};
  context.Version = POWER_REQUEST_CONTEXT_VERSION;
  context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
  context.Reason.SimpleReasonString = const_cast<wchar_t*>(wide_reason.c_str());

  base::win::ScopedHandle handle(PowerCreateRequestFn(&context));
  if (!handle.IsValid())
    return INVALID_HANDLE_VALUE;

  if (PowerSetRequestFn(handle, type))
    return handle.Take();

  // Something went wrong.
  return INVALID_HANDLE_VALUE;
}

// Takes ownership of the |handle|.
void DeletePowerRequest(POWER_REQUEST_TYPE type, HANDLE handle) {
  if (type == PowerRequestTestRequired)
    return;

  base::win::ScopedHandle request_handle(handle);
  if (!request_handle.IsValid())
    return;

  if (type == PowerRequestExecutionRequired &&
      base::win::GetVersion() < base::win::VERSION_WIN8) {
    return;
  }

  typedef BOOL (WINAPI* PowerClearRequestPtr)(HANDLE, POWER_REQUEST_TYPE);
  HMODULE module = GetModuleHandle(L"kernel32.dll");
  PowerClearRequestPtr PowerClearRequestFn =
      reinterpret_cast<PowerClearRequestPtr>(
          GetProcAddress(module, "PowerClearRequest"));

  if (!PowerClearRequestFn)
    return;

  BOOL success = PowerClearRequest(request_handle, type);
  DCHECK(success);
}

}  // namespace.

namespace base {

void SystemMonitor::ProcessWmPowerBroadcastMessage(int event_id) {
  PowerEvent power_event;
  switch (event_id) {
    case PBT_APMPOWERSTATUSCHANGE:  // The power status changed.
      power_event = POWER_STATE_EVENT;
      break;
    case PBT_APMRESUMEAUTOMATIC:  // Resume from suspend.
    //case PBT_APMRESUMESUSPEND:  // User-initiated resume from suspend.
                                  // We don't notify for this latter event
                                  // because if it occurs it is always sent as a
                                  // second event after PBT_APMRESUMEAUTOMATIC.
      power_event = RESUME_EVENT;
      break;
    case PBT_APMSUSPEND:  // System has been suspended.
      power_event = SUSPEND_EVENT;
      break;
    default:
      return;

    // Other Power Events:
    // PBT_APMBATTERYLOW - removed in Vista.
    // PBT_APMOEMEVENT - removed in Vista.
    // PBT_APMQUERYSUSPEND - removed in Vista.
    // PBT_APMQUERYSUSPENDFAILED - removed in Vista.
    // PBT_APMRESUMECRITICAL - removed in Vista.
    // PBT_POWERSETTINGCHANGE - user changed the power settings.
  }
  ProcessPowerMessage(power_event);
}

void SystemMonitor::BeginPowerRequirement(PowerRequirement requirement,
                                          const std::string& reason) {
  thread_checker_.CalledOnValidThread();

  HandleMap::iterator i = handles_.find(reason);
  if (i != handles_.end()) {
    // This is not the first request, just increase the requests count.
    i->second.second++;
    DCHECK_GT(i->second.second, 1);
    return;
  }

  HANDLE handle = CreatePowerRequest(PowerRequirementToType(requirement),
                                     reason);

  if (handle != INVALID_HANDLE_VALUE)
    handles_[reason] = std::pair<HANDLE, int>(handle, 1);
}

void SystemMonitor::EndPowerRequirement(PowerRequirement requirement,
                                        const std::string& reason) {
  thread_checker_.CalledOnValidThread();

  HandleMap::iterator i = handles_.find(reason);
  if (i == handles_.end()) {
    NOTREACHED();
    return;
  }

  // Decrease the requests count and see if this the last request.
  i->second.second--;
  DCHECK_GE(i->second.second, 0);

  if (i->second.second)
    return;

  DeletePowerRequest(PowerRequirementToType(requirement), i->second.first);
  handles_.erase(i);
}

size_t SystemMonitor::GetPowerRequirementsCountForTest() const {
  return handles_.size();
}

// Function to query the system to see if it is currently running on
// battery power.  Returns true if running on battery.
bool SystemMonitor::IsBatteryPower() {
  SYSTEM_POWER_STATUS status;
  if (!GetSystemPowerStatus(&status)) {
    DLOG(ERROR) << "GetSystemPowerStatus failed: " << GetLastError();
    return false;
  }
  return (status.ACLineStatus == 0);
}

}  // namespace base
