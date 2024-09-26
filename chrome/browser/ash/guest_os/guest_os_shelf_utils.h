// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHELF_UTILS_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHELF_UTILS_H_

#include <string>
#include "base/strings/string_piece_forward.h"

class Profile;

namespace guest_os {

// Returns an app_id for an app on the shelf, given its exo |window_startup_id|
// or |window_app_id|.
//
// First try to return a desktop file id matching the |window_startup_id|.
//
// If the |window_app_id| is empty, returns empty string. If we can uniquely
// match a registry entry, returns the crostini app_id for that. Otherwise,
// returns the string pointed to by |window_app_id|, prefixed by "crostini:".
//
// As the |window_app_id| is derived from fields set by the app itself, it is
// possible for an app to masquerade as a different app.
std::string GetGuestOsShelfAppId(Profile* profile,
                                 const std::string* window_app_id,
                                 const std::string* window_startup_id);

// Returns whether the app_id belongs to an unregistered/generic Crostini app.
bool IsUnregisteredCrostiniShelfAppId(base::StringPiece shelf_app_id);

// Returns whether the app_id belongs to an unregistered/generic GuestOS app.
bool IsUnregisteredGuestOsShelfAppId(base::StringPiece shelf_app_id);

// Returns whether the app_id is a Crostini app id. This includes both
// registered and unregistered Crostini apps.
bool IsCrostiniShelfAppId(const Profile* profile,
                          base::StringPiece shelf_app_id);

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHELF_UTILS_H_
