// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_WIN_CONSTANTS_H_
#define CHROME_UPDATER_WIN_WIN_CONSTANTS_H_

#include <windows.h>

#include "base/time/time.h"
#include "chrome/updater/updater_branding.h"

namespace updater {

extern const wchar_t kLegacyGoogleUpdateAppID[];

extern const wchar_t kGoogleUpdate3WebSystemClassProgId[];
extern const wchar_t kGoogleUpdate3WebUserClassProgId[];

// The prefix to use for global names in WIN32 API's. The prefix is necessary
// to avoid collision on kernel object names.
extern const wchar_t kGlobalPrefix[];

// Registry keys and value names.
#define COMPANY_KEY L"Software\\" COMPANY_SHORTNAME_STRING L"\\"

// Use |Update| instead of PRODUCT_FULLNAME_STRING for the registry key name
// to be backward compatible with Google Update / Omaha.
#define UPDATER_KEY COMPANY_KEY L"Update\\"
#define CLIENTS_KEY UPDATER_KEY L"Clients\\"
#define CLIENT_STATE_KEY UPDATER_KEY L"ClientState\\"
#define CLIENT_STATE_MEDIUM_KEY UPDATER_KEY L"ClientStateMedium\\"

#define COMPANY_POLICIES_KEY \
  L"Software\\Policies\\" COMPANY_SHORTNAME_STRING L"\\"
#define UPDATER_POLICIES_KEY COMPANY_POLICIES_KEY L"Update\\"

#define USER_REG_VISTA_LOW_INTEGRITY_HKCU     \
  L"Software\\Microsoft\\Internet Explorer\\" \
  L"InternetRegistry\\REGISTRY\\USER"

// The environment variable is created into the environment of the installer
// process to indicate the updater scope. The valid values for the environment
// variable are "0" and "1".
#define ENV_GOOGLE_UPDATE_IS_MACHINE COMPANY_SHORTNAME_STRING L"UpdateIsMachine"

extern const wchar_t kRegValuePV[];
extern const wchar_t kRegValueBrandCode[];
extern const wchar_t kRegValueAP[];
extern const wchar_t kRegValueDateOfLastActivity[];
extern const wchar_t kRegValueDateOfLastRollcall[];
extern const wchar_t kRegValueName[];

// Values created under `UPDATER_KEY`.
extern const wchar_t kRegValueUninstallCmdLine[];
extern const wchar_t kRegValueVersion[];

// Installer API registry names.
// Registry values read from the Clients key for transmitting custom install
// errors, messages, etc. On an update or install, the InstallerXXX values are
// renamed to LastInstallerXXX values. The LastInstallerXXX values remain around
// until the next update or install. Legacy MSI installers read values such as
// the `LastInstallerResultUIString` from the `ClientState` key in the registry
// and display the string.
extern const wchar_t kRegValueInstallerError[];
extern const wchar_t kRegValueInstallerExtraCode1[];
extern const wchar_t kRegValueInstallerProgress[];
extern const wchar_t kRegValueInstallerResult[];
extern const wchar_t kRegValueInstallerResultUIString[];
extern const wchar_t kRegValueInstallerSuccessLaunchCmdLine[];

extern const wchar_t kRegValueLastInstallerResult[];
extern const wchar_t kRegValueLastInstallerError[];
extern const wchar_t kRegValueLastInstallerExtraCode1[];
extern const wchar_t kRegValueLastInstallerResultUIString[];
extern const wchar_t kRegValueLastInstallerSuccessLaunchCmdLine[];

// AppCommand registry constants.
extern const wchar_t kRegKeyCommands[];
extern const wchar_t kRegValueCommandLine[];
extern const wchar_t kRegValueAutoRunOnOSUpgrade[];

// Device management.
//
// Registry for enrollment token.
extern const wchar_t kRegKeyCompanyCloudManagement[];
extern const wchar_t kRegValueEnrollmentToken[];

// The name of the policy indicating that enrollment in cloud-based device
// management is mandatory.
extern const wchar_t kRegValueEnrollmentMandatory[];

// Registry for DM token.
extern const wchar_t kRegKeyCompanyEnrollment[];
extern const wchar_t kRegValueDmToken[];

extern const wchar_t kWindowsServiceName[];
extern const wchar_t kWindowsInternalServiceName[];

// Windows event name used to signal the legacy GoogleUpdate processes to exit.
extern const wchar_t kShutdownEvent[];

// EXE name for the legacy GoogleUpdate processes.
extern const wchar_t kLegacyExeName[];

// crbug.com/1259178: there is a race condition on activating the COM service
// and the service shutdown. The race condition is likely to occur when a new
// instance of an updater coclass is created right after the last reference to
// an object hosted by the COM service is released. Therefore, introducing a
// slight delay before creating coclasses reduces (but it does not eliminate)
// the probability of running into this race condition, until a better
// solution is found.
inline constexpr base::TimeDelta kCreateUpdaterInstanceDelay =
    base::Milliseconds(200);

// `kLegacyServiceNamePrefix` is the common prefix for the legacy GoogleUpdate
// service names.
extern const wchar_t kLegacyServiceNamePrefix[];

// "Google Update Service" is the common prefix for the legacy GoogleUpdate
// service display names.
extern const wchar_t kLegacyServiceDisplayNamePrefix[];

// "Google Update" is the prefix for the legacy GoogleUpdate "Run" key value
// under HKCU.
extern const wchar_t kLegacyRunValuePrefix[];

// "GoogleUpdateTask{Machine/User}" is the common prefix for the legacy
// GoogleUpdate tasks for system and user respectively.
extern const wchar_t kLegacyTaskNamePrefixSystem[];
extern const wchar_t kLegacyTaskNamePrefixUser[];

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_WIN_CONSTANTS_H_
