// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/credential_provider_util.h"

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CredentialProviderUtilTest = PlatformTest;

// Test that the expected hash stays the same for the same URL.
TEST_F(CredentialProviderUtilTest, GetFaviconFileKey) {
  EXPECT_NSEQ(
      GetFaviconFileKey(GURL("https://login.yahoo.com/")),
      @"BD7639C34EA3480A8AAD704306C8870161761506AD948AC6FA037B83CFF22D37");
  EXPECT_NSEQ(
      GetFaviconFileKey(GURL("www.kijiji.ca")),
      @"E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
  EXPECT_NSEQ(
      GetFaviconFileKey(
          GURL("https://www.theweathernetwork.com/ca/weather/quebec/montreal")),
      @"A0F3B5AB4012A2EC0EA3AC950B6AD8982F6FF29DE632ECC3645D566E291E3D12");
  EXPECT_NSEQ(
      GetFaviconFileKey(GURL(
          "https://www.canadapost-postescanada.ca/track-reperage/en#/home")),
      @"9DD8ED2F4B375E5DDDEA137D8985FFD32521694331753E70AA815692CFE0653B");
}

}  // namespace
