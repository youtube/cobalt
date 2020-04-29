// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/util.h"

#include <aclapi.h>
#include <shlobj.h>
#include <windows.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/process_iterator.h"

namespace updater {

namespace {

// The number of iterations to poll if a process is stopped correctly.
const unsigned int kMaxProcessQueryIterations = 50;

// The sleep time in ms between each poll.
const unsigned int kProcessQueryWaitTimeMs = 100;

}  // namespace

HRESULT HRESULTFromLastError() {
  const auto error_code = ::GetLastError();
  return (error_code != NO_ERROR) ? HRESULT_FROM_WIN32(error_code) : E_FAIL;
}

bool IsProcessRunning(const wchar_t* executable) {
  base::NamedProcessIterator iter(executable, nullptr);
  const base::ProcessEntry* entry = iter.NextProcessEntry();
  return entry != nullptr;
}

bool WaitForProcessesStopped(const wchar_t* executable) {
  DCHECK(executable);
  VLOG(1) << "Wait for processes '" << executable << "'.";

  // Wait until the process is completely stopped.
  for (unsigned int iteration = 0; iteration < kMaxProcessQueryIterations;
       ++iteration) {
    if (!IsProcessRunning(executable))
      return true;
    ::Sleep(kProcessQueryWaitTimeMs);
  }

  // The process didn't terminate.
  LOG(ERROR) << "Cannot stop process '" << executable << "', timeout.";
  return false;
}

// This sets up COM security to allow NetworkService, LocalService, and System
// to call back into the process. It is largely inspired by
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa378987.aspx
// static
bool InitializeCOMSecurity() {
  // Create the security descriptor explicitly as follows because
  // CoInitializeSecurity() will not accept the relative security descriptors
  // returned by ConvertStringSecurityDescriptorToSecurityDescriptor().
  const size_t kSidCount = 5;
  uint64_t* sids[kSidCount][(SECURITY_MAX_SID_SIZE + sizeof(uint64_t) - 1) /
                            sizeof(uint64_t)] = {
      {}, {}, {}, {}, {},
  };

  // These are ordered by most interesting ones to try first.
  WELL_KNOWN_SID_TYPE sid_types[kSidCount] = {
      WinBuiltinAdministratorsSid,  // administrator group security identifier
      WinLocalServiceSid,           // local service security identifier
      WinNetworkServiceSid,         // network service security identifier
      WinSelfSid,                   // personal account security identifier
      WinLocalSystemSid,            // local system security identifier
  };

  // This creates a security descriptor that is equivalent to the following
  // security descriptor definition language (SDDL) string:
  //   O:BAG:BAD:(A;;0x1;;;LS)(A;;0x1;;;NS)(A;;0x1;;;PS)
  //   (A;;0x1;;;SY)(A;;0x1;;;BA)

  // Initialize the security descriptor.
  SECURITY_DESCRIPTOR security_desc = {};
  if (!::InitializeSecurityDescriptor(&security_desc,
                                      SECURITY_DESCRIPTOR_REVISION))
    return false;

  DCHECK_EQ(kSidCount, base::size(sids));
  DCHECK_EQ(kSidCount, base::size(sid_types));
  for (size_t i = 0; i < kSidCount; ++i) {
    DWORD sid_bytes = sizeof(sids[i]);
    if (!::CreateWellKnownSid(sid_types[i], nullptr, sids[i], &sid_bytes))
      return false;
  }

  // Setup the access control entries (ACE) for COM. You may need to modify
  // the access permissions for your application. COM_RIGHTS_EXECUTE and
  // COM_RIGHTS_EXECUTE_LOCAL are the minimum access rights required.
  EXPLICIT_ACCESS explicit_access[kSidCount] = {};
  DCHECK_EQ(kSidCount, base::size(sids));
  DCHECK_EQ(kSidCount, base::size(explicit_access));
  for (size_t i = 0; i < kSidCount; ++i) {
    explicit_access[i].grfAccessPermissions =
        COM_RIGHTS_EXECUTE | COM_RIGHTS_EXECUTE_LOCAL;
    explicit_access[i].grfAccessMode = SET_ACCESS;
    explicit_access[i].grfInheritance = NO_INHERITANCE;
    explicit_access[i].Trustee.pMultipleTrustee = nullptr;
    explicit_access[i].Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    explicit_access[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    explicit_access[i].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    explicit_access[i].Trustee.ptstrName = reinterpret_cast<LPTSTR>(sids[i]);
  }

  // Create an access control list (ACL) using this ACE list, if this succeeds
  // make sure to ::LocalFree(acl).
  ACL* acl = nullptr;
  DWORD acl_result = ::SetEntriesInAcl(base::size(explicit_access),
                                       explicit_access, nullptr, &acl);
  if (acl_result != ERROR_SUCCESS || acl == nullptr)
    return false;

  HRESULT hr = E_FAIL;

  // Set the security descriptor owner and group to Administrators and set the
  // discretionary access control list (DACL) to the ACL.
  if (::SetSecurityDescriptorOwner(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorGroup(&security_desc, sids[0], FALSE) &&
      ::SetSecurityDescriptorDacl(&security_desc, TRUE, acl, FALSE)) {
    // Initialize COM. You may need to modify the parameters of
    // CoInitializeSecurity() for your application. Note that an
    // explicit security descriptor is being passed down.
    hr = ::CoInitializeSecurity(
        &security_desc, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IDENTIFY, nullptr,
        EOAC_DISABLE_AAA | EOAC_NO_CUSTOM_MARSHAL, nullptr);
  }

  ::LocalFree(acl);
  return SUCCEEDED(hr);
}

}  // namespace updater
