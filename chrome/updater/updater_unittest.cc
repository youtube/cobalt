// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/util/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/win/access_token.h"
#include "base/win/windows_types.h"
#endif

namespace updater {

// Tests the updater process returns 0 when run with --test argument.
TEST(UpdaterTest, UpdaterExitCode) {
  base::FilePath out_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &out_dir));
  const base::FilePath updater =
      out_dir.Append(updater::GetExecutableRelativePath());

  base::LaunchOptions options;
#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif  // BUILDFLAG(IS_WIN)

  base::CommandLine command_line(updater);
  command_line.AppendSwitch("test");
  auto process = base::LaunchProcess(command_line, options);
  ASSERT_TRUE(process.IsValid());
  int exit_code = -1;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                             &exit_code));
  EXPECT_EQ(0, exit_code);
}

#if BUILDFLAG(IS_WIN)
// Checks that the unit test has the SE_DEBUG_NAME privilege when the process is
// running as Administrator.
TEST(UpdaterTest, UpdaterTestDebugPrivilege) {
  if (!::IsUserAnAdmin()) {
    return;
  }

  LUID luid = {0};
  ASSERT_TRUE(::LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid));

  CHROME_LUID chrome_luid = {0};
  chrome_luid.LowPart = luid.LowPart;
  chrome_luid.HighPart = luid.HighPart;
  const base::win::AccessToken::Privilege priv(chrome_luid,
                                               SE_PRIVILEGE_ENABLED);

  EXPECT_EQ(priv.GetName(), SE_DEBUG_NAME);
  EXPECT_EQ(priv.GetAttributes(), DWORD{SE_PRIVILEGE_ENABLED});
  EXPECT_TRUE(priv.IsEnabled());
}
#endif  // IS_WIN

}  // namespace updater
