// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_ENTRY_TEST_HELPER_H_
#define COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_ENTRY_TEST_HELPER_H_

#include <stdint.h>

#include <string>

#include "base/time/time.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace base {
class Time;
}

namespace sessions {
namespace test_data {

extern const int kIndex;
extern const int kUniqueID;
extern const int kReferrerPolicy;
extern const std::u16string kTitle;
extern const std::string kEncodedPageState;
extern const ui::PageTransition kTransitionType;
extern const bool kHasPostData;
extern const int64_t kPostID;
extern const bool kIsOverridingUserAgent;
extern const base::Time kTimestamp;
extern const int kHttpStatusCode;
extern const SerializedNavigationEntry::PasswordState kPasswordState;
extern const std::string kExtendedInfoKey1;
extern const std::string kExtendedInfoKey2;
extern const std::string kExtendedInfoValue1;
extern const std::string kExtendedInfoValue2;
extern const int64_t kParentTaskId;
extern const int64_t kRootTaskId;
extern const int64_t kTaskId;

}  // namespace test_data

// Set of test functions to manipulate a SerializedNavigationEntry.
class SerializedNavigationEntryTestHelper {
 public:
  SerializedNavigationEntryTestHelper() = delete;
  SerializedNavigationEntryTestHelper(
      const SerializedNavigationEntryTestHelper&) = delete;
  SerializedNavigationEntryTestHelper& operator=(
      const SerializedNavigationEntryTestHelper&) = delete;

  // Compares the two entries. This uses EXPECT_XXX on each member, if your test
  // needs to stop after this wrap calls to this in EXPECT_NO_FATAL_FAILURE.
  static void ExpectNavigationEquals(const SerializedNavigationEntry& expected,
                                     const SerializedNavigationEntry& actual);

  // Creates a SerializedNavigationEntry using the |test_data| constants above.
  //
  // Note that the returned SerializedNavigationEntry will have a bogus
  // PageState and therefore can only be used in limited unit tests (e.g. it
  // will most likely hit DCHECKs/NOTREACHEDs when passed to the //content
  // layer).
  static SerializedNavigationEntry CreateNavigationForTest();

  static void SetReferrerPolicy(int policy,
                                SerializedNavigationEntry* navigation);

  static void SetVirtualURL(const GURL& virtual_url,
                            SerializedNavigationEntry* navigation);

  static void SetEncodedPageState(const std::string& encoded_page_state,
                                  SerializedNavigationEntry* navigation);

  static void SetTransitionType(ui::PageTransition transition_type,
                                SerializedNavigationEntry* navigation);

  static void SetHasPostData(bool has_post_data,
                             SerializedNavigationEntry* navigation);

  static void SetOriginalRequestURL(const GURL& original_request_url,
                                    SerializedNavigationEntry* navigation);

  static void SetIsOverridingUserAgent(bool is_overriding_user_agent,
                                       SerializedNavigationEntry* navigation);

  static void SetTimestamp(base::Time timestamp,
                           SerializedNavigationEntry* navigation);

  static void SetReplacedEntryData(
      const SerializedNavigationEntry::ReplacedNavigationEntryData& data,
      SerializedNavigationEntry* navigation);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SERIALIZED_NAVIGATION_ENTRY_TEST_HELPER_H_
