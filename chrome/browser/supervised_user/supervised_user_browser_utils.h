
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_

#include <string>

#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class ProfileSelections;
class Profile;

namespace supervised_user {

// Returns true if both the extensions are enabled and the provided url is a
// Webstore or Download url.
bool IsSupportedChromeExtensionURL(const GURL& effective_url);

// Returns true if the parent allowlist should be skipped.
bool ShouldContentSkipParentAllowlistFiltering(content::WebContents* contents);

// Returns how supervised_user factories that are needed in Guest profile
// should be created.
ProfileSelections BuildProfileSelectionsForRegularAndGuest();

// Returns given name of the primary account associated with the profile.
std::string GetAccountGivenName(Profile& profile);

// Asserts that `is_child` matches the child status of the primary user.
// Terminates user session in case of status mismatch in order to prevent
// supervision incidents. Relevant on Chrome OS platform that has the concept
// of the user.
void AssertChildStatusOfTheUser(Profile* profile, bool is_child);

}  // namespace supervised_user

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_BROWSER_UTILS_H_
