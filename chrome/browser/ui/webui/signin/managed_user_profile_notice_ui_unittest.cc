// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/managed_user_profile_notice_ui.h"

#include "base/strings/to_string.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(ManagedUserProfileNoticeURLTest, ValidScreenTypeInURL) {
  GURL url = net::AppendQueryParameter(
      GURL(chrome::kChromeUIManagedUserProfileNoticeRefreshURL), "type",
      base::ToString(static_cast<int>(
          ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC)));
  EXPECT_EQ(ManagedUserProfileNoticeUI::GetScreenTypeFromURLForTesting(url),
            ManagedUserProfileNoticeUI::ScreenType::kEnterpriseOIDC);
}

TEST(ManagedUserProfileNoticeURLTest, InvalidScreenTypeInURL) {
  GURL url = net::AppendQueryParameter(
      GURL(chrome::kChromeUIManagedUserProfileNoticeRefreshURL), "type",
      "invalid");
  EXPECT_EQ(ManagedUserProfileNoticeUI::GetScreenTypeFromURLForTesting(url),
            ManagedUserProfileNoticeUI::ScreenType::kProfilePicker);
}

TEST(ManagedUserProfileNoticeURLTest, OutOfBoundsScreenTypeInURL) {
  GURL url = net::AppendQueryParameter(
      GURL(chrome::kChromeUIManagedUserProfileNoticeRefreshURL), "type", "999");
  EXPECT_EQ(ManagedUserProfileNoticeUI::GetScreenTypeFromURLForTesting(url),
            ManagedUserProfileNoticeUI::ScreenType::kProfilePicker);
}

TEST(ManagedUserProfileNoticeURLTest, MissingScreenTypeInURL) {
  GURL url = GURL(chrome::kChromeUIManagedUserProfileNoticeRefreshURL);
  EXPECT_EQ(ManagedUserProfileNoticeUI::GetScreenTypeFromURLForTesting(url),
            ManagedUserProfileNoticeUI::ScreenType::kProfilePicker);
}
