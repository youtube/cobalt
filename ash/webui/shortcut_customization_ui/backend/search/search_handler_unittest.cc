// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom-test-utils.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept_registry.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::shortcut_ui {

namespace {

class FakeObserver
    : public shortcut_customization::mojom::SearchResultsAvailabilityObserver {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  mojo::PendingRemote<
      shortcut_customization::mojom::SearchResultsAvailabilityObserver>
  GenerateRemote() {
    mojo::PendingRemote<
        shortcut_customization::mojom::SearchResultsAvailabilityObserver>
        remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  size_t num_calls() const { return num_calls_; }

 private:
  // shortcut_customization::mojom::SearchResultsAvailabilityObserver:
  void OnSearchResultsAvailabilityChanged() override { ++num_calls_; }

  size_t num_calls_ = 0;
  mojo::Receiver<
      shortcut_customization::mojom::SearchResultsAvailabilityObserver>
      receiver_{this};
};

std::vector<shortcut_ui::SearchConcept> GetTestSearchConcepts() {
  std::vector<shortcut_ui::SearchConcept> concepts;

  {
    std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_info_list;
    accelerator_info_list.emplace_back(ash::mojom::AcceleratorInfo::New(
        /*type=*/ash::mojom::AcceleratorType::kDefault,
        /*state=*/ash::mojom::AcceleratorState::kEnabled,
        /*locked=*/true,
        /*layout_properties=*/
        ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
            ash::mojom::StandardAcceleratorProperties::New(
                ui::Accelerator(
                    /*key_code=*/ui::KeyboardCode::VKEY_SPACE,
                    /*modifiers=*/ui::EF_CONTROL_DOWN),
                u"Space"))));
    concepts.emplace_back(
        fake_search_data::CreateFakeAcceleratorLayoutInfo(
            /*description=*/u"Open launcher",
            /*source=*/ash::mojom::AcceleratorSource::kAsh,
            /*action=*/fake_search_data::FakeActionIds::kAction1,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
        std::move(accelerator_info_list));
  }

  {
    std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_info_list;
    accelerator_info_list.emplace_back(ash::mojom::AcceleratorInfo::New(
        /*type=*/ash::mojom::AcceleratorType::kDefault,
        /*state=*/ash::mojom::AcceleratorState::kEnabled,
        /*locked=*/true,
        /*layout_properties=*/
        ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
            ash::mojom::StandardAcceleratorProperties::New(
                ui::Accelerator(
                    /*key_code=*/ui::KeyboardCode::VKEY_T,
                    /*modifiers=*/ui::EF_CONTROL_DOWN),
                u"T"))));
    concepts.emplace_back(
        fake_search_data::CreateFakeAcceleratorLayoutInfo(
            /*description=*/u"Open new tab",
            /*source=*/ash::mojom::AcceleratorSource::kBrowser,
            /*action=*/fake_search_data::FakeActionIds::kAction2,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
        std::move(accelerator_info_list));
  }

  {
    std::vector<ash::mojom::AcceleratorInfoPtr> accelerator_info_list;
    accelerator_info_list.emplace_back(ash::mojom::AcceleratorInfo::New(
        /*type=*/ash::mojom::AcceleratorType::kDefault,
        /*state=*/ash::mojom::AcceleratorState::kEnabled,
        /*locked=*/true,
        /*layout_properties=*/
        ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
            ash::mojom::StandardAcceleratorProperties::New(
                ui::Accelerator(
                    /*key_code=*/ui::KeyboardCode::VKEY_A,
                    /*modifiers=*/ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
                u"A"))));
    accelerator_info_list.emplace_back(ash::mojom::AcceleratorInfo::New(
        /*type=*/ash::mojom::AcceleratorType::kDefault,
        /*state=*/ash::mojom::AcceleratorState::kEnabled,
        /*locked=*/true,
        /*layout_properties=*/
        ash::mojom::LayoutStyleProperties::NewStandardAccelerator(
            ash::mojom::StandardAcceleratorProperties::New(
                ui::Accelerator(
                    /*key_code=*/ui::KeyboardCode::VKEY_BRIGHTNESS_DOWN,
                    /*modifiers=*/ui::EF_ALT_DOWN),
                u"BrightnessDown"))));

    concepts.emplace_back(
        fake_search_data::CreateFakeAcceleratorLayoutInfo(
            /*description=*/u"Open the Foo app",
            /*source=*/ash::mojom::AcceleratorSource::kAsh,
            /*action=*/fake_search_data::kAction3,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kDefault),
        std::move(accelerator_info_list));
  }

  {
    // Create a TextAccelerator.
    std::vector<ash::mojom::TextAcceleratorPartPtr> text_parts;
    text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
        u"Press ", ash::mojom::TextAcceleratorPartType::kPlainText));
    text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
        u"Ctrl", ash::mojom::TextAcceleratorPartType::kModifier));
    text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
        u"+", ash::mojom::TextAcceleratorPartType::kDelimiter));
    text_parts.push_back(ash::mojom::TextAcceleratorPart::New(
        u"A", ash::mojom::TextAcceleratorPartType::kKey));

    std::vector<ash::mojom::AcceleratorInfoPtr> text_accelerator_info_list;
    text_accelerator_info_list.emplace_back(ash::mojom::AcceleratorInfo::New(
        /*type=*/ash::mojom::AcceleratorType::kDefault,
        /*state=*/ash::mojom::AcceleratorState::kEnabled,
        /*locked=*/true,
        /*layout_properties=*/
        ash::mojom::LayoutStyleProperties::NewTextAccelerator(
            ash::mojom::TextAcceleratorProperties::New(
                std::move(text_parts)))));

    // Add that TextAccelerator to the list of SearchConcepts.
    concepts.emplace_back(
        fake_search_data::CreateFakeAcceleratorLayoutInfo(
            /*description=*/u"Select all text content",
            /*source=*/ash::mojom::AcceleratorSource::kAsh,
            /*action=*/fake_search_data::FakeActionIds::kAction4,
            /*style=*/ash::mojom::AcceleratorLayoutStyle::kText),
        std::move(text_accelerator_info_list));
  }

  return concepts;
}

// Creates a search result with some default values.
shortcut_customization::mojom::SearchResultPtr CreateFakeSearchResult() {
  return shortcut_customization::mojom::SearchResult::New(
      /*accelerator_layout_info=*/fake_search_data::
          CreateFakeAcceleratorLayoutInfo(
              u"Open launcher", ash::mojom::AcceleratorSource::kAsh,
              fake_search_data::FakeActionIds::kAction1,
              ash::mojom::AcceleratorLayoutStyle::kDefault),
      /*accelerator_infos=*/fake_search_data::CreateFakeAcceleratorInfoList(),
      /*relevance_score=*/0.5);
}

}  // namespace

class SearchHandlerTest : public testing::Test {
 protected:
  SearchHandlerTest()
      : search_concept_registry_(*local_search_service_proxy_.get()),
        handler_(&search_concept_registry_, local_search_service_proxy_.get()) {
  }
  ~SearchHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kSearchInShortcutsApp},
        /*disabled_features=*/{});

    handler_.BindInterface(handler_remote_.BindNewPipeAndPassReceiver());
    handler_remote_->AddSearchResultsAvailabilityObserver(
        results_availability_observer_.GenerateRemote());
    handler_remote_.FlushForTesting();
  }

  void VerifySearchResultIsPresent(
      const std::u16string description,
      const std::vector<shortcut_customization::mojom::SearchResultPtr>&
          search_results) const {
    auto description_iterator = find_if(
        search_results.begin(), search_results.end(),
        [&description](
            const shortcut_customization::mojom::SearchResultPtr& result) {
          return result->accelerator_layout_info->description == description;
        });
    // The description should be present in the list of search results.
    EXPECT_NE(description_iterator, search_results.end());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_ =
          std::make_unique<local_search_service::LocalSearchServiceProxy>(
              /*for_testing=*/true);
  shortcut_ui::SearchConceptRegistry search_concept_registry_;
  mojo::Remote<shortcut_customization::mojom::SearchHandler> handler_remote_;
  shortcut_ui::SearchHandler handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeObserver results_availability_observer_;
};

TEST_F(SearchHandlerTest, SearchResultsNormalUsage) {
  search_concept_registry_.SetSearchConcepts(GetTestSearchConcepts());
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();

  // SearchHandler observer should be called after the registry is updated.
  EXPECT_EQ(1u, results_availability_observer_.num_calls());

  std::vector<shortcut_customization::mojom::SearchResultPtr> search_results;

  // A search with no matches should return no results.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"this search matches nothing!",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 0u);

  // The number of observer calls should not have changed, even though a search
  // was completed. The observer is only called when the availability of results
  // changes, i.e. when the index is updated.
  EXPECT_EQ(1u, results_availability_observer_.num_calls());

  // The descriptions for the fake shortcuts are "Open launcher", "Open new
  // tab", "Open the Foo app", and "Select all text content".
  // The query "Open" should match the first three Concepts.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Open",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 3u);
  VerifySearchResultIsPresent(/*description=*/u"Open launcher",
                              /*search_results=*/search_results);
  VerifySearchResultIsPresent(/*description=*/u"Open new tab",
                              /*search_results=*/search_results);
  VerifySearchResultIsPresent(/*description=*/u"Open the Foo app",
                              /*search_results=*/search_results);

  // Checking again that the observer was not called after the previous search.
  EXPECT_EQ(1u, results_availability_observer_.num_calls());

  // The query "open" should also match the first three Concepts (query case
  // doesn't matter).
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"open",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 3u);
  VerifySearchResultIsPresent(/*description=*/u"Open launcher",
                              /*search_results=*/search_results);
  VerifySearchResultIsPresent(/*description=*/u"Open new tab",
                              /*search_results=*/search_results);
  VerifySearchResultIsPresent(/*description=*/u"Open the Foo app",
                              /*search_results=*/search_results);

  // For completeness, the query "OpEn" should also match the first three
  // Concepts.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"OpEn",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 3u);
  VerifySearchResultIsPresent(/*description=*/u"Open launcher",
                              /*search_results=*/search_results);
  VerifySearchResultIsPresent(/*description=*/u"Open new tab",
                              /*search_results=*/search_results);
  VerifySearchResultIsPresent(/*description=*/u"Open the Foo app",
                              /*search_results=*/search_results);

  // Searching for a specific shortcut should only include those results.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Open new tab",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 1u);
  EXPECT_EQ(search_results.at(0)->accelerator_layout_info->description,
            u"Open new tab");

  // Searching for a specific shortcut should work even if the query is a
  // "fuzzy" match.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Open tab",
              /*max_num_results=*/5u, &search_results);
  // In this case, the search service also returns the result for "Open the Foo
  // app", but with a lower relevance_score.
  EXPECT_EQ(search_results.size(), 2u);
  EXPECT_EQ(search_results.at(0)->accelerator_layout_info->description,
            u"Open new tab");
  EXPECT_EQ(search_results.at(1)->accelerator_layout_info->description,
            u"Open the Foo app");
  // Expect that earlier search results have a higher relevance score.
  EXPECT_GT(search_results.at(0)->relevance_score,
            search_results.at(1)->relevance_score);

  // Clear the index and verify that searches return no results, and that the
  // observer was called an additional time.
  std::vector<SearchConcept> empty_search_concepts;
  search_concept_registry_.SetSearchConcepts(std::move(empty_search_concepts));
  task_environment_.RunUntilIdle();
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Open",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 0u);
  EXPECT_EQ(results_availability_observer_.num_calls(), 2u);
}

TEST_F(SearchHandlerTest, SearchResultsEdgeCases) {
  search_concept_registry_.SetSearchConcepts(GetTestSearchConcepts());
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();
  std::vector<shortcut_customization::mojom::SearchResultPtr> search_results;

  // A search with no matches should return no results.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"this search matches nothing!",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 0u);

  // Testing some edge cases: searching with spaces on either side and in the
  // middle.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"       Open     new    tab       ",
              /*max_num_results=*/5u, &search_results);
  // Turns out the search service doesn't match any shortcuts with a malformed
  // query like this.
  EXPECT_EQ(search_results.size(), 0u);

  // Searching with spaces on either side, but not in the middle.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"       Open new tab       ",
              /*max_num_results=*/5u, &search_results);
  // Turns out the search service doesn't match any shortcuts with a malformed
  // query like this.
  EXPECT_EQ(search_results.size(), 0u);
}

TEST_F(SearchHandlerTest, SearchResultsSearchByKeys) {
  search_concept_registry_.SetSearchConcepts(GetTestSearchConcepts());
  handler_remote_.FlushForTesting();
  task_environment_.RunUntilIdle();
  std::vector<shortcut_customization::mojom::SearchResultPtr> search_results;

  // Searching for the keys/modifiers of a shortcut should only work for text
  // accelerators. In this case, all shortcuts contain "Ctrl" as one of their
  // modifiers, but only one of them is a text accelerator, so only that one
  // should be returned.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"ctrl",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 1u);
  VerifySearchResultIsPresent(/*description=*/u"Select all text content",
                              /*search_results=*/search_results);

  // Verify that different various combinations of uppercase and lowercase work
  // when querying by modifier.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"CTRL",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 1u);
  VerifySearchResultIsPresent(/*description=*/u"Select all text content",
                              /*search_results=*/search_results);
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"CtRl",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 1u);
  VerifySearchResultIsPresent(/*description=*/u"Select all text content",
                              /*search_results=*/search_results);

  // The test concept "Open the Foo app" has "Alt + BrightnessDown" as one of
  // its key combinations. Searching based on "BrightnessDown" should not return
  // that shortcut as a SearchResult because that shortcut is a standard
  // accelerator.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"BrightnessDown",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 0u);

  // Searching for text-based shortcuts should work. In this case, the query
  // should match the shortcut "Select all text content" which has a shortcut
  // "Press Ctrl+A".
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Press Ctrl+A",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 1u);
  VerifySearchResultIsPresent(/*description=*/u"Select all text content",
                              /*search_results=*/search_results);

  // Searching for text-based shortcuts should work with an inexact query.
  shortcut_customization::mojom::SearchHandlerAsyncWaiter(handler_remote_.get())
      .Search(u"Press",
              /*max_num_results=*/5u, &search_results);
  EXPECT_EQ(search_results.size(), 1u);
  VerifySearchResultIsPresent(/*description=*/u"Select all text content",
                              /*search_results=*/search_results);
}

TEST_F(SearchHandlerTest, CompareSearchResults) {
  // Create two equal fake search results.
  shortcut_customization::mojom::SearchResultPtr a = CreateFakeSearchResult();
  shortcut_customization::mojom::SearchResultPtr b = CreateFakeSearchResult();

  // CompareSearchResults() returns true if the first parameter should be ranked
  // higher than the second parameter. On a tie, this method should return
  // false. Since the two fake search results are equal, it should return false
  // regardless of the order of parameters.
  EXPECT_FALSE(SearchHandler::CompareSearchResults(a, b));
  EXPECT_FALSE(SearchHandler::CompareSearchResults(b, a));

  // Differ only on relevance score.
  a->relevance_score = 0;
  b->relevance_score = 1;

  // Comparison value should differ now that the relevance scores are different.
  EXPECT_NE(SearchHandler::CompareSearchResults(b, a),
            SearchHandler::CompareSearchResults(a, b));
  // CompareSearchResults() returns whether the first parameter should be higher
  // ranked than the second parameter.
  EXPECT_FALSE(SearchHandler::CompareSearchResults(a, b));
  EXPECT_TRUE(SearchHandler::CompareSearchResults(b, a));

  // Differ only on relevance score, this time using less extreme values.
  a->relevance_score = 0.123;
  b->relevance_score = 0.789;

  // Comparison value should differ now that the relevance scores are different.
  EXPECT_NE(SearchHandler::CompareSearchResults(b, a),
            SearchHandler::CompareSearchResults(a, b));
  // CompareSearchResults() returns whether the first parameter should be higher
  // ranked than the second parameter.
  EXPECT_FALSE(SearchHandler::CompareSearchResults(a, b));
  EXPECT_TRUE(SearchHandler::CompareSearchResults(b, a));
}

}  // namespace ash::shortcut_ui