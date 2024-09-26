// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/browser_util.h"

#include <windows.h>

// sddl.h must come after windows.h.
#include <sddl.h>

#include <string>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/win/scoped_localalloc.h"
#include "sandbox/win/src/win_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace browser_util {

bool IsBrowserAlreadyRunning() {
  static HANDLE handle = nullptr;
  base::FilePath exe_dir_path;
  // DIR_EXE is obtained from the path of FILE_EXE and, on Windows, FILE_EXE is
  // obtained from reading the PEB of the currently running process. This means
  // that even if the EXE file is moved, the DIR_EXE will still reflect the
  // original location of the EXE from when it was started. This is important as
  // IsBrowserAlreadyRunning must detect any running browser in Chrome's install
  // directory, and not in a temporary directory if it is subsequently renamed
  // or moved while running.
  if (!base::PathService::Get(base::DIR_EXE, &exe_dir_path)) {
    // If this fails, there isn't much that can be done. However, assuming that
    // browser is *not* already running is the safer action here, as it means
    // that any pending upgrade actions will occur and hopefully the issue that
    // caused this failure will be resolved by the newer version. This might
    // cause the currently running browser to be temporarily broken, but it's
    // probably broken already if this API is failing.
    return false;
  }
  absl::optional<std::wstring> nt_dir_name =
      sandbox::GetNtPathFromWin32Path(exe_dir_path.value());
  if (!nt_dir_name) {
    // See above for why false is returned here.
    return false;
  }
  std::replace(nt_dir_name->begin(), nt_dir_name->end(), '\\', '!');
  base::ranges::transform(*nt_dir_name, nt_dir_name->begin(), tolower);
  nt_dir_name = L"Global\\" + nt_dir_name.value();
  if (handle != NULL)
    ::CloseHandle(handle);

  // For this to work for both user and system installs, we need the event to be
  // accessible to all interactive users so that we can correctly detect any
  // instance they are running. Otherwise, we can end up executing pending
  // upgrade actions while there are instances running, resulting in reliability
  // issues for one of the users.
  // Security Descriptor for the global browser running event:
  //   SYSTEM : EVENT_ALL_ACCESS
  //   Interactive User : EVENT_ALL_ACCESS
  static constexpr wchar_t kAllAccessDescriptor[] =
      L"D:P(A;;0x1F0003;;;SY)(A;;0x1F0003;;;IU)";
  SECURITY_ATTRIBUTES attributes = {sizeof(SECURITY_ATTRIBUTES), nullptr,
                                    FALSE};
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptor(
          kAllAccessDescriptor, SDDL_REVISION_1,
          &attributes.lpSecurityDescriptor, nullptr)) {
    // If this fails, it usually means the security descriptor string is
    // incorrect. As a fallback, create the event with the default descriptor
    // by setting it to nullptr. This works for single user devices which is the
    // most common case for Windows.
    DPCHECK(false);
    attributes.lpSecurityDescriptor = nullptr;
  }
  base::win::ScopedLocalAlloc scoped_sd(attributes.lpSecurityDescriptor);

  handle = ::CreateEventW(&attributes, TRUE, TRUE, nt_dir_name->c_str());
  int error = ::GetLastError();
  return (error == ERROR_ALREADY_EXISTS || error == ERROR_ACCESS_DENIED);
}

}  // namespace browser_util
