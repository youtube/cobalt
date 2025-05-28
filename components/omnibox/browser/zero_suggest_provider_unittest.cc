// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_provider.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/history/core/browser/top_sites.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/browser/zero_suggest_cache_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "net/http/http_util.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

using testing::_;
using CacheEntry = ZeroSuggestCacheService::CacheEntry;
constexpr bool is_ios = !!BUILDFLAG(IS_IOS);

namespace {

constexpr int kCacheSize = 10;

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient() {
    ZeroSuggestProvider::RegisterProfilePrefs(
        search_engines_test_environment_.pref_service().registry());
    zero_suggest_cache_service_ = std::make_unique<ZeroSuggestCacheService>(
        std::make_unique<TestSchemeClassifier>(),
        &search_engines_test_environment_.pref_service(), kCacheSize);
  }
  FakeAutocompleteProviderClient(const FakeAutocompleteProviderClient&) =
      delete;
  FakeAutocompleteProviderClient& operator=(
      const FakeAutocompleteProviderClient&) = delete;

  bool SearchSuggestEnabled() const override { return true; }

  TemplateURLService* GetTemplateURLService() override {
    return search_engines_test_environment_.template_url_service();
  }

  const TemplateURLService* GetTemplateURLService() const override {
    return search_engines_test_environment_.template_url_service();
  }

  PrefService* GetPrefs() const override {
    return &search_engines_test_environment_.pref_service();
  }

  ZeroSuggestCacheService* GetZeroSuggestCacheService() override {
    return zero_suggest_cache_service_.get();
  }

  const ZeroSuggestCacheService* GetZeroSuggestCacheService() const override {
    return zero_suggest_cache_service_.get();
  }

  bool IsUrlDataCollectionActive() const override {
    return is_url_data_collection_active_;
  }

  void set_is_url_data_collection_active(bool is_url_data_collection_active) {
    is_url_data_collection_active_ = is_url_data_collection_active;
  }

  void Classify(
      const std::u16string& text,
      bool prefer_keyword,
      bool allow_exact_keyword_match,
      metrics::OmniboxEventProto::PageClassification page_classification,
      AutocompleteMatch* match,
      GURL* alternate_nav_url) override {
    // Populate enough of |match| to keep the ZeroSuggestProvider happy.
    match->type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
    match->destination_url = GURL(text);
  }

  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override {
    return scheme_classifier_;
  }

 private:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  bool is_url_data_collection_active_;
  std::unique_ptr<ZeroSuggestCacheService> zero_suggest_cache_service_;
  TestSchemeClassifier scheme_classifier_;
};

}  // namespace

class ZeroSuggestProviderTest : public testing::Test,
                                public AutocompleteProviderListener {
 public:
  ZeroSuggestProviderTest() = default;
  ZeroSuggestProviderTest(const ZeroSuggestProviderTest&) = delete;
  ZeroSuggestProviderTest& operator=(const ZeroSuggestProviderTest&) = delete;

  void SetUp() override;

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override;

  network::TestURLLoaderFactory* test_loader_factory() {
    return client_->test_url_loader_factory();
  }

  GURL GetSuggestURL(
      metrics::OmniboxEventProto::PageClassification page_classification,
      metrics::OmniboxFocusType focus_type,
      const std::string& page_url) {
    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.page_classification = page_classification;
    search_terms_args.focus_type = focus_type;
    search_terms_args.current_page_url = page_url;

    TemplateURLService* template_url_service = client_->GetTemplateURLService();
    return RemoteSuggestionsService::EndpointUrl(
        template_url_service->GetDefaultSearchProvider(), search_terms_args,
        template_url_service->search_terms_data());
  }

  // An AutocompleteInput that gets Zero Prefix Suggestions on NTP.
  AutocompleteInput ZeroPrefixInputForNTP(const bool is_prefetch) {
    AutocompleteInput input(u"",
                            is_prefetch
                                ? metrics::OmniboxEventProto::NTP_ZPS_PREFETCH
                                : metrics::OmniboxEventProto::NTP_REALBOX,
                            TestSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  // An AutocompleteInput that gets Prefix Suggestions on NTP.
  AutocompleteInput PrefixInputForNTP() {
    AutocompleteInput input(u"foobar", metrics::OmniboxEventProto::NTP_REALBOX,
                            TestSchemeClassifier());
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    return input;
  }

  // An AutocompleteInput that gets Zero Prefix Suggestions on WEB.
  AutocompleteInput ZeroPrefixInputForWeb(
      const bool is_prefetch,
      const std::string& input_url = "https://example.com/") {
    // On IOS WEB/SRP, input text is not empty.
    AutocompleteInput input(is_ios ? base::ASCIIToUTF16(input_url) : u"",
                            is_prefetch
                                ? metrics::OmniboxEventProto::OTHER_ZPS_PREFETCH
                                : metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  // An AutocompleteInput that gets Zero Prefix Suggestions on SRP.
  AutocompleteInput ZeroPrefixInputForSRP(
      const bool is_prefetch,
      const std::string& input_url = "https://www.google.com/search?q=foo") {
    AutocompleteInput input(
        // On IOS WEB/SRP, input text is not empty.
        is_ios ? base::ASCIIToUTF16(input_url) : u"",
        is_prefetch ? metrics::OmniboxEventProto::SRP_ZPS_PREFETCH
                    : metrics::OmniboxEventProto::
                          SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
        TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  // An AutocompleteInput that gets Prefix Suggestions on WEB.
  AutocompleteInput PrefixInputForWeb(
      const std::string& input_url = "https://example.com/") {
    AutocompleteInput input(u"foobar", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    return input;
  }

  // An AutocompleteInput that gets Prefix Suggestions on SRP.
  AutocompleteInput PrefixInputForSRP(
      const std::string& input_url = "https://www.google.com/search?q=foo") {
    AutocompleteInput input(u"foobar",
                            metrics::OmniboxEventProto::
                                SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                            TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    return input;
  }

  AutocompleteInput ZeroPrefixInputForLens(
      const std::string& input_url = "https://example.com/") {
    AutocompleteInput input(
        u"", metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX,
        TestSchemeClassifier());
    input.set_current_url(GURL(input_url));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    return input;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<ZeroSuggestProvider> provider_;
  bool provider_did_notify_;
};

void ZeroSuggestProviderTest::SetUp() {
  client_ = std::make_unique<FakeAutocompleteProviderClient>();

  // Activate URL data collection.
  client_->set_is_url_data_collection_active(true);

  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  template_url_service->Load();

  // Verify that Google is the default search provider.
  ASSERT_EQ(SEARCH_ENGINE_GOOGLE,
            template_url_service->GetDefaultSearchProvider()->GetEngineType(
                template_url_service->search_terms_data()));

  provider_ = ZeroSuggestProvider::Create(client_.get(), this);
  provider_did_notify_ = false;

  // Ensure the prefs-based cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 base::Value::Dict());

  // Ensure the in-memory cache is empty.
  ZeroSuggestCacheService* cache_svc = client_->GetZeroSuggestCacheService();
  cache_svc->ClearCache();

  scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_list_->InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetching,
                            omnibox::kZeroSuggestPrefetchingOnSRP,
                            omnibox::kZeroSuggestPrefetchingOnWeb},
      /*disabled_features=*/{});
}

void ZeroSuggestProviderTest::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  provider_did_notify_ = true;
}

// Tests whether zero-suggest is allowed on NTP.
TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestionsNTP) {
  AutocompleteInput zero_prefix_ntp_input =
      ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  AutocompleteInput prefix_ntp_input = PrefixInputForNTP();

  // zero-suggest suggestions are allowed on NTP.
  {
    EXPECT_EQ(
        std::make_pair(ZeroSuggestProvider::ResultType::kRemoteNoURL, true),
        ZeroSuggestProvider::GetResultTypeAndEligibility(
            client_.get(), zero_prefix_ntp_input));

    EXPECT_EQ(std::make_pair(ZeroSuggestProvider::ResultType::kNone, false),
              ZeroSuggestProvider::GetResultTypeAndEligibility(
                  client_.get(), prefix_ntp_input));
  }
}

TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestionsOnSearchActivity) {
  AutocompleteInput input(u"",
                          metrics::OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Offer ZPS when the user focus the omnibox.
  EXPECT_EQ(
      std::make_pair(ZeroSuggestProvider::ResultType::kRemoteNoURL, true),
      ZeroSuggestProvider::GetResultTypeAndEligibility(client_.get(), input));

  // Don't offer ZPS when the user is typing.
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  EXPECT_EQ(
      std::make_pair(ZeroSuggestProvider::ResultType::kNone, false),
      ZeroSuggestProvider::GetResultTypeAndEligibility(client_.get(), input));
}

// Tests whether zero-suggest is allowed on Web/SRP when the external request
// conditions are met.
TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestionsContextualWebAndSRP) {
  AutocompleteInput prefix_web_input = PrefixInputForWeb();
  AutocompleteInput prefix_srp_input = PrefixInputForSRP();
  AutocompleteInput zero_prefix_web_input =
      ZeroPrefixInputForWeb(/*is_prefetch=*/false);
  AutocompleteInput zero_prefix_srp_input =
      ZeroPrefixInputForSRP(/*is_prefetch=*/false);
  AutocompleteInput zero_prefix_lens_input = ZeroPrefixInputForLens();

  {
    EXPECT_EQ(std::make_pair(ZeroSuggestProvider::ResultType::kNone, false),
              ZeroSuggestProvider::GetResultTypeAndEligibility(
                  client_.get(), prefix_web_input));

    EXPECT_EQ(std::make_pair(ZeroSuggestProvider::ResultType::kNone, false),
              ZeroSuggestProvider::GetResultTypeAndEligibility(
                  client_.get(), prefix_srp_input));

    EXPECT_EQ(
        std::make_pair(ZeroSuggestProvider::ResultType::kRemoteNoURL, true),
        ZeroSuggestProvider::GetResultTypeAndEligibility(
            client_.get(), zero_prefix_lens_input));

    EXPECT_EQ(
        std::make_pair(ZeroSuggestProvider::ResultType::kRemoteSendURL, true),
        ZeroSuggestProvider::GetResultTypeAndEligibility(
            client_.get(), zero_prefix_srp_input));

    EXPECT_EQ(
        std::make_pair(ZeroSuggestProvider::ResultType::kRemoteSendURL, true),
        ZeroSuggestProvider::GetResultTypeAndEligibility(
            client_.get(), zero_prefix_web_input));
  }
}

// Tests whether zero-suggest is allowed on NTP/Web/SRP with various external
TEST_F(ZeroSuggestProviderTest, AllowZeroPrefixSuggestionsRequestEligibility) {
  // Keep a reference to the Google default search provider.
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  const TemplateURL* google_provider =
      template_url_service->GetDefaultSearchProvider();

  // Benchmark test for NTP.
  auto test_ntp = [this]() {
    const auto& input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
    const auto [result_type, eligible] =
        ZeroSuggestProvider::GetResultTypeAndEligibility(client_.get(), input);
    EXPECT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL, result_type);
    return eligible;
  };

  // Benchmark test for Lens searchboxes.
  auto test_lens = [this]() {
    const auto& input = ZeroPrefixInputForLens();
    const auto [result_type, eligible] =
        ZeroSuggestProvider::GetResultTypeAndEligibility(client_.get(), input);
    EXPECT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL, result_type);
    return eligible;
  };

  // Benchmark test for valid page URL.
  auto test_other = [this]() {
    const auto& input = ZeroPrefixInputForWeb(/*is_prefetch=*/false);
    const auto [result_type, eligible] =
        ZeroSuggestProvider::GetResultTypeAndEligibility(client_.get(), input);
    EXPECT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL, result_type);
    return eligible;
  };

  // Benchmark test for Search Results Page URL.
  auto test_srp = [this](const TemplateURL* template_url) {
    const auto& input = ZeroPrefixInputForSRP(
        /*is_prefetch=*/false,
        /*input_url= */
        template_url->GenerateSearchURL(SearchTermsData()).spec());
    const auto [result_type, eligible] =
        ZeroSuggestProvider::GetResultTypeAndEligibility(client_.get(), input);
    EXPECT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL, result_type);
    return eligible;
  };

  // Enable URL data collection.
  client_->set_is_url_data_collection_active(true);

  {
    // Zero-suggest is generally not allowed for invalid or non-HTTP(S) URLs.
    AutocompleteInput on_focus_ineligible_url_input = ZeroPrefixInputForWeb(
        /*is_prefetch=*/false,
        /*input_url= */ "chrome://history");
    EXPECT_EQ(std::make_pair(ZeroSuggestProvider::ResultType::kNone, false),
              ZeroSuggestProvider::GetResultTypeAndEligibility(
                  client_.get(), on_focus_ineligible_url_input));
  }
  {
    // Zero-suggest is generally not allowed for non-empty inputs.
    EXPECT_EQ(std::make_pair(ZeroSuggestProvider::ResultType::kNone, false),
              ZeroSuggestProvider::GetResultTypeAndEligibility(
                  client_.get(), PrefixInputForNTP()));
  }
  {
    // Zero-suggest is generally not allowed for non-empty inputs.
    EXPECT_EQ(std::make_pair(ZeroSuggestProvider::ResultType::kNone, false),
              ZeroSuggestProvider::GetResultTypeAndEligibility(
                  client_.get(), PrefixInputForSRP()));
  }
  {
    // Zero-suggest request can be made on NTP.
    EXPECT_TRUE(test_ntp());
  }
  {
    // Zero-suggest request can be made from Lens searchboxes.
    EXPECT_TRUE(test_lens());
  }

  {
    // Valid SRP URLs can be sent in the zero-suggest request.
    EXPECT_TRUE(test_srp(google_provider));
  }
  {
    // Valid page URLs can be sent in the zero-suggest request.
    EXPECT_TRUE(test_other());
  }

  // Deactivate URL data collection. This ensures that the page URL
  // cannot be sent and zero-suggest is disallowed unless the URL is the SRP or
  // the request is being made from the Lens searchboxes.
  client_->set_is_url_data_collection_active(false);

  {
    // Zero-suggest request can be made on NTP.
    EXPECT_TRUE(test_ntp());
  }
  {
    // Zero-suggest request can be made from Lens searchboxes.
    EXPECT_TRUE(test_lens());
  }
  {
    // Valid SRP URLs can be sent in the zero-suggest request.
    EXPECT_TRUE(test_srp(google_provider));
  }
  {
    // Valid page URLs cannot be sent in the zero-suggest request.
    EXPECT_FALSE(test_other());
  }

  // Reactivate URL data collection.
  client_->set_is_url_data_collection_active(true);

  // Change the default search provider to a non-Google one.
  TemplateURLData non_google_provider_data;
  non_google_provider_data.SetURL("https://www.nongoogle.com/?q={searchTerms}");
  non_google_provider_data.suggestions_url =
      "https://www.nongoogle.com/suggest/?q={searchTerms}";
  auto* non_google_provider = template_url_service->Add(
      std::make_unique<TemplateURL>(non_google_provider_data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      non_google_provider);

  {
    // Zero-suggest request cannot be made on NTP.
    EXPECT_FALSE(test_ntp());
  }
  {
    // Zero-suggest request cannot be made from Lens searchboxes.
    EXPECT_FALSE(test_lens());
  }
  {
    // Valid SRP URLs cannot be sent in the zero-suggest request.
    EXPECT_FALSE(test_srp(non_google_provider));
  }
  {
    // Valid page URLs cannot be sent in the zero-suggest request.
    EXPECT_FALSE(test_other());
  }

  // Change the default search provider back to Google.
  template_url_service->SetUserSelectedDefaultSearchProvider(
      const_cast<TemplateURL*>(google_provider));
}

TEST_F(ZeroSuggestProviderTest, EligibilityHistogram) {
  {
    base::HistogramTester histogram_tester;

    provider_->Start(ZeroPrefixInputForNTP(/*is_prefetch=*/false), false);

    // Make sure the default provider's suggest endpoint was queried without the
    // current page URL.
    EXPECT_FALSE(provider_->done());
    GURL suggest_url = GetSuggestURL(
        metrics::OmniboxEventProto::NTP_REALBOX,
        metrics::OmniboxFocusType::INTERACTION_FOCUS, std::string());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

    test_loader_factory()->AddResponse(
        test_loader_factory()->GetPendingRequest(0)->request.url.spec(),
        R"(["",[],[],[],{}])");
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount("Omnibox.ZeroSuggestProvider.Eligibility",
                                      1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.Eligibility", 0 /*kEligible*/, 1);
  }
  {
    base::HistogramTester histogram_tester;

    provider_->Start(PrefixInputForNTP(), false);

    // Make sure the default provider's suggest endpoint was not queried.
    EXPECT_TRUE(provider_->done());
    EXPECT_EQ(0, test_loader_factory()->NumPending());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount("Omnibox.ZeroSuggestProvider.Eligibility",
                                      1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.Eligibility", 3 /*kGenerallyIneligible*/,
        1);
  }
}

TEST_F(ZeroSuggestProviderTest, SendRequestWithoutLensInteractionResponse) {
  AutocompleteInput input = ZeroPrefixInputForLens();
  provider_->Start(input, false);

  // Make sure the default provider's suggest endpoint was queried with the
  // expected client and without Lens Suggest signals.
  EXPECT_FALSE(provider_->done());
  EXPECT_EQ(1, test_loader_factory()->NumPending());
  EXPECT_FALSE(base::EndsWith(
      test_loader_factory()->GetPendingRequest(0)->request.url.spec(),
      "iil=", base::CompareCase::SENSITIVE));

  test_loader_factory()->AddResponse(
      test_loader_factory()->GetPendingRequest(0)->request.url.spec(),
      R"(["",[],[],[],{}])");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());
}

TEST_F(ZeroSuggestProviderTest, SendRequestWithLensInteractionResponse) {
  AutocompleteInput input = ZeroPrefixInputForLens();
  lens::proto::LensOverlaySuggestInputs lens_overlay_suggest_inputs;
  lens_overlay_suggest_inputs.set_encoded_image_signals("xyz");
  input.set_lens_overlay_suggest_inputs(lens_overlay_suggest_inputs);
  provider_->Start(input, false);

  // Make sure the default provider's suggest endpoint was queried with the
  // expected client and Lens Suggest signals.
  EXPECT_FALSE(provider_->done());
  EXPECT_EQ(1, test_loader_factory()->NumPending());
  EXPECT_TRUE(base::EndsWith(
      test_loader_factory()->GetPendingRequest(0)->request.url.spec(),
      "iil=xyz", base::CompareCase::SENSITIVE));

  test_loader_factory()->AddResponse(
      test_loader_factory()->GetPendingRequest(0)->request.url.spec(),
      R"(["",[],[],[],{}])");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());
}

TEST_F(ZeroSuggestProviderTest, StartStopNTP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");

  // Make sure valid input starts the provider.
  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure invalid input stops the provider.
  AutocompleteInput prefix_input = PrefixInputForNTP();
  provider_->Start(prefix_input, false);
  EXPECT_TRUE(provider_->done());
  // Expect that matches did not get populated out of cache.
  EXPECT_TRUE(provider_->matches().empty());
  // Expect that network request was not sent.
  EXPECT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener since the
  // request was invalidated.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, StartStopSRP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = ZeroPrefixInputForSRP(/*is_prefetch=*/false);
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_FOCUS, input.current_url().spec());

  // Make sure valid input starts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure invalid input stops the provider.
  AutocompleteInput prefix_input = PrefixInputForWeb();
  provider_->Start(prefix_input, false);
  EXPECT_TRUE(provider_->done());
  // Expect that matches did not get populated out of cache.
  EXPECT_TRUE(provider_->matches().empty());
  // Expect that network request was not sent.
  EXPECT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener since the
  // request was invalidated.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, StartStopWeb) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = ZeroPrefixInputForWeb(
      /*is_prefetch=*/false);
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                                   metrics::OmniboxFocusType::INTERACTION_FOCUS,
                                   input.current_url().spec());

  // Make sure valid input starts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure invalid input stops the provider.
  AutocompleteInput prefix_input = PrefixInputForWeb();
  provider_->Start(prefix_input, false);
  EXPECT_TRUE(provider_->done());
  // Expect that matches did not get populated out of cache.
  EXPECT_TRUE(provider_->matches().empty());
  // Expect that network request was not sent.
  EXPECT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener since the
  // request was invalidated.
  EXPECT_FALSE(provider_did_notify_);

  // Make sure valid input restarts the provider.
  provider_->Start(input, false);
  EXPECT_FALSE(provider_->done());
  // Expect that matches got populated out of cache.
  EXPECT_FALSE(provider_->matches().empty());
  // Expect that network request was sent.
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  // Expect the provider to not have notified the provider listener yet.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRunNTP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", /*kRequestSent*/ 1, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseConvertedToMatches*/ 5, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  EXPECT_EQ(3U, provider_->matches().size());
  PrefService* prefs = client_->GetPrefs();
  EXPECT_EQ(json_response,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRunSRP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable prefetching on SRP and disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetchingOnSRP},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching});

  AutocompleteInput input = ZeroPrefixInputForSRP(/*is_prefetch=*/false);
  provider_->Start(input, false);

  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_FOCUS, input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseConvertedToMatches*/ 5, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  EXPECT_EQ(3U, provider_->matches().size());
  PrefService* prefs = client_->GetPrefs();
  EXPECT_EQ(json_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRunWeb) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable prefetching on Web and disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetchingOnWeb},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching});

  AutocompleteInput input = ZeroPrefixInputForWeb(/*is_prefetch=*/false);
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  EXPECT_TRUE(provider_->matches().empty());

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                                   metrics::OmniboxFocusType::INTERACTION_FOCUS,
                                   input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseConvertedToMatches*/ 5, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  EXPECT_EQ(3U, provider_->matches().size());
  PrefService* prefs = client_->GetPrefs();
  EXPECT_EQ(json_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestOmitAsynchronousMatchesTrueNTP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  input.set_omit_asynchronous_matches(true);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 base::Value::Dict());

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // There should be no pending network requests, given that asynchronous logic
  // has been explicitly disabled (`omit_asynchronous_matches_ == true`).
  ASSERT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));

  // Expect the provider to not have notified the provider listener since the
  // request was not sent.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestOmitAsynchronousMatchesTrueSRP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  AutocompleteInput input = ZeroPrefixInputForSRP(/*is_prefetch=*/false);
  input.set_omit_asynchronous_matches(true);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_FOCUS, input.current_url().spec());

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 base::Value::Dict());

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // There should be no pending network requests, given that asynchronous logic
  // has been explicitly disabled (`omit_asynchronous_matches_ == true`).
  ASSERT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));

  // Expect the provider to not have notified the provider listener since the
  // request was not sent.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestOmitAsynchronousMatchesTrueWeb) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  AutocompleteInput input = ZeroPrefixInputForWeb(/*is_prefetch=*/false);
  input.set_omit_asynchronous_matches(true);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                                   metrics::OmniboxFocusType::INTERACTION_FOCUS,
                                   input.current_url().spec());

  // Ensure the cache is empty.
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, "");
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 base::Value::Dict());

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());
  EXPECT_TRUE(provider_->done());
  EXPECT_TRUE(provider_->matches().empty());

  // There should be no pending network requests, given that asynchronous logic
  // has been explicitly disabled (`omit_asynchronous_matches_ == true`).
  ASSERT_FALSE(test_loader_factory()->IsPending(suggest_url.spec()));

  // Expect the provider to not have notified the provider listener since the
  // request was not sent.
  EXPECT_FALSE(provider_did_notify_);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResultsNTP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", /*kRequestSent*/ 1, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_F(ZeroSuggestProviderTest, TestZeroSuggestHasInMemoryCachedResultsNTP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kZeroSuggestInMemoryCaching);

  // Set up the in-memory cache with the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  ZeroSuggestCacheService* cache_svc = client_->GetZeroSuggestCacheService();
  cache_svc->StoreZeroSuggestResponse("", json_response);

  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", /*kRequestSent*/ 1, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            cache_svc->ReadZeroSuggestResponse("").response_json);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResultsSRP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable prefetching on SRP and disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetchingOnSRP},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching});

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = ZeroPrefixInputForSRP(/*is_prefetch=*/false);
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_FOCUS, input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest, TestZeroSuggestHasInMemoryCachedResultsSRP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable in-memory ZPS caching and prefetching on SRP.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestInMemoryCaching,
                            omnibox::kZeroSuggestPrefetchingOnSRP},
      /*disabled_features=*/{});

  // Set up the in-memory cache with the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  AutocompleteInput input = ZeroPrefixInputForSRP(/*is_prefetch=*/false);
  ZeroSuggestCacheService* cache_svc = client_->GetZeroSuggestCacheService();
  cache_svc->StoreZeroSuggestResponse(input.current_url().spec(),
                                      json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_FOCUS, input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            cache_svc->ReadZeroSuggestResponse(input.current_url().spec())
                .response_json);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResultsWeb) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable prefetching on Web and disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetchingOnWeb},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching});

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = ZeroPrefixInputForWeb(/*is_prefetch=*/false);
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                                   metrics::OmniboxFocusType::INTERACTION_FOCUS,
                                   input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest, TestZeroSuggestHasInMemoryCachedResultsWeb) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable in-memory caching and prefetching on Web.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestInMemoryCaching,
                            omnibox::kZeroSuggestPrefetchingOnWeb},
      /*disabled_features=*/{});

  // Set up the in-memory cache with the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  AutocompleteInput input = ZeroPrefixInputForWeb(/*is_prefetch=*/false);
  ZeroSuggestCacheService* cache_svc = client_->GetZeroSuggestCacheService();
  cache_svc->StoreZeroSuggestResponse(input.current_url().spec(),
                                      json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                                   metrics::OmniboxFocusType::INTERACTION_FOCUS,
                                   input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect the provider to not have notified the provider listener when using
  // the cached response.
  EXPECT_FALSE(provider_did_notify_);

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 4);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);

  // Expect the same results after the response has been handled.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Expect the new results to have been stored.
  EXPECT_EQ(json_response2,
            cache_svc->ReadZeroSuggestResponse(input.current_url().spec())
                .response_json);
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestReceivedEmptyResultsNTP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable ZPS prefetching on NTP and disable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetching},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching});

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url =
      GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                    metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response(R"(["",[],[],[],{}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 5);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", /*kRequestSent*/ 1, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseConvertedToMatches*/ 5, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  // Expect the new results to have been stored.
  EXPECT_EQ(empty_response,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestReceivedEmptyResultsSRP) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable prefetching on SRP and disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetchingOnSRP},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching});

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = ZeroPrefixInputForSRP(/*is_prefetch=*/false);
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(
      metrics::OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
      metrics::OmniboxFocusType::INTERACTION_FOCUS, input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response(R"(["",[],[],[],{}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 5);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseConvertedToMatches*/ 5, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  // Expect the new results to have been stored.
  EXPECT_EQ(empty_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest,
       TestPsuggestZeroSuggestReceivedEmptyResultsWeb) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable prefetching on Web and disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetchingOnWeb},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching});

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  AutocompleteInput input = ZeroPrefixInputForWeb(/*is_prefetch=*/false);
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
      prefs, input.current_url().spec(), json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                                   metrics::OmniboxFocusType::INTERACTION_FOCUS,
                                   input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response(R"(["",[],[],[],{}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 5);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
      1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
      /*kRemoteResponseConvertedToMatches*/ 5, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  // Expect the new results to have been stored.
  EXPECT_EQ(empty_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, input.current_url().spec()));
}

TEST_F(ZeroSuggestProviderTest, TestZeroSuggestReceivedInvalidResults) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  std::vector<std::string> invalid_responses = {"", "}bro|ken{", "[]",
                                                R"(["",{}])"};

  // Verify that none of the invalid ZPS responses trigger storage of ZPS data.
  for (auto response : invalid_responses) {
    provider_->Start(input, false);
    ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
              provider_->GetResultTypeRunningForTesting());

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                      metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

    test_loader_factory()->AddResponse(suggest_url.spec(), response);

    // Spin event loop to allow network request to go through.
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(provider_->done());
    EXPECT_TRUE(provider_->matches().empty());

    // Provider shouldn't have notified any provider listeners.
    EXPECT_FALSE(provider_did_notify_);

    test_loader_factory()->ClearResponses();
  }

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      2 * invalid_responses.size());
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", /*kRequestSent*/ 1,
      invalid_responses.size());
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kResponseReceived*/ 3, invalid_responses.size());
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 0);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestPrefetchThenNTPOnFocus) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  {
    base::HistogramTester histogram_tester;

    // Start a prefetch request.
    AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch = */ true);
    provider_->StartPrefetch(input);
    EXPECT_TRUE(provider_->done());

    // Expect the results to be empty.
    ASSERT_EQ(0U, provider_->matches().size());

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::NTP_ZPS_PREFETCH,
                      metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response2(
        R"(["",["search4", "search5", "search6"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 3);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", /*kRequestSent*/ 1, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch",
        /*kResponseReceived*/ 3, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch",
        /*kRemoteResponseCached*/ 4, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same empty results after the response has been handled.
    ASSERT_EQ(0U, provider_->matches().size());

    // Expect the new response to have been stored in the pref.
    EXPECT_EQ(json_response2,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
  }
  {
    base::HistogramTester histogram_tester;

    // Start a non-prefetch request.
    AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
    provider_->Start(input, false);
    EXPECT_FALSE(provider_->done());
    ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
              provider_->GetResultTypeRunningForTesting());

    // Expect the results from the cached response.
    ASSERT_EQ(3U, provider_->matches().size());
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                      metrics::OmniboxFocusType::INTERACTION_FOCUS, "");
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response3(
        R"(["",["search7", "search8", "search9"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response3);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 4);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
        /*kCachedResponseConvertedToMatches*/ 0, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", /*kRequestSent*/ 1, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
        /*kResponseReceived*/ 3, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
        /*kRemoteResponseCached*/ 4, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same results after the response has been handled.
    ASSERT_EQ(3U, provider_->matches().size());
    EXPECT_EQ(u"search4", provider_->matches()[0].contents);
    EXPECT_EQ(u"search5", provider_->matches()[1].contents);
    EXPECT_EQ(u"search6", provider_->matches()[2].contents);

    // Expect the new response to have been stored in the pref.
    EXPECT_EQ(json_response3,
              prefs->GetString(omnibox::kZeroSuggestCachedResults));
  }
}

TEST_F(ZeroSuggestProviderTest, TestMultipleZeroSuggestPrefetchesInFlight) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  PrefService* prefs = client_->GetPrefs();
  base::HistogramTester histogram_tester;

  // Start a prefetch request on NTP.
  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch =*/true);
  provider_->StartPrefetch(input);
  EXPECT_TRUE(provider_->done());

  // Expect the results to be empty.
  ASSERT_EQ(0U, provider_->matches().size());

  // Verify that there's an in-flight prefetch request for NTP context.
  GURL suggest_url = GetSuggestURL(input.current_page_classification(),
                                   input.focus_type(), "");
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");

  test_loader_factory()->AddResponse(suggest_url.spec(), json_response);

  // Start a prefetch request on SRP.
  input = ZeroPrefixInputForSRP(/*is_prefetch=*/true);
  provider_->StartPrefetch(input);
  EXPECT_TRUE(provider_->done());

  // Expect the results to be empty.
  ASSERT_EQ(0U, provider_->matches().size());

  // Verify that there's an in-flight prefetch request for SRP context.
  suggest_url = GetSuggestURL(input.current_page_classification(),
                              input.focus_type(), input.current_url().spec());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));

  std::string json_response2(
      R"(["",["search4", "search5", "search6"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");

  test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

  // Resolve all in-flight ZPS prefetch requests.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 3);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 3);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);

  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", /*kRequestSent*/ 1, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
      /*kRequestInvalidated*/ 2, 0);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
      /*kRemoteResponseCached*/ 4, 1);

  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", /*kRequestSent*/ 1, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", /*kRequestInvalidated*/ 2,
      0);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch",
      /*kRemoteResponseCached*/ 4, 1);

  // Expect the provider to not have notified the provider listener since the
  // matches were not updated.
  EXPECT_FALSE(provider_did_notify_);

  // Expect the same empty results after the response has been handled.
  ASSERT_EQ(0U, provider_->matches().size());

  // Expect the responses to have been stored in the appropriate prefs.
  EXPECT_EQ(json_response,
            prefs->GetString(omnibox::kZeroSuggestCachedResults));

  const std::string current_url = input.current_url().spec();
  const std::string* stored_response =
      prefs->GetDict(omnibox::kZeroSuggestCachedResultsWithURL)
          .FindString(current_url);
  ASSERT_TRUE(stored_response && *stored_response == json_response2);
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestPrefetchThenSRP) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory caching and prefetching on SRP.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching,
                             omnibox::kZeroSuggestPrefetchingOnSRP});

  PrefService* prefs = client_->GetPrefs();

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");

  {
    base::HistogramTester histogram_tester;

    // Start a prefetch request.
    AutocompleteInput input = ZeroPrefixInputForSRP(/*is_prefetch=*/true);
    // Set up the pref to cache the response from the previous run.
    omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
        prefs, input.current_url().spec(), json_response);
    provider_->StartPrefetch(input);
    EXPECT_TRUE(provider_->done());

    // Expect the results to be empty.
    ASSERT_EQ(0U, provider_->matches().size());

    GURL suggest_url =
        GetSuggestURL(input.current_page_classification(), input.focus_type(),
                      input.current_url().spec());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response2(
        R"(["",["search4", "search5", "search6"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 2);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", /*kRequestSent*/ 1, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
        /*kResponseReceived*/ 3, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same empty results after the response has been handled.
    ASSERT_EQ(0U, provider_->matches().size());

    // Expect the response to not have been stored in the prefs.
    EXPECT_EQ(json_response,
              omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, input.current_url().spec()));
  }
  {
    base::HistogramTester histogram_tester;

    // Start a non-prefetch request.
    AutocompleteInput input = ZeroPrefixInputForSRP(/*is_prefetch=*/false);
    provider_->Start(input, false);
    EXPECT_FALSE(provider_->done());
    ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
              provider_->GetResultTypeRunningForTesting());

    // Expect the results to be empty.
    ASSERT_EQ(0U, provider_->matches().size());

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::
                          SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                      metrics::OmniboxFocusType::INTERACTION_FOCUS,
                      input.current_url().spec());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response3(
        R"(["",["search7", "search8", "search9"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response3);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 3);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
        1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        /*kResponseReceived*/ 3, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        /*kRemoteResponseConvertedToMatches*/ 5, 1);

    // Expect the provider to have notified the provider listener.
    EXPECT_TRUE(provider_did_notify_);

    // Expect the results after the response has been handled.
    ASSERT_EQ(3U, provider_->matches().size());
    EXPECT_EQ(u"search7", provider_->matches()[0].contents);
    EXPECT_EQ(u"search8", provider_->matches()[1].contents);
    EXPECT_EQ(u"search9", provider_->matches()[2].contents);

    // Expect the response to not have been stored in the prefs.
    EXPECT_EQ(json_response,
              omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, input.current_url().spec()));
  }
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestPrefetchThenWeb) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory caching and prefetching on Web.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching,
                             omnibox::kZeroSuggestPrefetchingOnWeb});

  PrefService* prefs = client_->GetPrefs();

  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");

  {
    base::HistogramTester histogram_tester;

    // Start a prefetch request.
    AutocompleteInput input = ZeroPrefixInputForWeb(/*is_prefetch=*/true);
    // Set up the pref to cache the response from the previous run.
    omnibox::SetUserPreferenceForZeroSuggestCachedResponse(
        prefs, input.current_url().spec(), json_response);
    provider_->StartPrefetch(input);
    EXPECT_TRUE(provider_->done());

    // Expect the results to be empty.
    ASSERT_EQ(0U, provider_->matches().size());

    GURL suggest_url =
        GetSuggestURL(input.current_page_classification(), input.focus_type(),
                      input.current_url().spec());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response2(
        R"(["",["search4", "search5", "search6"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response2);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 2);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", /*kRequestSent*/ 1, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch",
        /*kResponseReceived*/ 3, 1);

    // Expect the provider to not have notified the provider listener since the
    // matches were not updated.
    EXPECT_FALSE(provider_did_notify_);

    // Expect the same empty results after the response has been handled.
    ASSERT_EQ(0U, provider_->matches().size());

    // Expect the response to not have been stored in the prefs.
    EXPECT_EQ(json_response,
              omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, input.current_url().spec()));
  }
  {
    base::HistogramTester histogram_tester;

    // Start a non-prefetch request.
    AutocompleteInput input = ZeroPrefixInputForWeb(/*is_prefetch=*/false);
    provider_->Start(input, false);
    EXPECT_FALSE(provider_->done());
    ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteSendURL,
              provider_->GetResultTypeRunningForTesting());

    // Expect the results to be empty.
    ASSERT_EQ(0U, provider_->matches().size());

    GURL suggest_url =
        GetSuggestURL(metrics::OmniboxEventProto::OTHER,
                      metrics::OmniboxFocusType::INTERACTION_FOCUS,
                      input.current_url().spec());
    EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
    std::string json_response3(
        R"(["",["search7", "search8", "search9"],)"
        R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
        R"("google:verbatimrelevance":1300}])");
    test_loader_factory()->AddResponse(suggest_url.spec(), json_response3);

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(provider_->done());

    // Expect correct histograms to have been logged.
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
    histogram_tester.ExpectTotalCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 3);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", /*kRequestSent*/ 1,
        1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        /*kResponseReceived*/ 3, 1);
    histogram_tester.ExpectBucketCount(
        "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch",
        /*kRemoteResponseConvertedToMatches*/ 5, 1);

    // Expect the provider to have notified the provider listener.
    EXPECT_TRUE(provider_did_notify_);

    // Expect the results after the response has been handled.
    ASSERT_EQ(3U, provider_->matches().size());
    EXPECT_EQ(u"search7", provider_->matches()[0].contents);
    EXPECT_EQ(u"search8", provider_->matches()[1].contents);
    EXPECT_EQ(u"search9", provider_->matches()[2].contents);

    // Expect the response to not have been stored in the prefs.
    EXPECT_EQ(json_response,
              omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                  prefs, input.current_url().spec()));
  }
}

TEST_F(ZeroSuggestProviderTest, TestNoURLResultTypeWithNonEmptyURLInput) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Disable in-memory caching.
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(omnibox::kZeroSuggestInMemoryCaching);

  // Configure the "No URL" input with a non-empty URL.
  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  input.set_current_url(GURL("https://www.google.com"));

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300}])");
  PrefService* prefs = client_->GetPrefs();

  // Store cached ZPS response for NTP (which is associated with an empty page
  // URL).
  omnibox::SetUserPreferenceForZeroSuggestCachedResponse(prefs, std::string(),
                                                         json_response);

  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Despite the input having a non-empty URL, since the provider enforces an
  // empty string as the key for the cached response on NTP, the matches get
  // populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  GURL suggest_url = GetSuggestURL(metrics::OmniboxEventProto::NTP_REALBOX,
                                   metrics::OmniboxFocusType::INTERACTION_FOCUS,
                                   std::string());
  EXPECT_TRUE(test_loader_factory()->IsPending(suggest_url.spec()));
  std::string empty_response(R"(["",[],[],[],{}])");
  test_loader_factory()->AddResponse(suggest_url.spec(), empty_response);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(provider_->done());

  // Expect correct histograms to have been logged.
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.URLBased.NonPrefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.Prefetch", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", 5);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kCachedResponseConvertedToMatches*/ 0, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch", /*kRequestSent*/ 1, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kResponseReceived*/ 3, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseCached*/ 4, 1);
  histogram_tester.ExpectBucketCount(
      "Omnibox.ZeroSuggestProvider.NoURL.NonPrefetch",
      /*kRemoteResponseConvertedToMatches*/ 5, 1);

  // Expect the provider to have notified the provider listener.
  EXPECT_TRUE(provider_did_notify_);

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  // Expect the new results to have been stored, keyed off an empty page URL.
  EXPECT_EQ(empty_response,
            omnibox::GetUserPreferenceForZeroSuggestCachedResponse(
                prefs, std::string()));
}

TEST_F(ZeroSuggestProviderTest, TestDeleteMatchClearsPrefsBasedCache) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable ZPS prefetching on NTP and disable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{omnibox::kZeroSuggestPrefetching},
      /*disabled_features=*/{omnibox::kZeroSuggestInMemoryCaching});

  // Set up the pref to cache the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300,)"
      R"("google:suggestdetail":)"
      R"([{"du": "https://www.google.com/s1"},)"
      R"({"du": "https://www.google.com/s2"},)"
      R"({"du": "https://www.google.com/s3"}]}])");

  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  base::Value::Dict new_dict;
  new_dict.Set("https://www.google.com", json_response);
  prefs->SetDict(omnibox::kZeroSuggestCachedResultsWithURL,
                 std::move(new_dict));

  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Ensure that both cache prefs start off with non-empty values.
  ASSERT_FALSE(prefs->GetString(omnibox::kZeroSuggestCachedResults).empty());
  ASSERT_FALSE(
      prefs->GetDict(omnibox::kZeroSuggestCachedResultsWithURL).empty());

  provider_->DeleteMatch(provider_->matches()[0]);

  // Verify that cache prefs for "ZPS on NTP" and "ZPS on SRP/Web" have both
  // been cleared.
  ASSERT_TRUE(prefs->GetString(omnibox::kZeroSuggestCachedResults).empty());
  ASSERT_TRUE(
      prefs->GetDict(omnibox::kZeroSuggestCachedResultsWithURL).empty());
}

TEST_F(ZeroSuggestProviderTest, TestDeleteMatchClearsInMemoryCache) {
  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Enable in-memory ZPS caching.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(omnibox::kZeroSuggestInMemoryCaching);

  // Set up the in-memory cache with the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300,)"
      R"("google:suggestdetail":)"
      R"([{"du": "https://www.google.com/s1"},)"
      R"({"du": "https://www.google.com/s2"},)"
      R"({"du": "https://www.google.com/s3"}]}])");

  ZeroSuggestCacheService* cache_svc = client_->GetZeroSuggestCacheService();
  cache_svc->StoreZeroSuggestResponse("", json_response);
  cache_svc->StoreZeroSuggestResponse("https://www.google.com", json_response);

  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Ensure that both cache entries have non-empty values.
  ASSERT_FALSE(cache_svc->ReadZeroSuggestResponse("").response_json.empty());
  ASSERT_FALSE(cache_svc->ReadZeroSuggestResponse("https://www.google.com")
                   .response_json.empty());

  provider_->DeleteMatch(provider_->matches()[0]);

  // Verify that the entire cache has been cleared.
  ASSERT_TRUE(cache_svc->IsInMemoryCacheEmptyForTesting());
}

TEST_F(ZeroSuggestProviderTest, TestDeleteMatchTriggersDeletionRequest) {
  base::UserActionTester user_action_tester;

  EXPECT_CALL(*client_, IsAuthenticated())
      .WillRepeatedly(testing::Return(true));

  // Set up the cache with the response from the previous run.
  std::string json_response(
      R"(["",["search1", "search2", "search3"],)"
      R"([],[],{"google:suggestrelevance":[602, 601, 600],)"
      R"("google:verbatimrelevance":1300,)"
      R"("google:suggestdetail":)"
      R"([{"du": "https://www.google.com/s1"},)"
      R"({"du": "https://www.google.com/s2"},)"
      R"({"du": "https://www.google.com/s3"}]}])");

  PrefService* prefs = client_->GetPrefs();
  prefs->SetString(omnibox::kZeroSuggestCachedResults, json_response);

  // Trigger a non-prefetch ZPS provider run.
  AutocompleteInput input = ZeroPrefixInputForNTP(/*is_prefetch=*/false);
  provider_->Start(input, false);
  ASSERT_EQ(ZeroSuggestProvider::ResultType::kRemoteNoURL,
            provider_->GetResultTypeRunningForTesting());

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(3U, provider_->matches().size());  // 3 results, no verbatim match
  EXPECT_EQ(u"search1", provider_->matches()[0].contents);
  EXPECT_EQ(u"search2", provider_->matches()[1].contents);
  EXPECT_EQ(u"search3", provider_->matches()[2].contents);

  // Test a successful deletion request.
  provider_->DeleteMatch(provider_->matches()[0]);

  const std::string del_url1 = "https://www.google.com/s1";
  ASSERT_TRUE(test_loader_factory()->IsPending(del_url1));
  test_loader_factory()->AddResponse(del_url1, "");

  // Spin event loop to allow deletion request to go through.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Omnibox.ZeroSuggestDelete.Success"));

  // Test a failed deletion request.
  test_loader_factory()->ClearResponses();
  provider_->DeleteMatch(provider_->matches()[0]);

  const std::string del_url2 = "https://www.google.com/s2";
  ASSERT_TRUE(test_loader_factory()->IsPending(del_url2));

  auto head = network::mojom::URLResponseHead::New();
  std::string headers(
      "HTTP/1.1 500 Server Failure\nContent-type: application/json\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->mime_type = "application/json";
  test_loader_factory()->AddResponse(GURL(del_url2), std::move(head), "",
                                     network::URLLoaderCompletionStatus());

  // Spin event loop to allow deletion request to go through.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Omnibox.ZeroSuggestDelete.Failure"));
}
