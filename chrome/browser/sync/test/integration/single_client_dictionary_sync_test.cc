// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/dictionary_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/driver/sync_service_impl.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using testing::ElementsAre;
using testing::IsEmpty;

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/1296569): Re-enable when spell check dictionary issues are
// addressed.
#define MAYBE_SingleClientDictionarySyncTest \
  DISABLED_SingleClientDictionarySyncTest
#else
#define MAYBE_SingleClientDictionarySyncTest SingleClientDictionarySyncTest
#endif

class MAYBE_SingleClientDictionarySyncTest : public SyncTest {
 public:
  MAYBE_SingleClientDictionarySyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~MAYBE_SingleClientDictionarySyncTest() override = default;
};

IN_PROC_BROWSER_TEST_F(MAYBE_SingleClientDictionarySyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  dictionary_helper::LoadDictionaries();
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0), IsEmpty());

  const std::string word = "foo";
  EXPECT_TRUE(dictionary_helper::AddWord(0, word));
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0), ElementsAre(word));

  EXPECT_TRUE(dictionary_helper::RemoveWord(0, word));
  EXPECT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  EXPECT_THAT(dictionary_helper::GetDictionaryWords(0), IsEmpty());
}

}  // namespace
