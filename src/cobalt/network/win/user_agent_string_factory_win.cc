/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/network/user_agent_string_factory.h"

#include <Windows.h>

#include "base/stringprintf.h"

namespace cobalt {
namespace network {

namespace {

class UserAgentStringFactoryWin : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWin();

 private:
  static std::string GetOSNameAndVersion();
  static bool IsWOW64Enabled();
  static std::string GetArchitectureTokens();
};

std::string UserAgentStringFactoryWin::GetOSNameAndVersion() {
  OSVERSIONINFOEX version_info = {sizeof(version_info)};
  BOOL succeeded =
      GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info));
  DCHECK_NE(0, succeeded);

  return base::StringPrintf("Windows NT %lu.%lu", version_info.dwMajorVersion,
                            version_info.dwMinorVersion);
}

bool UserAgentStringFactoryWin::IsWOW64Enabled() {
  typedef BOOL(WINAPI * IsWow64ProcessFunc)(HANDLE, PBOOL);
  MSVC_PUSH_DISABLE_WARNING(4191)
  MSVC_PUSH_DISABLE_WARNING(6387)
  IsWow64ProcessFunc is_wow64_process = reinterpret_cast<IsWow64ProcessFunc>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "IsWow64Process"));
  MSVC_POP_WARNING()
  MSVC_POP_WARNING()
  if (!is_wow64_process) {
    return false;
  }

  BOOL is_wow64 = FALSE;
  return (*is_wow64_process)(GetCurrentProcess(), &is_wow64) != 0 &&
         is_wow64 == TRUE;
}

std::string UserAgentStringFactoryWin::GetArchitectureTokens() {
  SYSTEM_INFO system_info = {0};
  GetNativeSystemInfo(&system_info);

  switch (system_info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:  // X86
      return IsWOW64Enabled() ? "WOW64" : "";
    case PROCESSOR_ARCHITECTURE_AMD64:  // X64
      return "Win64; x64";
    case PROCESSOR_ARCHITECTURE_IA64:  // IA64
      return "Win64; IA64";
    default:
      DLOG(WARNING) << "Unknown Windows architecture.";
      return "";
  }
}

UserAgentStringFactoryWin::UserAgentStringFactoryWin() {
  os_name_and_version_ = GetOSNameAndVersion();
  architecture_tokens_ = GetArchitectureTokens();
}

}  // namespace

scoped_ptr<UserAgentStringFactory>
UserAgentStringFactory::ForCurrentPlatform() {
  return scoped_ptr<UserAgentStringFactory>(new UserAgentStringFactoryWin());
}

}  // namespace network
}  // namespace cobalt
