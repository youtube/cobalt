// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

// Some of these tests overlap with FirstPartySetParser unittests, but
// overlapping test coverage isn't the worst thing.
namespace content {

namespace {

using ParseErrorType = FirstPartySetsHandler::ParseErrorType;
using ParseWarningType = FirstPartySetsHandler::ParseWarningType;

const char* kAdditionsField = "additions";
const char* kPrimaryField = "primary";
const char* kCctldsField = "ccTLDs";

const char* kFirstPartySetsClearSiteDataOutcomeHistogram =
    "FirstPartySets.Initialization.ClearSiteDataOutcome";

const char* kDelayedQueriesCountHistogram =
    "Cookie.FirstPartySets.Browser.DelayedQueriesCount";
const char* kMostDelayedQueryDeltaHistogram =
    "Cookie.FirstPartySets.Browser.MostDelayedQueryDelta";

}  // namespace

TEST(FirstPartySetsHandlerImpl, ValidateEnterprisePolicy_ValidPolicy) {
  base::Value input = base::JSONReader::Read(R"(
             {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }
                ]
              }
            )")
                          .value();
  // Validation doesn't fail with an error and there are no warnings to output.
  auto [success, warnings] =
      FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict());
  EXPECT_TRUE(success.has_value());
  EXPECT_THAT(warnings, IsEmpty());
}

TEST(FirstPartySetsHandlerImpl,
     ValidateEnterprisePolicy_ValidPolicyWithWarnings) {
  // Some input that matches our policies schema but returns non-fatal warnings.
  base::Value input = base::JSONReader::Read(R"(
              {
                "replacements": [],
                "additions": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"],
                    "ccTLDs": {
                      "https://non-canonical.test": ["https://primary1.test"]
                    }
                  }
                ]
              }
            )")
                          .value();
  // Validation succeeds without errors.
  auto [success, warnings] =
      FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict());
  EXPECT_TRUE(success.has_value());
  // Outputs metadata that can be used to surface a descriptive warning.
  EXPECT_THAT(
      warnings,
      UnorderedElementsAre(FirstPartySetsHandler::ParseWarning(
          ParseWarningType::kCctldKeyNotCanonical,
          {kAdditionsField, 0, kCctldsField, "https://non-canonical.test"})));
}

TEST(FirstPartySetsHandlerImpl, ValidateEnterprisePolicy_InvalidPolicy) {
  // Some input that matches our policies schema but breaks FPS invariants.
  // For more test coverage, see the ParseSetsFromEnterprisePolicy unit tests.
  base::Value input = base::JSONReader::Read(R"(
              {
                "replacements": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite1.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary1.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }
                ]
              }
            )")
                          .value();
  // Validation fails with an error.
  auto [success, warnings] =
      FirstPartySetsHandler::ValidateEnterprisePolicy(input.GetDict());
  EXPECT_FALSE(success.has_value());
  // An appropriate ParseError is returned.
  EXPECT_EQ(success.error(), FirstPartySetsHandler::ParseError(
                                 ParseErrorType::kNonDisjointSets,
                                 {kAdditionsField, 0, kPrimaryField}));
}

class FirstPartySetsHandlerImplTest : public ::testing::Test {
 public:
  explicit FirstPartySetsHandlerImplTest(bool enabled)
      : handler_(FirstPartySetsHandlerImpl::CreateForTesting(
            /*enabled=*/enabled,
            /*embedder_will_provide_public_sets=*/enabled)) {
    CHECK(scoped_dir_.CreateUniqueTempDir());
    CHECK(PathExists(scoped_dir_.GetPath()));
  }

  base::File WritePublicSetsFile(base::StringPiece content) {
    base::FilePath path =
        scoped_dir_.GetPath().Append(FILE_PATH_LITERAL("sets_file.json"));
    CHECK(base::WriteFile(path, content));

    return base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }

  net::GlobalFirstPartySets GetSetsAndWait(FirstPartySetsHandlerImpl& handler) {
    base::test::TestFuture<net::GlobalFirstPartySets> future;
    absl::optional<net::GlobalFirstPartySets> result =
        handler.GetSets(future.GetCallback());
    return result.has_value() ? std::move(result).value() : future.Take();
  }

  void ClearSiteDataOnChangedSetsForContextAndWait(
      FirstPartySetsHandlerImpl& handler,
      BrowserContext* context,
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config) {
    base::RunLoop run_loop;
    handler.ClearSiteDataOnChangedSetsForContext(
        base::BindLambdaForTesting([&]() { return context; }),
        browser_context_id, std::move(context_config),
        base::BindLambdaForTesting(
            [&](net::FirstPartySetsContextConfig,
                net::FirstPartySetsCacheFilter) { run_loop.Quit(); }));
    run_loop.Run();
  }

  absl::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
  GetPersistedSetsAndWait(FirstPartySetsHandlerImpl& handler,
                          const std::string& browser_context_id) {
    base::test::TestFuture<absl::optional<
        std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>>
        future;
    handler.GetPersistedSetsForTesting(browser_context_id,
                                       future.GetCallback());
    return future.Take();
  }

  absl::optional<bool> HasEntryInBrowserContextsClearedAndWait(
      FirstPartySetsHandlerImpl& handler,
      const std::string& browser_context_id) {
    base::test::TestFuture<absl::optional<bool>> future;
    handler.HasBrowserContextClearedForTesting(browser_context_id,
                                               future.GetCallback());
    return future.Take();
  }

  net::GlobalFirstPartySets GetSetsAndWait() {
    return GetSetsAndWait(handler());
  }

  void ClearSiteDataOnChangedSetsForContextAndWait(
      BrowserContext* context,
      const std::string& browser_context_id,
      net::FirstPartySetsContextConfig context_config) {
    ClearSiteDataOnChangedSetsForContextAndWait(
        handler(), context, browser_context_id, std::move(context_config));
  }

  absl::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
  GetPersistedSetsAndWait(const std::string& browser_context_id) {
    return GetPersistedSetsAndWait(handler(), browser_context_id);
  }

  absl::optional<bool> HasEntryInBrowserContextsClearedAndWait(
      const std::string& browser_context_id) {
    return HasEntryInBrowserContextsClearedAndWait(handler(),
                                                   browser_context_id);
  }

  FirstPartySetsHandlerImpl& handler() { return handler_; }

  BrowserContext* context() { return &context_; }

 protected:
  base::ScopedTempDir scoped_dir_;

 private:
  BrowserTaskEnvironment env_;
  TestBrowserContext context_;
  FirstPartySetsHandlerImpl handler_;
};

class FirstPartySetsHandlerImplDisabledTest
    : public FirstPartySetsHandlerImplTest {
 public:
  FirstPartySetsHandlerImplDisabledTest()
      : FirstPartySetsHandlerImplTest(/*enabled=*/false) {}
};

TEST_F(FirstPartySetsHandlerImplDisabledTest, InitMetrics) {
  base::HistogramTester histogram_tester;
  handler().Init(
      /*user_data_dir=*/{}, LocalSetDeclaration());

  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram_tester.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

class FirstPartySetsHandlerImplEnabledTest
    : public FirstPartySetsHandlerImplTest {
 public:
  FirstPartySetsHandlerImplEnabledTest()
      : FirstPartySetsHandlerImplTest(/*enabled=*/true) {}
};

TEST_F(FirstPartySetsHandlerImplEnabledTest, EmptyDBPath) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite1.test"));

  handler().SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(""));

  // Empty `user_data_dir` will fail to load persisted sets, but that will not
  // prevent `on_sets_ready` from being invoked.
  handler().Init(
      /*user_data_dir=*/{},
      LocalSetDeclaration(
          R"({"primary": "https://example.test",)"
          R"("associatedSites": ["https://associatedsite1.test"]})"));

  EXPECT_THAT(
      GetSetsAndWait().FindEntries({example, associated},
                                   net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_FeatureNotEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFirstPartySets,
      {{features::kFirstPartySetsClearSiteDataOnChangedSets.name, "false"}});
  base::HistogramTester histogram;
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  const std::string browser_context_id = "profile";
  const std::string input =
      R"({"primary": "https://foo.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  handler().SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(input));

  handler().Init(scoped_dir_.GetPath(), LocalSetDeclaration());
  ASSERT_THAT(GetSetsAndWait().FindEntries({foo, associated},
                                           net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(foo, net::FirstPartySetEntry(
                                foo, net::SiteType::kPrimary, absl::nullopt)),
                  Pair(associated, net::FirstPartySetEntry(
                                       foo, net::SiteType::kAssociated, 0))));

  histogram.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);

  ClearSiteDataOnChangedSetsForContextAndWait(
      context(), browser_context_id, net::FirstPartySetsContextConfig());

  absl::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      persisted = GetPersistedSetsAndWait(browser_context_id);
  EXPECT_TRUE(persisted.has_value());
  EXPECT_THAT(
      persisted->first.FindEntries({foo, associated}, persisted->second),
      IsEmpty());
  // Should not be recorded.
  histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_ManualSet_Successful) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFirstPartySets,
      {{features::kFirstPartySetsClearSiteDataOnChangedSets.name, "true"}});

  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  const std::string browser_context_id = "profile";

  base::HistogramTester histogram;
  FirstPartySetsHandlerImpl handler =
      FirstPartySetsHandlerImpl::CreateForTesting(true, false);
  const std::string input =
      R"({"primary": "https://foo.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));

  handler.Init(scoped_dir_.GetPath(), LocalSetDeclaration(input));

  // Should not yet be recorded.
  histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
  ClearSiteDataOnChangedSetsForContextAndWait(
      handler, context(), browser_context_id,
      net::FirstPartySetsContextConfig());

  absl::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      persisted = GetPersistedSetsAndWait(handler, browser_context_id);
  EXPECT_TRUE(persisted.has_value());
  EXPECT_THAT(
      persisted->first.FindEntries({foo, associated}, persisted->second),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            absl::nullopt)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                       absl::nullopt))));
  histogram.ExpectUniqueSample(kFirstPartySetsClearSiteDataOutcomeHistogram,
                               /*sample=*/true, 1);
  histogram.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_PublicSetsWithDiff_Successful) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFirstPartySets,
      {{features::kFirstPartySetsClearSiteDataOnChangedSets.name, "true"}});

  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));
  net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

  const std::string browser_context_id = "profile";

  {
    base::HistogramTester histogram;
    FirstPartySetsHandlerImpl handler =
        FirstPartySetsHandlerImpl::CreateForTesting(true, true);
    const std::string input =
        R"({"primary": "https://foo.test", )"
        R"("associatedSites": ["https://associatedsite.test"]})";
    ASSERT_TRUE(base::JSONReader::Read(input));
    handler.SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(input));

    handler.Init(scoped_dir_.GetPath(), LocalSetDeclaration());

    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(false));

    // Should not yet be recorded.
    histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
    ClearSiteDataOnChangedSetsForContextAndWait(
        handler, context(), browser_context_id,
        net::FirstPartySetsContextConfig());
    absl::optional<
        std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
        persisted = GetPersistedSetsAndWait(handler, browser_context_id);
    EXPECT_TRUE(persisted.has_value());
    EXPECT_THAT(
        persisted->first.FindEntries({foo, associated}, persisted->second),
        UnorderedElementsAre(
            Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                              absl::nullopt)),
            Pair(associated,
                 net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                         absl::nullopt))));
    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(true));

    histogram.ExpectUniqueSample(kFirstPartySetsClearSiteDataOutcomeHistogram,
                                 /*sample=*/true, 1);

    // Make sure the database is closed properly before being opened again.
    handler.SynchronouslyResetDBHelperForTesting();
  }

  // Verify FPS transition clearing is working for non-empty sites-to-clear
  // list.
  {
    base::HistogramTester histogram;
    FirstPartySetsHandlerImpl handler =
        FirstPartySetsHandlerImpl::CreateForTesting(true, true);
    const std::string input =
        R"({"primary": "https://foo.test", )"
        R"("associatedSites": ["https://associatedsite2.test"]})";
    ASSERT_TRUE(base::JSONReader::Read(input));
    // The new public sets need to be associated with a different version.
    handler.SetPublicFirstPartySets(base::Version("0.0.2"),
                                    WritePublicSetsFile(input));

    handler.Init(scoped_dir_.GetPath(), LocalSetDeclaration());

    // Should not yet be recorded.
    histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
    ClearSiteDataOnChangedSetsForContextAndWait(
        handler, context(), browser_context_id,
        net::FirstPartySetsContextConfig());
    absl::optional<
        std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
        persisted = GetPersistedSetsAndWait(handler, browser_context_id);
    EXPECT_TRUE(persisted.has_value());
    EXPECT_THAT(
        persisted->first.FindEntries({foo, associated2}, persisted->second),
        UnorderedElementsAre(
            Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                              absl::nullopt)),
            Pair(associated2,
                 net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                         absl::nullopt))));
    EXPECT_THAT(
        HasEntryInBrowserContextsClearedAndWait(handler, browser_context_id),
        Optional(true));

    histogram.ExpectUniqueSample(kFirstPartySetsClearSiteDataOutcomeHistogram,
                                 /*sample=*/true, 1);
  }
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_EmptyDBPath) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFirstPartySets,
      {{features::kFirstPartySetsClearSiteDataOnChangedSets.name, "true"}});

  base::HistogramTester histogram;
  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  const std::string browser_context_id = "profile";
  const std::string input =
      R"({"primary": "https://foo.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  handler().SetPublicFirstPartySets(base::Version("0.0.1"),
                                    WritePublicSetsFile(input));

  handler().Init(
      /*user_data_dir=*/{}, LocalSetDeclaration());
  ASSERT_THAT(GetSetsAndWait().FindEntries({foo, associated},
                                           net::FirstPartySetsContextConfig()),
              UnorderedElementsAre(
                  Pair(foo, net::FirstPartySetEntry(
                                foo, net::SiteType::kPrimary, absl::nullopt)),
                  Pair(associated, net::FirstPartySetEntry(
                                       foo, net::SiteType::kAssociated, 0))));

  base::RunLoop run_loop;
  ClearSiteDataOnChangedSetsForContextAndWait(
      context(), browser_context_id, net::FirstPartySetsContextConfig());

  EXPECT_EQ(GetPersistedSetsAndWait(browser_context_id), absl::nullopt);
  // Should not be recorded.
  histogram.ExpectTotalCount(kFirstPartySetsClearSiteDataOutcomeHistogram, 0);
  histogram.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ClearSiteDataOnChangedSetsForContext_BeforeSetsReady) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFirstPartySets,
      {{features::kFirstPartySetsClearSiteDataOnChangedSets.name, "true"}});

  base::HistogramTester histogram;

  handler().Init(scoped_dir_.GetPath(), LocalSetDeclaration());

  const std::string browser_context_id = "profile";
  base::test::TestFuture<net::FirstPartySetsContextConfig,
                         net::FirstPartySetsCacheFilter>
      future;
  handler().ClearSiteDataOnChangedSetsForContext(
      base::BindLambdaForTesting([&]() { return context(); }),
      browser_context_id, net::FirstPartySetsContextConfig(),
      future.GetCallback());

  handler().SetPublicFirstPartySets(
      base::Version("0.0.1"),
      WritePublicSetsFile(
          R"({"primary": "https://foo.test", )"
          R"("associatedSites": ["https://associatedsite.test"]})"));

  EXPECT_TRUE(future.Wait());

  net::SchemefulSite foo(GURL("https://foo.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  absl::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
      persisted = GetPersistedSetsAndWait(browser_context_id);
  EXPECT_TRUE(persisted.has_value());
  EXPECT_THAT(
      persisted->first.FindEntries({foo, associated}, persisted->second),
      UnorderedElementsAre(
          Pair(foo, net::FirstPartySetEntry(foo, net::SiteType::kPrimary,
                                            absl::nullopt)),
          Pair(associated,
               net::FirstPartySetEntry(foo, net::SiteType::kAssociated,
                                       absl::nullopt))));
  histogram.ExpectUniqueSample(kFirstPartySetsClearSiteDataOutcomeHistogram,
                               /*sample=*/true, 1);
  histogram.ExpectTotalCount(kDelayedQueriesCountHistogram, 1);
  histogram.ExpectTotalCount(kMostDelayedQueryDeltaHistogram, 1);
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_AfterSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));

  handler().Init(scoped_dir_.GetPath(), LocalSetDeclaration());

  // Wait until initialization is complete.
  GetSetsAndWait();

  EXPECT_THAT(
      handler()
          .GetSets(base::NullCallback())
          .value()
          .FindEntries({example, associated},
                       net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       GetSetsIfEnabledAndReady_BeforeSetsReady) {
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  // Call GetSets before the sets are ready, and before Init has been called.
  base::test::TestFuture<net::GlobalFirstPartySets> future;
  EXPECT_EQ(handler().GetSets(future.GetCallback()), absl::nullopt);

  handler().Init(scoped_dir_.GetPath(), LocalSetDeclaration());

  const std::string input =
      R"({"primary": "https://example.test", )"
      R"("associatedSites": ["https://associatedsite.test"]})";
  ASSERT_TRUE(base::JSONReader::Read(input));
  handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                    WritePublicSetsFile(input));

  EXPECT_THAT(
      future.Get().FindEntries({example, associated},
                               net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));

  EXPECT_THAT(
      handler()
          .GetSets(base::NullCallback())
          .value()
          .FindEntries({example, associated},
                       net::FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(example, net::FirstPartySetEntry(
                            example, net::SiteType::kPrimary, absl::nullopt)),
          Pair(associated, net::FirstPartySetEntry(
                               example, net::SiteType::kAssociated, 0))));
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ComputeFirstPartySetMetadata_SynchronousResult) {
  handler().Init(scoped_dir_.GetPath(), LocalSetDeclaration());

  handler().SetPublicFirstPartySets(
      base::Version("1.2.3"),
      WritePublicSetsFile(
          R"({"primary": "https://example.test", )"
          R"("associatedSites": ["https://associatedsite.test"]})"));

  // Exploit another helper to wait until the public sets file has been read.
  GetSetsAndWait();

  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));

  base::test::TestFuture<net::FirstPartySetMetadata> future;
  handler().ComputeFirstPartySetMetadata(example, &associated,
                                         /*party_context=*/{},
                                         net::FirstPartySetsContextConfig(),
                                         future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_NE(future.Take(), net::FirstPartySetMetadata());
}

TEST_F(FirstPartySetsHandlerImplEnabledTest,
       ComputeFirstPartySetMetadata_AsynchronousResult) {
  // Send query before the sets are ready.
  base::test::TestFuture<net::FirstPartySetMetadata> future;
  net::SchemefulSite example(GURL("https://example.test"));
  net::SchemefulSite associated(GURL("https://associatedsite.test"));
  handler().ComputeFirstPartySetMetadata(
      example, &associated, /*party_context=*/{},
      net::FirstPartySetsContextConfig(), future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  handler().Init(scoped_dir_.GetPath(), LocalSetDeclaration());

  handler().SetPublicFirstPartySets(
      base::Version("1.2.3"),
      WritePublicSetsFile(
          R"({"primary": "https://example.test", )"
          R"("associatedSites": ["https://associatedsite.test"]})"));

  EXPECT_NE(future.Get(), net::FirstPartySetMetadata());
}

class FirstPartySetsHandlerGetContextConfigForPolicyTest
    : public FirstPartySetsHandlerImplEnabledTest {
 public:
  FirstPartySetsHandlerGetContextConfigForPolicyTest() {
    handler().Init(scoped_dir_.GetPath(), LocalSetDeclaration());
  }

  // Writes the public list of First-Party Sets which GetContextConfigForPolicy
  // awaits.
  //
  // Initializes the First-Party Sets with the following relationship:
  //
  // [
  //   {
  //     "primary": "https://primary1.test",
  //     "associatedSites": ["https://associatedsite1.test",
  //     "https://associatedsite2.test"]
  //   }
  // ]
  void InitPublicFirstPartySets() {
    net::SchemefulSite primary1(GURL("https://primary1.test"));
    net::SchemefulSite associated1(GURL("https://associatedsite1.test"));
    net::SchemefulSite associated2(GURL("https://associatedsite2.test"));

    const std::string input =
        R"({"primary": "https://primary1.test", )"
        R"("associatedSites": ["https://associatedsite1.test", "https://associatedsite2.test"]})";
    ASSERT_TRUE(base::JSONReader::Read(input));
    handler().SetPublicFirstPartySets(base::Version("1.2.3"),
                                      WritePublicSetsFile(input));

    ASSERT_THAT(
        GetSetsAndWait().FindEntries({primary1, associated1, associated2},
                                     net::FirstPartySetsContextConfig()),
        SizeIs(3));
  }

 protected:
  base::OnceCallback<void(net::FirstPartySetsContextConfig)>
  GetConfigCallback() {
    return future_.GetCallback();
  }

  net::FirstPartySetsContextConfig GetConfig() { return future_.Take(); }

 private:
  base::test::TestFuture<net::FirstPartySetsContextConfig> future_;
};

TEST_F(FirstPartySetsHandlerGetContextConfigForPolicyTest,
       DefaultOverridesPolicy_DefaultContextConfigs) {
  base::Value policy = base::JSONReader::Read(R"({})").value();
  handler().GetContextConfigForPolicy(&policy.GetDict(), GetConfigCallback());

  InitPublicFirstPartySets();
  EXPECT_EQ(GetConfig(), net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsHandlerGetContextConfigForPolicyTest,
       MalformedOverridesPolicy_DefaultContextConfigs) {
  base::Value policy = base::JSONReader::Read(R"({
    "replacements": 123,
    "additions": true
  })")
                           .value();
  handler().GetContextConfigForPolicy(&policy.GetDict(), GetConfigCallback());

  InitPublicFirstPartySets();
  EXPECT_EQ(GetConfig(), net::FirstPartySetsContextConfig());
}

TEST_F(FirstPartySetsHandlerGetContextConfigForPolicyTest,
       NonDefaultOverridesPolicy_NonDefaultContextConfigs) {
  base::Value policy = base::JSONReader::Read(R"(
                {
                "replacements": [
                  {
                    "primary": "https://associatedsite1.test",
                    "associatedSites": ["https://primary3.test"]
                  }
                ],
                "additions": [
                  {
                    "primary": "https://primary2.test",
                    "associatedSites": ["https://associatedsite2.test"]
                  }
                ]
              }
            )")
                           .value();
  handler().GetContextConfigForPolicy(&policy.GetDict(), GetConfigCallback());

  InitPublicFirstPartySets();
  // We don't care what the customizations are, here; we only care that they're
  // not a no-op.
  EXPECT_FALSE(GetConfig().empty());
}

}  // namespace content
