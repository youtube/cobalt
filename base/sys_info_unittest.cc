// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/sys_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest SysInfoTest;

TEST_F(SysInfoTest, NumProcs) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GE(base::SysInfo::NumberOfProcessors(), 1);
}

TEST_F(SysInfoTest, AmountOfMem) {
  // We aren't actually testing that it's correct, just that it's sane.
  EXPECT_GT(base::SysInfo::AmountOfPhysicalMemory(), 0);
  EXPECT_GT(base::SysInfo::AmountOfPhysicalMemoryMB(), 0);
}

TEST_F(SysInfoTest, AmountOfFreeDiskSpace) {
  // We aren't actually testing that it's correct, just that it's sane.
  std::wstring tmp_path;
  ASSERT_TRUE(file_util::GetTempDir(&tmp_path));
  EXPECT_GT(base::SysInfo::AmountOfFreeDiskSpace(tmp_path), 0) << tmp_path;
}

TEST_F(SysInfoTest, GetEnvVar) {
  // Every setup should have non-empty PATH...
  EXPECT_NE(base::SysInfo::GetEnvVar(L"PATH"), L"");
}

TEST_F(SysInfoTest, HasEnvVar) {
  // Every setup should have PATH...
  EXPECT_TRUE(base::SysInfo::HasEnvVar(L"PATH"));
}

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
TEST_F(SysInfoTest, OperatingSystemVersionNumbers) {
  int32 os_major_version = -1;
  int32 os_minor_version = -1;
  int32 os_bugfix_version = -1;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  EXPECT_GT(os_major_version, -1);
  EXPECT_GT(os_minor_version, -1);
  EXPECT_GT(os_bugfix_version, -1);
}
#endif

#if defined(OS_CHROMEOS)
TEST_F(SysInfoTest, GoogleChromeOSVersionNumbers) {
  int32 os_major_version = -1;
  int32 os_minor_version = -1;
  int32 os_bugfix_version = -1;
  std::string lsb_release("FOO=1234123.34.5\n");
  lsb_release.append(base::SysInfo::GetLinuxStandardBaseVersionKey());
  lsb_release.append("=1.2.3.4\n");
  base::SysInfo::ParseLsbRelease(lsb_release,
                                 &os_major_version,
                                 &os_minor_version,
                                 &os_bugfix_version);
  EXPECT_EQ(os_major_version, 1);
  EXPECT_EQ(os_minor_version, 2);
  EXPECT_EQ(os_bugfix_version, 3);
}

TEST_F(SysInfoTest, GoogleChromeOSVersionNumbersFirst) {
  int32 os_major_version = -1;
  int32 os_minor_version = -1;
  int32 os_bugfix_version = -1;
  std::string lsb_release(base::SysInfo::GetLinuxStandardBaseVersionKey());
  lsb_release.append("=1.2.3.4\n");
  lsb_release.append("FOO=1234123.34.5\n");
  base::SysInfo::ParseLsbRelease(lsb_release,
                                 &os_major_version,
                                 &os_minor_version,
                                 &os_bugfix_version);
  EXPECT_EQ(os_major_version, 1);
  EXPECT_EQ(os_minor_version, 2);
  EXPECT_EQ(os_bugfix_version, 3);
}

TEST_F(SysInfoTest, GoogleChromeOSNoVersionNumbers) {
  int32 os_major_version = -1;
  int32 os_minor_version = -1;
  int32 os_bugfix_version = -1;
  std::string lsb_release("FOO=1234123.34.5\n");
  base::SysInfo::ParseLsbRelease(lsb_release,
                                 &os_major_version,
                                 &os_minor_version,
                                 &os_bugfix_version);
  EXPECT_EQ(os_major_version, -1);
  EXPECT_EQ(os_minor_version, -1);
  EXPECT_EQ(os_bugfix_version, -1);
}

#endif  // OS_CHROMEOS
