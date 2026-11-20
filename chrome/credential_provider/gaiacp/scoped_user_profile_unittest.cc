// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/scoped_user_profile.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace credential_provider {

TEST(ScopedUserProfileStaticTest, IsValidPictureUrl) {
  // Valid URLs
  EXPECT_TRUE(ScopedUserProfile::IsValidPictureUrl(
      L"https://lh3.googleusercontent.com/a/abc"));
  EXPECT_TRUE(ScopedUserProfile::IsValidPictureUrl(
      L"https://lh4.googleusercontent.com/a/abc=s96"));
  EXPECT_TRUE(ScopedUserProfile::IsValidPictureUrl(
      L"https://googleusercontent.com/path"));

  // Invalid hosts
  EXPECT_FALSE(ScopedUserProfile::IsValidPictureUrl(
      L"https://malicious.com/picture.jpg"));
  EXPECT_FALSE(ScopedUserProfile::IsValidPictureUrl(
      L"https://lh3.googleusercontent.com.malicious.com/a/abc"));
  EXPECT_FALSE(ScopedUserProfile::IsValidPictureUrl(
      L"https://fake-googleusercontent.com/a/abc"));
  EXPECT_FALSE(
      ScopedUserProfile::IsValidPictureUrl(L"https://google.com/picture.jpg"));

  // Invalid URLs
  EXPECT_FALSE(ScopedUserProfile::IsValidPictureUrl(L"not-a-url"));
  EXPECT_FALSE(ScopedUserProfile::IsValidPictureUrl(L""));

  // Valid host but empty or root path
  EXPECT_FALSE(ScopedUserProfile::IsValidPictureUrl(
      L"https://lh3.googleusercontent.com"));
  EXPECT_FALSE(ScopedUserProfile::IsValidPictureUrl(
      L"https://lh3.googleusercontent.com/"));
}

TEST(ScopedUserProfileStaticTest, BuildProfilePictureUrl) {
  // URL without suffix
  EXPECT_EQ("https://lh3.googleusercontent.com/a/abc=s96",
            ScopedUserProfile::BuildProfilePictureUrl(
                GURL("https://lh3.googleusercontent.com/a/abc"), 96));

  // URL with existing suffix
  EXPECT_EQ("https://lh3.googleusercontent.com/a/abc=s128",
            ScopedUserProfile::BuildProfilePictureUrl(
                GURL("https://lh3.googleusercontent.com/a/abc=s96"), 128));

  // Invalid URL
  EXPECT_EQ("",
            ScopedUserProfile::BuildProfilePictureUrl(GURL("not-a-url"), 96));
}

}  // namespace credential_provider
