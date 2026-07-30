// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_

class Profile;

class AmbientAuthenticationTestHelper {
 public:
  AmbientAuthenticationTestHelper() = default;
  static bool IsAmbientAuthAllowedForProfile(Profile* profile);
  static bool IsIncognitoAllowedInPolicy(int policy_value);
  static bool IsGuestAllowedInPolicy(int policy_value);
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_TEST_UTILS_H_
