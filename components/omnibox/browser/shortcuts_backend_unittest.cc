// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_backend.h"

#include <stddef.h>

#include <iterator>
#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/fake_autocomplete_provider.h"
#include "components/omnibox/browser/shortcuts_constants.h"
#include "components/omnibox/browser/shortcuts_database.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// ShortcutsBackendTest -------------------------------------------------------

class ShortcutsBackendTest : public testing::Test,
                             public ShortcutsBackend::ShortcutsBackendObserver {
 public:
  ShortcutsBackendTest();
  ShortcutsBackendTest(const ShortcutsBackendTest&) = delete;
  ShortcutsBackendTest& operator=(const ShortcutsBackendTest&) = delete;

  ShortcutsDatabase::Shortcut::MatchCore MatchCoreForTesting(
      const std::string& url,
      const std::string& contents_class = std::string(),
      const std::string& description_class = std::string(),
      AutocompleteMatch::Type type = AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  void SetSearchProvider();

  void SetUp() override;
  void TearDown() override;

  void OnShortcutsLoaded() override;
  void OnShortcutsChanged() override;

  const ShortcutsBackend::ShortcutMap& shortcuts_map() const {
    return backend_->shortcuts_map();
  }
  bool changed_notified() const { return changed_notified_; }
  void set_changed_notified(bool changed_notified) {
    changed_notified_ = changed_notified;
  }

  void InitBackend();
  bool AddShortcut(const ShortcutsDatabase::Shortcut& shortcut);
  bool UpdateShortcut(const ShortcutsDatabase::Shortcut& shortcut);
  bool DeleteShortcutsWithURL(const GURL& url);
  bool DeleteShortcutsWithIDs(
      const ShortcutsDatabase::ShortcutIDs& deleted_ids);
  bool ShortcutExists(const std::u16string& terms) const;
  std::vector<std::u16string> ShortcutsMapKeys() const;
  const ShortcutsDatabase::Shortcut FindShortcut(
      const std::u16string& terms) const;
  void ClearShortcutsMap();

  TemplateURLService* GetTemplateURLService();

  ShortcutsBackend* backend() { return backend_.get(); }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
  }

 private:
  base::ScopedTempDir profile_dir_;
  // `scoped_feature_list_` needs to be destroyed after TaskEnvironment is
  // destroyed, so that other threads won't access the feature list while it is
  // being destroyed.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<history::HistoryService> history_service_;

  scoped_refptr<ShortcutsBackend> backend_;

  bool load_notified_ = false;
  bool changed_notified_ = false;
};

ShortcutsBackendTest::ShortcutsBackendTest() = default;

ShortcutsDatabase::Shortcut::MatchCore
ShortcutsBackendTest::MatchCoreForTesting(const std::string& url,
                                          const std::string& contents_class,
                                          const std::string& description_class,
                                          AutocompleteMatch::Type type) {
  AutocompleteMatch match(nullptr, 0, false, type);
  match.destination_url = GURL(url);
  match.contents = u"test";
  match.contents_class =
      AutocompleteMatch::ClassificationsFromString(contents_class);
  match.description_class =
      AutocompleteMatch::ClassificationsFromString(description_class);
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(match.contents);
  SearchTermsData search_terms_data;
  return ShortcutsBackend::MatchToMatchCore(match, template_url_service_.get(),
                                            &search_terms_data);
}

void ShortcutsBackendTest::SetSearchProvider() {
  TemplateURLData data;
  data.SetURL("http://foo.com/search?bar={searchTerms}");
  data.SetShortName(u"foo");
  data.SetKeyword(u"foo");

  TemplateURL* template_url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
}

void ShortcutsBackendTest::SetUp() {
  ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
  template_url_service_ = std::make_unique<TemplateURLService>(nullptr, 0);
  history_service_ =
      history::CreateHistoryService(profile_dir_.GetPath(), true);
  ASSERT_TRUE(history_service_);

  base::FilePath shortcuts_database_path =
      profile_dir_.GetPath().Append(kShortcutsDatabaseName);
  backend_ = new ShortcutsBackend(
      template_url_service_.get(), std::make_unique<SearchTermsData>(),
      history_service_.get(), shortcuts_database_path, false);
  ASSERT_TRUE(backend_.get());
  backend_->AddObserver(this);
}

void ShortcutsBackendTest::TearDown() {
  backend_->RemoveObserver(this);
  backend_->ShutdownOnUIThread();
  backend_.reset();

  // Explicitly shut down the history service and wait for its backend to be
  // destroyed to prevent resource leaks.
  base::RunLoop run_loop;
  history_service_->SetOnBackendDestroyTask(run_loop.QuitClosure());
  history_service_->Shutdown();
  run_loop.Run();

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(profile_dir_.Delete());
}

void ShortcutsBackendTest::OnShortcutsLoaded() {
  load_notified_ = true;
}

void ShortcutsBackendTest::OnShortcutsChanged() {
  changed_notified_ = true;
}

void ShortcutsBackendTest::InitBackend() {
  ASSERT_TRUE(backend_);
  ASSERT_FALSE(load_notified_);
  ASSERT_FALSE(backend_->initialized());
  backend_->Init();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(load_notified_);
  EXPECT_TRUE(backend_->initialized());
}

bool ShortcutsBackendTest::AddShortcut(
    const ShortcutsDatabase::Shortcut& shortcut) {
  return backend_->AddShortcut(shortcut);
}

bool ShortcutsBackendTest::UpdateShortcut(
    const ShortcutsDatabase::Shortcut& shortcut) {
  return backend_->UpdateShortcut(shortcut);
}

bool ShortcutsBackendTest::DeleteShortcutsWithURL(const GURL& url) {
  return backend_->DeleteShortcutsWithURL(url);
}

bool ShortcutsBackendTest::DeleteShortcutsWithIDs(
    const ShortcutsDatabase::ShortcutIDs& deleted_ids) {
  return backend_->DeleteShortcutsWithIDs(deleted_ids);
}

bool ShortcutsBackendTest::ShortcutExists(const std::u16string& terms) const {
  return shortcuts_map().find(terms) != shortcuts_map().end();
}

std::vector<std::u16string> ShortcutsBackendTest::ShortcutsMapKeys() const {
  std::vector<std::u16string> keys;
  base::ranges::transform(shortcuts_map(), std::back_inserter(keys),
                          [](const auto& entry) { return entry.first; });
  return keys;
}

const ShortcutsDatabase::Shortcut ShortcutsBackendTest::FindShortcut(
    const std::u16string& terms) const {
  const auto iter = shortcuts_map().find(terms);
  return iter == shortcuts_map().end() ? ShortcutsDatabase::Shortcut{}
                                       : iter->second;
}

void ShortcutsBackendTest::ClearShortcutsMap() {
  backend_->shortcuts_map_.clear();
}

TemplateURLService* ShortcutsBackendTest::GetTemplateURLService() {
  return template_url_service_.get();
}

// Actual tests ---------------------------------------------------------------

// Verifies that creating MatchCores strips classifications and sanitizes match
// types.
TEST_F(ShortcutsBackendTest, SanitizeMatchCore) {
  struct {
    std::string input_contents_class;
    std::string input_description_class;
    AutocompleteMatch::Type input_type;
    std::string output_contents_class;
    std::string output_description_class;
    AutocompleteMatch::Type output_type;
  } cases[] = {
      {"0,1,4,0", "0,3,4,1", AutocompleteMatchType::URL_WHAT_YOU_TYPED,
       "0,1,4,0", "0,1", AutocompleteMatchType::HISTORY_URL},
      {"0,3,5,1", "0,2,5,0", AutocompleteMatchType::NAVSUGGEST, "0,1", "0,0",
       AutocompleteMatchType::HISTORY_URL},
      {"0,1", "0,0,11,2,15,0", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
       "0,1", "0,0", AutocompleteMatchType::SEARCH_HISTORY},
      {"0,1", "0,0", AutocompleteMatchType::SEARCH_SUGGEST, "0,1", "0,0",
       AutocompleteMatchType::SEARCH_HISTORY},
      {"0,1", "0,0", AutocompleteMatchType::SEARCH_SUGGEST_ENTITY, "", "",
       AutocompleteMatchType::SEARCH_HISTORY},
      {"0,1", "0,0", AutocompleteMatchType::SEARCH_SUGGEST_TAIL, "", "",
       AutocompleteMatchType::SEARCH_HISTORY},
      {"0,1", "0,0", AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED, "", "",
       AutocompleteMatchType::SEARCH_HISTORY},
      {"0,1", "0,0", AutocompleteMatchType::SEARCH_SUGGEST_PROFILE, "", "",
       AutocompleteMatchType::SEARCH_HISTORY},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    ShortcutsDatabase::Shortcut::MatchCore match_core(MatchCoreForTesting(
        std::string(), cases[i].input_contents_class,
        cases[i].input_description_class, cases[i].input_type));
    EXPECT_EQ(cases[i].output_contents_class, match_core.contents_class)
        << ":i:" << i << ":type:" << cases[i].input_type;
    EXPECT_EQ(cases[i].output_description_class, match_core.description_class)
        << ":i:" << i << ":type:" << cases[i].input_type;
    EXPECT_EQ(cases[i].output_type, match_core.type)
        << ":i:" << i << ":type:" << cases[i].input_type;
  }
}

TEST_F(ShortcutsBackendTest, EntitySuggestionTest) {
  SetSearchProvider();
  AutocompleteMatch match;
  match.fill_into_edit = u"franklin d roosevelt";
  match.type = AutocompleteMatchType::SEARCH_SUGGEST_ENTITY;
  match.contents = u"roosevelt";
  match.contents_class =
      AutocompleteMatch::ClassificationsFromString("0,0,5,2");
  match.description = u"Franklin D. Roosevelt";
  match.description_class = AutocompleteMatch::ClassificationsFromString("0,4");
  match.destination_url =
      GURL("http://www.foo.com/search?bar=franklin+d+roosevelt&gs_ssp=1234");
  match.keyword = u"foo";
  match.search_terms_args =
      std::make_unique<TemplateURLRef::SearchTermsArgs>(match.fill_into_edit);

  SearchTermsData search_terms_data;
  ShortcutsDatabase::Shortcut::MatchCore match_core =
      ShortcutsBackend::MatchToMatchCore(match, GetTemplateURLService(),
                                         &search_terms_data);
  EXPECT_EQ("http://foo.com/search?bar=franklin+d+roosevelt",
            match_core.destination_url.spec());
  EXPECT_EQ(match.fill_into_edit, match_core.contents);
  EXPECT_EQ("0,0", match_core.contents_class);
  EXPECT_EQ(std::u16string(), match_core.description);
  EXPECT_TRUE(match_core.description_class.empty());
}

TEST_F(ShortcutsBackendTest, MatchCoreDescriptionTest) {
  // When match.description_for_shortcuts is empty, match_core should use
  // match.description.
  {
    AutocompleteMatch match;
    match.description = u"the cat";
    match.description_class =
        AutocompleteMatch::ClassificationsFromString("0,1");

    SearchTermsData search_terms_data;
    ShortcutsDatabase::Shortcut::MatchCore match_core =
        ShortcutsBackend::MatchToMatchCore(match, GetTemplateURLService(),
                                           &search_terms_data);
    EXPECT_EQ(match_core.description, match.description);
    EXPECT_EQ(
        match_core.description_class,
        AutocompleteMatch::ClassificationsToString(match.description_class));
  }

  // When match.description_for_shortcuts is set, match_core should use it
  // instead of match.description.
  {
    AutocompleteMatch match;
    match.description = u"the cat";
    match.description_class =
        AutocompleteMatch::ClassificationsFromString("0,1");
    match.description_for_shortcuts = u"the elephant";
    match.description_class_for_shortcuts =
        AutocompleteMatch::ClassificationsFromString("0,4");

    SearchTermsData search_terms_data;
    ShortcutsDatabase::Shortcut::MatchCore match_core =
        ShortcutsBackend::MatchToMatchCore(match, GetTemplateURLService(),
                                           &search_terms_data);
    EXPECT_EQ(match_core.description, match.description_for_shortcuts);
    EXPECT_EQ(match_core.description_class,
              AutocompleteMatch::ClassificationsToString(
                  match.description_class_for_shortcuts));
  }
}

TEST_F(ShortcutsBackendTest, AddAndUpdateShortcut) {
  InitBackend();
  EXPECT_FALSE(changed_notified());

  ShortcutsDatabase::Shortcut shortcut(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", u"goog",
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut));
  EXPECT_TRUE(changed_notified());
  auto shortcut_iter(shortcuts_map().find(shortcut.text));
  ASSERT_TRUE(shortcut_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut.id, shortcut_iter->second.id);
  EXPECT_EQ(shortcut.match_core.contents,
            shortcut_iter->second.match_core.contents);

  set_changed_notified(false);
  shortcut.match_core.contents = u"Google Web Search";
  EXPECT_TRUE(UpdateShortcut(shortcut));
  EXPECT_TRUE(changed_notified());
  shortcut_iter = shortcuts_map().find(shortcut.text);
  ASSERT_TRUE(shortcut_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut.id, shortcut_iter->second.id);
  EXPECT_EQ(shortcut.match_core.contents,
            shortcut_iter->second.match_core.contents);
}

TEST_F(ShortcutsBackendTest, DeleteShortcuts) {
  InitBackend();
  ShortcutsDatabase::Shortcut shortcut1(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", u"goog",
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut1));

  ShortcutsDatabase::Shortcut shortcut2(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E0", u"gle",
      MatchCoreForTesting("http://www.google.com"), base::Time::Now(), 100);
  EXPECT_TRUE(AddShortcut(shortcut2));

  ShortcutsDatabase::Shortcut shortcut3(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E1", u"sp",
      MatchCoreForTesting("http://www.sport.com"), base::Time::Now(), 10);
  EXPECT_TRUE(AddShortcut(shortcut3));

  ShortcutsDatabase::Shortcut shortcut4(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E2", u"mov",
      MatchCoreForTesting("http://www.film.com"), base::Time::Now(), 10);
  EXPECT_TRUE(AddShortcut(shortcut4));

  ASSERT_EQ(4U, shortcuts_map().size());
  EXPECT_EQ(shortcut1.id, shortcuts_map().find(shortcut1.text)->second.id);
  EXPECT_EQ(shortcut2.id, shortcuts_map().find(shortcut2.text)->second.id);
  EXPECT_EQ(shortcut3.id, shortcuts_map().find(shortcut3.text)->second.id);
  EXPECT_EQ(shortcut4.id, shortcuts_map().find(shortcut4.text)->second.id);

  EXPECT_TRUE(DeleteShortcutsWithURL(shortcut1.match_core.destination_url));

  ASSERT_EQ(2U, shortcuts_map().size());
  EXPECT_EQ(0U, shortcuts_map().count(shortcut1.text));
  EXPECT_EQ(0U, shortcuts_map().count(shortcut2.text));
  const ShortcutsBackend::ShortcutMap::const_iterator shortcut3_iter(
      shortcuts_map().find(shortcut3.text));
  ASSERT_TRUE(shortcut3_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut3.id, shortcut3_iter->second.id);
  const ShortcutsBackend::ShortcutMap::const_iterator shortcut4_iter(
      shortcuts_map().find(shortcut4.text));
  ASSERT_TRUE(shortcut4_iter != shortcuts_map().end());
  EXPECT_EQ(shortcut4.id, shortcut4_iter->second.id);

  ShortcutsDatabase::ShortcutIDs deleted_ids;
  deleted_ids.push_back(shortcut3.id);
  deleted_ids.push_back(shortcut4.id);
  EXPECT_TRUE(DeleteShortcutsWithIDs(deleted_ids));

  ASSERT_EQ(0U, shortcuts_map().size());
}

TEST_F(ShortcutsBackendTest, AddOrUpdateShortcut_3CharShortening) {
  scoped_feature_list().InitAndEnableFeature(omnibox::kShortcutExpanding);
  InitBackend();

  AutocompleteMatch match;
  match.destination_url = GURL("https://www.google.com");

  // Should not have a shortcut initially.
  EXPECT_EQ(shortcuts_map().size(), 0u);
  EXPECT_FALSE(ShortcutExists(u"we need very long text for this test"));

  // Should have shortcut after shortcut is added to a match.
  backend()->AddOrUpdateShortcut(u"we need very long text for this test",
                                 match);
  EXPECT_EQ(shortcuts_map().size(), 1u);
  EXPECT_TRUE(ShortcutExists(u"we need very long text for this test"));

  // Should not shorten shortcut when a slightly shorter input (less than 3
  // chars shorter) is used for the match.
  backend()->AddOrUpdateShortcut(u"we need very long text for this t", match);
  EXPECT_EQ(shortcuts_map().size(), 1u);
  EXPECT_FALSE(ShortcutExists(u"we need very long text for this t"));
  EXPECT_TRUE(ShortcutExists(u"we need very long text for this test"));

  // Should shorten shortcut when a shorter input is used for the match.
  backend()->AddOrUpdateShortcut(u"we need very long", match);
  EXPECT_EQ(shortcuts_map().size(), 1u);
  EXPECT_TRUE(ShortcutExists(u"we need very long text"));
  EXPECT_FALSE(ShortcutExists(u"we need very long text for this test"));

  // Should add new shortcut when a longer input is used for the match.
  backend()->AddOrUpdateShortcut(u"we need very long text for this test",
                                 match);
  EXPECT_EQ(shortcuts_map().size(), 2u);
  EXPECT_TRUE(ShortcutExists(u"we need very long text"));
  EXPECT_TRUE(ShortcutExists(u"we need very long text for this test"));

  // Should shorten shortcut when a shorter input is used for the match. The
  // shorter shortcut to the same match should remain.
  backend()->AddOrUpdateShortcut(u"we need very long text fo", match);
  EXPECT_EQ(shortcuts_map().size(), 2u);
  EXPECT_TRUE(ShortcutExists(u"we need very long text"));
  EXPECT_TRUE(ShortcutExists(u"we need very long text for this"));
  EXPECT_FALSE(ShortcutExists(u"we need very long text for this test"));

  // Should only touch the shortest shortcut. The longer shortcut to the same
  // match should remain.
  backend()->AddOrUpdateShortcut(u"we", match);
  EXPECT_EQ(shortcuts_map().size(), 2u);
  EXPECT_TRUE(ShortcutExists(u"we need"));
  EXPECT_FALSE(ShortcutExists(u"we need very long text"));
  EXPECT_TRUE(ShortcutExists(u"we need very long text for this"));
}

TEST_F(ShortcutsBackendTest, AddOrUpdateShortcut_Expanding) {
  scoped_feature_list().InitAndEnableFeature(omnibox::kShortcutExpanding);
  InitBackend();

  AutocompleteMatch match;
  match.destination_url = GURL("https://www.host-shared2.com/path");
  match.description = u"https://www.description.com";
  match.contents = u"a an app apple i it word ZaZaaZZ symbols(╯°□°）╯ shared1";
  match.contents_class.emplace_back(0, 0);
  match.description_class.emplace_back(0, 0);

  // Should not have a shortcut initially.
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre());

  // Should expand last word when creating shortcuts.
  backend()->AddOrUpdateShortcut(u"w w", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"w word"));

  // Should expand last word when updating shortcuts.
  backend()->AddOrUpdateShortcut(u"w w", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"w word"));
  ClearShortcutsMap();

  // Should not expand other words when the last word is already expanded.
  backend()->AddOrUpdateShortcut(u"w word", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"w word"));
  ClearShortcutsMap();

  // Should prefer to expand to words at least 3 chars long. Should pick words
  // that come first, if there are multiple matches at least 3 chars long.
  backend()->AddOrUpdateShortcut(u"a", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"app"));
  ClearShortcutsMap();

  // Should prefer to expand to words that come first if all matches are shorter
  // than 3 chars long.
  backend()->AddOrUpdateShortcut(u"i", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"i"));
  ClearShortcutsMap();

  // Should not expand to words in the `description`.
  backend()->AddOrUpdateShortcut(u"d", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"d"));
  ClearShortcutsMap();

  // Should not expand to words in the URL path.
  backend()->AddOrUpdateShortcut(u"p", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"p"));
  ClearShortcutsMap();

  // Should expand to words in the URL host.
  backend()->AddOrUpdateShortcut(u"h", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"host"));
  ClearShortcutsMap();

  // Should prefer expanding to words in the `contents`.
  backend()->AddOrUpdateShortcut(u"shar", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"shared1"));
  ClearShortcutsMap();

  // When updating, should expand the last word after appending up to 3 chars.
  backend()->AddOrUpdateShortcut(u"an apple a", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"an apple app"));
  // 'an appl[e a]' should be expanded to 'an apple app'.
  backend()->AddOrUpdateShortcut(u"an appl", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"an apple app"));
  // But 'an app[le ]' should be expanded to 'an apple', removing the word 'app'
  // and trailing whitespace.
  backend()->AddOrUpdateShortcut(u"an app", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"an apple"));
  ClearShortcutsMap();

  // Should be case-insensitive when matching, but preserve case when creating
  // shortcut text. The match word is 'ZaZaaZZ'.
  // '[zAz][aaZZ]': the matched shortcut text should preserve the case of the
  // input, while the expanded shortcut text should preserve the case of the
  // match title.
  backend()->AddOrUpdateShortcut(u"zAz", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"zazaazz"));
  EXPECT_EQ(FindShortcut(u"zazaazz").text, u"zAzaaZZ");

  // When updating, '[Z][Az][aaZZ]': the matched shortcut text should preserve
  // the case of the input, while the expanded shortcut text should preserve the
  // case of the previous shortcut text rather than the match title.
  backend()->AddOrUpdateShortcut(u"Z", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"zazaazz"));
  EXPECT_EQ(FindShortcut(u"zazaazz").text, u"ZAzaaZZ");
  ClearShortcutsMap();

  // Should match inconsistent trailing whitespace and expand the last word
  // correctly.
  backend()->AddOrUpdateShortcut(u"appl   ", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"apple"));
  // Likewise for updating shortcuts.
  backend()->AddOrUpdateShortcut(u"appl   ", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"apple"));
  ClearShortcutsMap();

  // Should neither crash nor expand texts containing no words. Should still add
  // the text if it contains symbols, but not if it contains only whitespace.
  backend()->AddOrUpdateShortcut(u"(╯°□°", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"(╯°□°"));
  ClearShortcutsMap();
  backend()->AddOrUpdateShortcut(u"    ", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre());
  ClearShortcutsMap();

  // Should not expand words with trailing word-breaking symbols.
  backend()->AddOrUpdateShortcut(u"symb°□°", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"symb°□°"));
  ClearShortcutsMap();

  // Should expand words following symbols.
  backend()->AddOrUpdateShortcut(u"(╯°□°）╯symb", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"(╯°□°）╯symbols"));
  ClearShortcutsMap();

  // Should neither crash nor add a shortcut when text is empty.
  backend()->AddOrUpdateShortcut(u"", match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre());

  // Should not expand when match contents is empty.
  AutocompleteMatch match_without_contents;
  match_without_contents.destination_url = GURL("https://www.host.com/google");
  match_without_contents.description = u"google";
  match_without_contents.description_class.emplace_back(0, 0);
  backend()->AddOrUpdateShortcut(u"goo", match_without_contents);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"goo"));
  ClearShortcutsMap();

  // Should expand with description when `swap_contents_and_description` is
  // true.
  AutocompleteMatch swapped_match;
  swapped_match.swap_contents_and_description = true;
  swapped_match.destination_url = GURL("https://www.google.com");
  swapped_match.contents = u"https://www.googlecontents.com";
  swapped_match.description = u"googledescription";
  swapped_match.contents_class.emplace_back(0, 0);
  swapped_match.description_class.emplace_back(0, 0);
  backend()->AddOrUpdateShortcut(u"goo", swapped_match);
  EXPECT_THAT(ShortcutsMapKeys(), testing::ElementsAre(u"googledescription"));
  ClearShortcutsMap();
}

TEST_F(ShortcutsBackendTest, AddOrUpdateShortcut_Expanding_Prefix) {
  // Test `ExpandToFullWord`, specifically focussing on detecting and handling
  // the cases when `text` is a prefix of `match_text`.

  scoped_feature_list().InitAndEnableFeature(omnibox::kShortcutExpanding);
  InitBackend();

  const auto test = [&](const std::string& text, const std::string& match_text,
                        const std::string& expected_expanded_text) {
    SCOPED_TRACE("Text: " + text + ", match_text: " + match_text);
    AutocompleteMatch match;
    match.contents = base::UTF8ToUTF16(match_text);
    match.contents_class.emplace_back(0, 0);

    // Should expand last word when creating shortcuts.
    backend()->AddOrUpdateShortcut(base::UTF8ToUTF16(text), match);
    EXPECT_THAT(
        ShortcutsMapKeys(),
        testing::ElementsAre(base::UTF8ToUTF16(expected_expanded_text)));

    ClearShortcutsMap();
  };

  // When `text` does prefix `match_text`, should expand to the next word in
  // `match_text` instead of the 1st matching word.
  test("x", "x1 x2", "x1");
  test("x1 x", "x1 x2", "x1 x2");

  // When `text` doesn't prefix `match_text`, should expand to the 1st matching
  // word in `match_text`.
  test("x2 x", "x1 x2", "x2 x1");
  // Even if that produces repeated words. (It'd be too complicated to avoid
  // this without introducing even greater edge cases).
  test("x1 x", "x1 y x2", "x1 x1");

  // When prefix matching, should use the next word even if its short.
  test("x1 x", "x1 y x2 xyz", "x1 xyz");
  // When not prefix matching, should use the 1st word at least 3 chars long if
  // available.
  test("x1 x", "x1 x2 xyz", "x1 x2");

  // Both prefix and non prefix matching should handle trailing whitespace.
  // Trailing whitespace should not prompt expansion to the next `match_text`.
  test("x1 ", "x1 x2", "x1");
  // Trailing whitespace should not prevent expansion of the last `text` word.
  test("x1 xy ", "x1 xyz", "x1 xyz");
  test("x1 xyz ", "x1 xyz", "x1 xyz");
}
