// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_UTILS_H_

class PrefService;

namespace set_up_list_utils {

// Returns `true` if the SetUpList should be active based on how long it has
// been since the user finished the FRE, and whether it has been disabled via
// a local state pref. The check excludes the disabled pref if
// `include_disable_pref` is false.
bool IsSetUpListActive(PrefService* local_state,
                       bool include_disable_pref = true);

// true if the Set Up List should be shown in a compacted layout in the Magic
// Stack.
bool ShouldShowCompactedSetUpListModule();

}  // namespace set_up_list_utils

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_UTILS_H_
