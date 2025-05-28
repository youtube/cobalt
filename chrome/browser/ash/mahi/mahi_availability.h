// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_

namespace ash::mahi_availability {

// Check whether Mahi is allowed. This function checks following restrictions:
//   * age: if not demo mode, the account must not hit minor restrictions
//   * country: the country code must be in the allow list.
//   * If not in demo mode, guest session is not allowed.
bool CanUseMahiService();

// Check if the mahi feature is available to use. It can be unavailable if the
// mahi feature flag is disabled, or the age and country requirements are not
// met.
bool IsMahiAvailable();

// Check if the Pompano feature is available to use.
// Pompano is an add-on feature of mahi. Currently we make it available only
// when mahi is available AND its own feature flag is enabled.
bool IsPompanoAvailable();

}  // namespace ash::mahi_availability

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_AVAILABILITY_H_
