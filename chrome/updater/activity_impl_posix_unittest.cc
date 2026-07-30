// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/updater/activity_impl_util_posix.h"
#include "chrome/updater/updater_branding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

constexpr base::FilePath::CharType kHomeDir[] = FILE_PATH_LITERAL("/home/user");

}  // namespace

TEST(GetActiveFileTest, ReturnsExpectedActiveFilePath) {
  const std::string app_id = "COM.GOOGLE.CHROME";

#if BUILDFLAG(IS_MAC)
  const base::FilePath expected = base::FilePath(kHomeDir)
                                      .Append("Library")
                                      .Append(COMPANY_SHORTNAME_STRING)
                                      .Append(KEYSTONE_NAME)
                                      .Append("Actives")
                                      .Append(app_id);
#elif BUILDFLAG(IS_LINUX)
  const base::FilePath expected = base::FilePath(kHomeDir)
                                      .Append(".local")
                                      .Append(COMPANY_SHORTNAME_STRING)
                                      .Append(PRODUCT_FULLNAME_STRING)
                                      .Append("Actives")
                                      .Append(app_id);
#endif

  EXPECT_EQ(GetActiveFile(base::FilePath(kHomeDir), app_id), expected);
}

TEST(GetActiveFileTest, RejectsTraversalComponents) {
  EXPECT_FALSE(GetActiveFile(base::FilePath(kHomeDir), "..").has_value());
  EXPECT_FALSE(
      GetActiveFile(base::FilePath(kHomeDir), "../etc/shadow").has_value());
  EXPECT_FALSE(GetActiveFile(base::FilePath(kHomeDir), "a/../b").has_value());
  EXPECT_FALSE(
      GetActiveFile(base::FilePath(kHomeDir), "a/b/../../../c").has_value());
  EXPECT_FALSE(GetActiveFile(base::FilePath(kHomeDir), "").has_value());
  EXPECT_FALSE(GetActiveFile(base::FilePath(kHomeDir), ".").has_value());
}

TEST(GetActiveFileTest, RejectsAbsolutePaths) {
  EXPECT_FALSE(GetActiveFile(base::FilePath(kHomeDir), "/").has_value());
  EXPECT_FALSE(
      GetActiveFile(base::FilePath(kHomeDir), "/etc/shadow").has_value());
  EXPECT_FALSE(GetActiveFile(base::FilePath(kHomeDir), "/a").has_value());
  EXPECT_FALSE(GetActiveFile(base::FilePath(kHomeDir), "/a/b").has_value());
}

TEST(GetActiveFileTest, RejectsSubdirectories) {
  EXPECT_FALSE(GetActiveFile(base::FilePath(kHomeDir), "a/b").has_value());
}

}  // namespace updater
