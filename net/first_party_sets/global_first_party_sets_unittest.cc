// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/global_first_party_sets.h"

#include <set>
#include <string>

#include "base/containers/flat_map.h"
#include "base/version.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace net {

namespace {

const base::Version kVersion("1.2.3");
const SchemefulSite kPrimary(GURL("https://primary.test"));
const SchemefulSite kPrimary2(GURL("https://primary2.test"));
const SchemefulSite kPrimary3(GURL("https://primary3.test"));
const SchemefulSite kAssociated1(GURL("https://associated1.test"));
const SchemefulSite kAssociated1Cctld(GURL("https://associated1.cctld"));
const SchemefulSite kAssociated1Cctld2(GURL("https://associated1.cctld2"));
const SchemefulSite kAssociated2(GURL("https://associated2.test"));
const SchemefulSite kAssociated3(GURL("https://associated3.test"));
const SchemefulSite kAssociated4(GURL("https://associated4.test"));
const SchemefulSite kAssociated5(GURL("https://associated5.test"));
const SchemefulSite kService(GURL("https://service.test"));

base::flat_map<SchemefulSite, FirstPartySetEntry> CollectEffectiveSetEntries(
    const GlobalFirstPartySets& sets,
    const FirstPartySetsContextConfig& config) {
  base::flat_map<SchemefulSite, FirstPartySetEntry> got;
  sets.ForEachEffectiveSetEntry(
      config, [&](const SchemefulSite& site, const FirstPartySetEntry& entry) {
        DCHECK(!got.contains(site));
        got[site] = entry;
        return true;
      });

  // Consistency check: verify that all of the returned entries are what we'd
  // get if we called FindEntry directly.
  for (const auto& [site, entry] : got) {
    DCHECK_EQ(sets.FindEntry(site, config).value(), entry);
  }
  return got;
}

}  // namespace

class GlobalFirstPartySetsTest : public ::testing::Test {
 public:
  GlobalFirstPartySetsTest() = default;
};

TEST_F(GlobalFirstPartySetsTest, CtorSkipsInvalidVersion) {
  GlobalFirstPartySets sets(
      base::Version(), /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});

  EXPECT_THAT(
      sets.FindEntries({kPrimary, kAssociated1}, FirstPartySetsContextConfig()),
      IsEmpty());
}

TEST_F(GlobalFirstPartySetsTest, Clone) {
  base::Version version("1.2.3.4.5");
  const SchemefulSite example(GURL("https://example.test"));
  const SchemefulSite example_cctld(GURL("https://example.cctld"));
  const SchemefulSite member1(GURL("https://member1.test"));
  const FirstPartySetEntry entry(example, SiteType::kPrimary, absl::nullopt);
  const FirstPartySetEntry member1_entry(example, SiteType::kAssociated, 1);

  const SchemefulSite foo(GURL("https://foo.test"));
  const SchemefulSite member2(GURL("https://member2.test"));
  const FirstPartySetEntry foo_entry(foo, SiteType::kPrimary, absl::nullopt);
  const FirstPartySetEntry member2_entry(foo, SiteType::kAssociated, 1);

  GlobalFirstPartySets sets(version,
                            /*entries=*/
                            {{example, entry}, {member1, member1_entry}},
                            /*aliases=*/{{example_cctld, example}});
  sets.ApplyManuallySpecifiedSet({{foo, foo_entry}, {member2, member2_entry}});

  EXPECT_EQ(sets, sets.Clone());
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_Nonexistent) {
  SchemefulSite example(GURL("https://example.test"));

  EXPECT_THAT(
      GlobalFirstPartySets().FindEntry(example, FirstPartySetsContextConfig()),
      absl::nullopt);
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_Exists) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite decoy_site(GURL("https://decoy.test"));
  FirstPartySetEntry entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry decoy_entry(example, SiteType::kAssociated, 1);

  EXPECT_THAT(GlobalFirstPartySets(kVersion,
                                   {
                                       {example, entry},
                                       {decoy_site, decoy_entry},
                                   },
                                   {})
                  .FindEntry(example, FirstPartySetsContextConfig()),
              Optional(entry));
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_ExistsWhenNormalized) {
  SchemefulSite https_example(GURL("https://example.test"));
  SchemefulSite wss_example(GURL("wss://example.test"));
  FirstPartySetEntry entry(https_example, SiteType::kPrimary, absl::nullopt);

  EXPECT_THAT(GlobalFirstPartySets(kVersion,
                                   {
                                       {https_example, entry},
                                   },
                                   {})
                  .FindEntry(wss_example, FirstPartySetsContextConfig()),
              Optional(entry));
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_ExistsViaOverride) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  FirstPartySetsContextConfig config(
      {{example, net::FirstPartySetEntryOverride(override_entry)}});

  EXPECT_THAT(GlobalFirstPartySets(kVersion,
                                   {
                                       {example, public_entry},
                                   },
                                   {})
                  .FindEntry(example, config),
              Optional(override_entry));
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_RemovedViaOverride) {
  SchemefulSite example(GURL("https://example.test"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);

  FirstPartySetsContextConfig config(
      {{example, net::FirstPartySetEntryOverride()}});

  EXPECT_THAT(GlobalFirstPartySets(kVersion,
                                   {
                                       {example, public_entry},
                                   },
                                   {})
                  .FindEntry(example, config),
              absl::nullopt);
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_ExistsViaAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry entry(example, SiteType::kPrimary, absl::nullopt);

  EXPECT_THAT(GlobalFirstPartySets(kVersion,
                                   {
                                       {example, entry},
                                   },
                                   {{example_cctld, example}})
                  .FindEntry(example_cctld, FirstPartySetsContextConfig()),
              Optional(entry));
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_ExistsViaOverrideWithDecoyAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  FirstPartySetsContextConfig config(
      {{example_cctld, net::FirstPartySetEntryOverride(override_entry)}});

  EXPECT_THAT(GlobalFirstPartySets(kVersion,
                                   {
                                       {example, public_entry},
                                   },
                                   {{example_cctld, example}})
                  .FindEntry(example_cctld, config),
              Optional(override_entry));
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_RemovedViaOverrideWithDecoyAlias) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);

  FirstPartySetsContextConfig config(
      {{example_cctld, net::FirstPartySetEntryOverride()}});

  EXPECT_THAT(GlobalFirstPartySets(kVersion,
                                   {
                                       {example, public_entry},
                                   },
                                   {{example_cctld, example}})
                  .FindEntry(example_cctld, config),
              absl::nullopt);
}

TEST_F(GlobalFirstPartySetsTest, FindEntry_AliasesIgnoredForConfig) {
  SchemefulSite example(GURL("https://example.test"));
  SchemefulSite example_cctld(GURL("https://example.cctld"));
  FirstPartySetEntry public_entry(example, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry override_entry(example, SiteType::kAssociated, 1);

  FirstPartySetsContextConfig config(
      {{example, net::FirstPartySetEntryOverride(override_entry)}});

  // FindEntry should ignore aliases when using the customizations. Public
  // aliases only apply to sites in the public sets.
  EXPECT_THAT(GlobalFirstPartySets(kVersion,
                                   {
                                       {example, public_entry},
                                   },
                                   {{example_cctld, example}})
                  .FindEntry(example_cctld, config),
              public_entry);
}

TEST_F(GlobalFirstPartySetsTest, Empty_Empty) {
  EXPECT_TRUE(GlobalFirstPartySets().empty());
}

TEST_F(GlobalFirstPartySetsTest, Empty_NonemptyEntries) {
  EXPECT_FALSE(
      GlobalFirstPartySets(
          kVersion,
          {
              {kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
              {kAssociated4,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
          },
          {})
          .empty());
}

TEST_F(GlobalFirstPartySetsTest, Empty_NonemptyManualSet) {
  GlobalFirstPartySets sets;
  sets.ApplyManuallySpecifiedSet({
      {kPrimary,
       FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
      {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
  });
  EXPECT_FALSE(sets.empty());
}

TEST_F(GlobalFirstPartySetsTest, InvalidPublicSetsVersion_NonemptyManualSet) {
  GlobalFirstPartySets sets(
      base::Version(), /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});
  ASSERT_TRUE(sets.empty());
  sets.ApplyManuallySpecifiedSet({
      {kPrimary,
       FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
      {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
  });

  // The manual set should still be available, even though the component was
  // invalid.
  EXPECT_FALSE(sets.empty());
  EXPECT_THAT(
      sets.FindEntries({kPrimary, kAssociated1, kAssociated4},
                       FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kAssociated4,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0))));
}

TEST_F(GlobalFirstPartySetsTest,
       ForEachEffectiveSetEntry_ManualSetAndConfig_FullIteration) {
  GlobalFirstPartySets global_sets;
  global_sets.ApplyManuallySpecifiedSet({
      {kPrimary,
       FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
      {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      {kAssociated5, FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)},
  });

  // Modify kPrimary's set by removing kAssociated5 and modifying kAssociated4,
  // via policy.
  FirstPartySetsContextConfig config = global_sets.ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
              {kAssociated1, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)},
              {kAssociated1Cctld,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                  absl::nullopt)},
              {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)},
              {kService,
               FirstPartySetEntry(kPrimary, SiteType::kService, absl::nullopt)},
          },
      },
      /*addition_sets=*/{});

  // Note that since the policy sets take precedence over the manual set,
  // kAssociated5 is no longer in an FPS.
  EXPECT_THAT(
      CollectEffectiveSetEntries(global_sets, config),
      UnorderedElementsAre(
          Pair(kAssociated1Cctld,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kAssociated1, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kService, FirstPartySetEntry(kPrimary, SiteType::kService,
                                            absl::nullopt))));
}

class PopulatedGlobalFirstPartySetsTest : public GlobalFirstPartySetsTest {
 public:
  PopulatedGlobalFirstPartySetsTest()
      : global_sets_(
            kVersion,
            {
                {kPrimary, FirstPartySetEntry(kPrimary,
                                              SiteType::kPrimary,
                                              absl::nullopt)},
                {kAssociated1,
                 FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
                {kAssociated2,
                 FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)},
                {kService, FirstPartySetEntry(kPrimary,
                                              SiteType::kService,
                                              absl::nullopt)},
                {kPrimary2, FirstPartySetEntry(kPrimary2,
                                               SiteType::kPrimary,
                                               absl::nullopt)},
                {kAssociated3,
                 FirstPartySetEntry(kPrimary2, SiteType::kAssociated, 0)},
            },
            {
                {kAssociated1Cctld, kAssociated1},
            }) {}

  GlobalFirstPartySets& global_sets() { return global_sets_; }

 private:
  GlobalFirstPartySets global_sets_;
};

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ApplyManuallySpecifiedSet_DeduplicatesPrimaryPrimary) {
  // kPrimary overlaps as primary of both sets, so the existing set should be
  // wiped out.
  global_sets().ApplyManuallySpecifiedSet({
      {kPrimary,
       FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
      {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(
      global_sets().FindEntries(
          {
              kPrimary,
              kAssociated1,
              kAssociated2,
              kAssociated4,
              kService,
              kAssociated1Cctld,
          },
          FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kAssociated4,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ApplyManuallySpecifiedSet_DeduplicatesPrimaryNonprimary) {
  // kPrimary overlaps as a primary of the public set and non-primary of the CLI
  // set, so the existing set should be wiped out.
  global_sets().ApplyManuallySpecifiedSet({
      {kPrimary3,
       FirstPartySetEntry(kPrimary3, SiteType::kPrimary, absl::nullopt)},
      {kPrimary, FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(
      global_sets().FindEntries(
          {
              kPrimary,
              kAssociated1,
              kAssociated2,
              kAssociated4,
              kService,
              kPrimary3,
              kAssociated1Cctld,
          },
          FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(kPrimary3, FirstPartySetEntry(kPrimary3, SiteType::kPrimary,
                                             absl::nullopt)),
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ApplyManuallySpecifiedSet_DeduplicatesNonprimaryPrimary) {
  // kAssociated1 overlaps as a non-primary of the public set and primary of the
  // CLI set, so the CLI set should steal it and wipe out its alias, but
  // otherwise leave the set intact.
  global_sets().ApplyManuallySpecifiedSet({
      {kAssociated1,
       FirstPartySetEntry(kAssociated1, SiteType::kPrimary, absl::nullopt)},
      {kAssociated4,
       FirstPartySetEntry(kAssociated1, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(
      global_sets().FindEntries(
          {
              kPrimary,
              kAssociated1,
              kAssociated2,
              kAssociated4,
              kService,
              kPrimary3,
              kAssociated1Cctld,
          },
          FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)),
          Pair(kService,
               FirstPartySetEntry(kPrimary, SiteType::kService, absl::nullopt)),
          Pair(kAssociated1,
               FirstPartySetEntry(kAssociated1, SiteType::kPrimary,
                                  absl::nullopt)),
          Pair(kAssociated4,
               FirstPartySetEntry(kAssociated1, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ApplyManuallySpecifiedSet_DeduplicatesNonprimaryNonprimary) {
  // kAssociated1 overlaps as a non-primary of the public set and non-primary of
  // the CLI set, so the CLI set should steal it and wipe out its alias.
  global_sets().ApplyManuallySpecifiedSet({
      {kPrimary3,
       FirstPartySetEntry(kPrimary3, SiteType::kPrimary, absl::nullopt)},
      {kAssociated1, FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(
      global_sets().FindEntries(
          {
              kPrimary,
              kAssociated1,
              kAssociated2,
              kAssociated4,
              kService,
              kPrimary3,
              kAssociated1Cctld,
          },
          FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)),
          Pair(kService,
               FirstPartySetEntry(kPrimary, SiteType::kService, absl::nullopt)),
          Pair(kPrimary3, FirstPartySetEntry(kPrimary3, SiteType::kPrimary,
                                             absl::nullopt)),
          Pair(kAssociated1,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ApplyManuallySpecifiedSet_PrunesInducedSingletons) {
  // Steal kAssociated3, so that kPrimary2 becomes a singleton, and verify that
  // kPrimary2 is no longer considered in a set.
  global_sets().ApplyManuallySpecifiedSet({
      {kPrimary3,
       FirstPartySetEntry(kPrimary3, SiteType::kPrimary, absl::nullopt)},
      {kAssociated3, FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(
      global_sets().FindEntries({kPrimary2}, FirstPartySetsContextConfig()),
      IsEmpty());
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ApplyManuallySpecifiedSet_RespectsManualAlias) {
  // Both the public sets and the locally-defined set define an alias for
  // kAssociated1, but both define a different set for that site too.  Only the
  // locally-defined alias should be observable.
  global_sets().ApplyManuallySpecifiedSet({
      {kPrimary3,
       FirstPartySetEntry(kPrimary3, SiteType::kPrimary, absl::nullopt)},
      {kAssociated1, FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
      {kAssociated1Cctld2,
       FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(
      global_sets().FindEntries(
          {
              kAssociated1,
              kAssociated1Cctld,
              kAssociated1Cctld2,
          },
          FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(kAssociated1,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0)),
          Pair(kAssociated1Cctld2,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ForEachPublicSetEntry_FullIteration) {
  int count = 0;
  EXPECT_TRUE(global_sets().ForEachPublicSetEntry(
      [&](const SchemefulSite& site, const FirstPartySetEntry& entry) {
        ++count;
        return true;
      }));
  EXPECT_EQ(count, 7);
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ForEachPublicSetEntry_EarlyReturn) {
  int count = 0;
  EXPECT_FALSE(global_sets().ForEachPublicSetEntry(
      [&](const SchemefulSite& site, const FirstPartySetEntry& entry) {
        ++count;
        return count < 4;
      }));
  EXPECT_EQ(count, 4);
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ForEachEffectiveSetEntry_PublicSetsOnly_FullIteration) {
  EXPECT_THAT(
      CollectEffectiveSetEntries(global_sets(), FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(kAssociated1Cctld,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)),
          Pair(kAssociated1,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)),
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)),
          Pair(kAssociated3,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated, 0)),
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt)),
          Pair(kService, FirstPartySetEntry(kPrimary, SiteType::kService,
                                            absl::nullopt))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ForEachEffectiveSetEntry_PublicSetsWithManualSet_FullIteration) {
  // Replace kPrimary's set (including the alias and service site) with just
  // {kPrimary, kAssociated4}.
  global_sets().ApplyManuallySpecifiedSet({
      {kPrimary,
       FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
      {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
  });

  EXPECT_THAT(
      CollectEffectiveSetEntries(global_sets(), FirstPartySetsContextConfig()),
      UnorderedElementsAre(
          Pair(kAssociated3,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated, 0)),
          Pair(kAssociated4,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)),
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ForEachEffectiveSetEntry_PublicSetsWithConfig_FullIteration) {
  // Modify kPrimary's set by removing kAssociated2 and adding kAssociated4, via
  // policy.
  FirstPartySetsContextConfig config = global_sets().ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
              {kAssociated1, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)},
              {kAssociated1Cctld,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                  absl::nullopt)},
              {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)},
              {kService,
               FirstPartySetEntry(kPrimary, SiteType::kService, absl::nullopt)},
          },
      },
      /*addition_sets=*/{});

  EXPECT_THAT(
      CollectEffectiveSetEntries(global_sets(), config),
      UnorderedElementsAre(
          Pair(kAssociated1Cctld,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kAssociated1, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kAssociated3,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated, 0)),
          Pair(kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt)),
          Pair(kService, FirstPartySetEntry(kPrimary, SiteType::kService,
                                            absl::nullopt))));
}

TEST_F(
    PopulatedGlobalFirstPartySetsTest,
    ForEachEffectiveSetEntry_PublicSetsWithManualSetAndConfig_FullIteration) {
  // Replace kPrimary's set (including the alias and service site) with just
  // {kPrimary, kAssociated4, kAssociated5}.
  global_sets().ApplyManuallySpecifiedSet({
      {kPrimary,
       FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
      {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      {kAssociated5, FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)},
  });

  // Modify kPrimary's set by removing kAssociated2 and adding kAssociated4, via
  // policy.
  FirstPartySetsContextConfig config = global_sets().ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
              {kAssociated1, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)},
              {kAssociated1Cctld,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                  absl::nullopt)},
              {kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)},
              {kService,
               FirstPartySetEntry(kPrimary, SiteType::kService, absl::nullopt)},
          },
      },
      /*addition_sets=*/{});

  // Note that since the policy sets take precedence over the manual set,
  // kAssociated5 is no longer in an FPS.
  EXPECT_THAT(
      CollectEffectiveSetEntries(global_sets(), config),
      UnorderedElementsAre(
          Pair(kAssociated1Cctld,
               FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kAssociated1, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kAssociated3,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated, 0)),
          Pair(kAssociated4, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt)),
          Pair(kService, FirstPartySetEntry(kPrimary, SiteType::kService,
                                            absl::nullopt))));
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ComputeMetadata_EmptyContext) {
  SchemefulSite nonmember(GURL("https://nonmember.test"));

  for (const SchemefulSite* top_frame :
       std::initializer_list<const SchemefulSite*>{&kPrimary, nullptr}) {
    EXPECT_EQ(global_sets()
                  .ComputeMetadata(nonmember, top_frame, {},
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kCrossParty);

    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kPrimary, top_frame, {},
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);

    EXPECT_EQ(global_sets()
                  .ComputeMetadata(SchemefulSite(GURL("http://primary.test")),
                                   top_frame, {}, FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kCrossParty);
  }

  EXPECT_EQ(global_sets()
                .ComputeMetadata(kPrimary, &nonmember, {},
                                 FirstPartySetsContextConfig())
                .context()
                .context_type(),
            SamePartyContext::Type::kCrossParty);
  EXPECT_EQ(global_sets()
                .ComputeMetadata(nonmember, &kPrimary, {},
                                 FirstPartySetsContextConfig())
                .context()
                .context_type(),
            SamePartyContext::Type::kCrossParty);
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ComputeMetadata_ContextIsNonmember) {
  SchemefulSite nonmember(GURL("https://nonmember.test"));
  std::set<SchemefulSite> context({nonmember});

  for (const SchemefulSite* top_frame :
       std::initializer_list<const SchemefulSite*>{&kPrimary, nullptr}) {
    for (const SchemefulSite& site : std::initializer_list<SchemefulSite>{
             kPrimary,
             SchemefulSite(GURL("http://primary.test")),
             SchemefulSite(GURL("http://associated1.test")),
             SchemefulSite(GURL("http://primary2.test")),
             SchemefulSite(GURL("http://associated3.test")),
             nonmember,
         }) {
      EXPECT_EQ(global_sets()
                    .ComputeMetadata(site, top_frame, context,
                                     FirstPartySetsContextConfig())
                    .context()
                    .context_type(),
                SamePartyContext::Type::kCrossParty)
          << site;
    }
  }
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ComputeMetadata_ContextIsPrimary) {
  std::set<SchemefulSite> context({kPrimary});

  for (const SchemefulSite* top_frame :
       std::initializer_list<const SchemefulSite*>{&kPrimary, nullptr}) {
    for (const SchemefulSite& site : std::initializer_list<SchemefulSite>{
             SchemefulSite(GURL("http://primary.test")),
             kPrimary2,
             kAssociated3,
             SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(global_sets()
                    .ComputeMetadata(site, top_frame, context,
                                     FirstPartySetsContextConfig())
                    .context()
                    .context_type(),
                SamePartyContext::Type::kCrossParty)
          << site;
    }
    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kPrimary, top_frame, context,
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);

    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kAssociated1, top_frame, context,
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);
  }
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ComputeMetadata_ContextIsNonprimary) {
  std::set<SchemefulSite> context({kAssociated1});

  for (const SchemefulSite* top_frame :
       std::initializer_list<const SchemefulSite*>{&kPrimary, nullptr}) {
    for (const SchemefulSite& site : std::initializer_list<SchemefulSite>{
             SchemefulSite(GURL("http://primary.test")),
             kPrimary2,
             kAssociated3,
             SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(global_sets()
                    .ComputeMetadata(site, top_frame, context,
                                     FirstPartySetsContextConfig())
                    .context()
                    .context_type(),
                SamePartyContext::Type::kCrossParty)
          << site;
    }
    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kPrimary, top_frame, context,
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);

    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kPrimary, top_frame, context,
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);

    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kAssociated1, top_frame, context,
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);
  }
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ComputeMetadata_ContextIsPrimaryAndNonprimary) {
  std::set<SchemefulSite> context({kPrimary, kAssociated1});

  for (const SchemefulSite* top_frame :
       std::initializer_list<const SchemefulSite*>{&kPrimary, nullptr}) {
    for (const SchemefulSite& site : std::initializer_list<SchemefulSite>{
             SchemefulSite(GURL("http://primary.test")),
             kPrimary2,
             kAssociated3,
             SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(global_sets()
                    .ComputeMetadata(site, top_frame, context,
                                     FirstPartySetsContextConfig())
                    .context()
                    .context_type(),
                SamePartyContext::Type::kCrossParty)
          << site;
    }
    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kPrimary, top_frame, context,
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);

    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kAssociated1, top_frame, context,
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);

    EXPECT_EQ(global_sets()
                  .ComputeMetadata(kAssociated2, top_frame, context,
                                   FirstPartySetsContextConfig())
                  .context()
                  .context_type(),
              SamePartyContext::Type::kSameParty);
  }
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ComputeMetadata_ContextMixesParties) {
  std::set<SchemefulSite> context({kPrimary, kAssociated1, kPrimary2});

  for (const SchemefulSite* top_frame :
       std::initializer_list<const SchemefulSite*>{&kPrimary, nullptr}) {
    for (const SchemefulSite& site : std::initializer_list<SchemefulSite>{
             kPrimary,
             SchemefulSite(GURL("http://primary.test")),
             kAssociated1,
             kPrimary2,
             kAssociated3,
             SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(global_sets()
                    .ComputeMetadata(site, top_frame, context,
                                     FirstPartySetsContextConfig())
                    .context()
                    .context_type(),
                SamePartyContext::Type::kCrossParty)
          << site;
    }
  }
}

TEST_F(PopulatedGlobalFirstPartySetsTest,
       ComputeMetadata_ContextMixesMembersAndNonmembers) {
  std::set<SchemefulSite> context({
      kPrimary,
      kAssociated1,
      SchemefulSite(GURL("http://nonmember.test")),
  });

  for (const SchemefulSite* top_frame :
       std::initializer_list<const SchemefulSite*>{&kPrimary, nullptr}) {
    for (const SchemefulSite& site : std::initializer_list<SchemefulSite>{
             kPrimary,
             SchemefulSite(GURL("http://primary.test")),
             kAssociated1,
             kPrimary2,
             kAssociated3,
             SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(global_sets()
                    .ComputeMetadata(site, top_frame, context,
                                     FirstPartySetsContextConfig())
                    .context()
                    .context_type(),
                SamePartyContext::Type::kCrossParty)
          << site;
    }
  }
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ComputeMetadata_ContextMixesSchemes) {
  SchemefulSite primary_http(GURL("http://primary.test"));
  std::set<SchemefulSite> context({kPrimary, kAssociated1, primary_http});

  for (const SchemefulSite* top_frame :
       std::initializer_list<const SchemefulSite*>{&kPrimary, nullptr}) {
    for (const SchemefulSite& site : std::initializer_list<SchemefulSite>{
             kPrimary,
             primary_http,
             kAssociated1,
             kPrimary2,
             kAssociated3,
             SchemefulSite(GURL("https://nonmember.test")),
         }) {
      EXPECT_EQ(global_sets()
                    .ComputeMetadata(site, top_frame, context,
                                     FirstPartySetsContextConfig())
                    .context()
                    .context_type(),
                SamePartyContext::Type::kCrossParty)
          << site;
    }
  }
}

TEST_F(PopulatedGlobalFirstPartySetsTest, ComputeMetadata) {
  SchemefulSite nonmember(GURL("https://nonmember.test"));
  SchemefulSite nonmember1(GURL("https://nonmember1.test"));
  SchemefulSite wss_associated1(GURL("wss://associated1.test"));
  SchemefulSite wss_nonmember(GURL("wss://nonmember.test"));
  FirstPartySetEntry primary_entry(kPrimary, SiteType::kPrimary, absl::nullopt);
  FirstPartySetEntry associated_entry(kPrimary, SiteType::kAssociated, 0);

  // Works as usual for sites that are in First-Party sets.
  EXPECT_EQ(
      global_sets().ComputeMetadata(kAssociated1, &kAssociated1, {kAssociated1},
                                    FirstPartySetsContextConfig()),
      FirstPartySetMetadata(
          SamePartyContext(SamePartyContext::Type::kSameParty),
          &associated_entry, &associated_entry));
  EXPECT_EQ(
      global_sets().ComputeMetadata(kPrimary, &kAssociated1, {kAssociated1},
                                    FirstPartySetsContextConfig()),
      FirstPartySetMetadata(
          SamePartyContext(SamePartyContext::Type::kSameParty), &primary_entry,
          &associated_entry));
  EXPECT_EQ(
      global_sets().ComputeMetadata(kAssociated1, &kPrimary, {kAssociated1},
                                    FirstPartySetsContextConfig()),
      FirstPartySetMetadata(
          SamePartyContext(SamePartyContext::Type::kSameParty),
          &associated_entry, &primary_entry));
  EXPECT_EQ(
      global_sets().ComputeMetadata(kAssociated1, &kAssociated1, {kPrimary},
                                    FirstPartySetsContextConfig()),
      FirstPartySetMetadata(
          SamePartyContext(SamePartyContext::Type::kSameParty),
          &associated_entry, &associated_entry));
  EXPECT_EQ(global_sets().ComputeMetadata(kAssociated1, &kAssociated1,
                                          {kAssociated1, kPrimary},
                                          FirstPartySetsContextConfig()),
            FirstPartySetMetadata(
                SamePartyContext(SamePartyContext::Type::kSameParty),
                &associated_entry, &associated_entry));

  // Works if the site is provided with WSS scheme instead of HTTPS.
  EXPECT_EQ(global_sets().ComputeMetadata(wss_associated1, &kAssociated1,
                                          {kAssociated1, kPrimary},
                                          FirstPartySetsContextConfig()),
            FirstPartySetMetadata(
                SamePartyContext(SamePartyContext::Type::kSameParty),
                &associated_entry, &associated_entry));

  EXPECT_EQ(
      global_sets().ComputeMetadata(nonmember, &kAssociated1, {kAssociated1},
                                    FirstPartySetsContextConfig()),
      FirstPartySetMetadata(
          SamePartyContext(SamePartyContext::Type::kCrossParty), nullptr,
          &associated_entry));
  EXPECT_EQ(
      global_sets().ComputeMetadata(kAssociated1, &nonmember, {kAssociated1},
                                    FirstPartySetsContextConfig()),
      FirstPartySetMetadata(
          SamePartyContext(SamePartyContext::Type::kCrossParty),
          &associated_entry, nullptr));
  EXPECT_EQ(global_sets().ComputeMetadata(wss_nonmember, &wss_associated1,
                                          {kAssociated1, kPrimary},
                                          FirstPartySetsContextConfig()),
            FirstPartySetMetadata(
                SamePartyContext(SamePartyContext::Type::kCrossParty), nullptr,
                &associated_entry));

  EXPECT_EQ(global_sets().ComputeMetadata(nonmember, &nonmember, {nonmember},
                                          FirstPartySetsContextConfig()),
            FirstPartySetMetadata(
                SamePartyContext(SamePartyContext::Type::kCrossParty), nullptr,
                nullptr));

  EXPECT_EQ(global_sets().ComputeMetadata(kAssociated1, &kAssociated1,
                                          {kAssociated1, nonmember},
                                          FirstPartySetsContextConfig()),
            FirstPartySetMetadata(
                SamePartyContext(SamePartyContext::Type::kCrossParty),
                &associated_entry, &associated_entry));
}

TEST_F(GlobalFirstPartySetsTest, ComputeConfig_Empty) {
  EXPECT_EQ(GlobalFirstPartySets(
                kVersion,
                /*entries=*/
                {
                    {kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary,
                                                  absl::nullopt)},
                    {kAssociated1,
                     FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
                },
                /*aliases=*/{})
                .ComputeConfig({}, {}),
            FirstPartySetsContextConfig());
}

TEST_F(GlobalFirstPartySetsTest,
       ComputeConfig_Replacements_NoIntersection_NoRemoval) {
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt)},
              {kAssociated2,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)},
          },
      },
      /*addition_sets=*/{});
  EXPECT_THAT(
      sets.FindEntries({kAssociated2, kPrimary2}, config),
      UnorderedElementsAre(
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt))));
}

// The common associated site between the policy and existing set is removed
// from its previous set.
TEST_F(
    GlobalFirstPartySetsTest,
    ComputeConfig_Replacements_ReplacesExistingAssociatedSite_RemovedFromFormerSet) {
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
          {kAssociated2,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt)},
              {kAssociated2,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)},
          },
      },
      /*addition_sets=*/{});
  EXPECT_THAT(
      sets.FindEntries({kPrimary2, kAssociated2}, config),
      UnorderedElementsAre(
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt))));
}

// The common primary between the policy and existing set is removed and its
// former associated sites are removed since they are now unowned.
TEST_F(
    GlobalFirstPartySetsTest,
    ComputeConfig_Replacements_ReplacesExistingPrimary_RemovesFormerAssociatedSites) {
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
          {kAssociated2,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
              {kAssociated3, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)},
          },
      },
      /*addition_sets=*/{});
  EXPECT_THAT(
      sets.FindEntries({kAssociated3, kPrimary, kAssociated1, kAssociated2},
                       config),
      UnorderedElementsAre(
          Pair(kAssociated3, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary,
                                            absl::nullopt))));
}

// The common associated site between the policy and existing set is removed and
// any leftover singletons are deleted.
TEST_F(
    GlobalFirstPartySetsTest,
    ComputeConfig_Replacements_ReplacesExistingAssociatedSite_RemovesSingletons) {
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary3, FirstPartySetEntry(kPrimary3, SiteType::kPrimary,
                                             absl::nullopt)},
              {kAssociated1,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated,
                                  absl::nullopt)},
          },
      },
      /*addition_sets=*/{});
  EXPECT_THAT(
      sets.FindEntries({kAssociated1, kPrimary3, kPrimary}, config),
      UnorderedElementsAre(
          Pair(kAssociated1,
               FirstPartySetEntry(kPrimary3, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kPrimary3, FirstPartySetEntry(kPrimary3, SiteType::kPrimary,
                                             absl::nullopt))));
}

// The policy set and the existing set have nothing in common so the policy set
// gets added in without updating the existing set.
TEST_F(GlobalFirstPartySetsTest,
       ComputeConfig_Additions_NoIntersection_AddsWithoutUpdating) {
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/{},
      /*addition_sets=*/{
          {
              {kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt)},
              {kAssociated2,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)},
          },
      });
  EXPECT_THAT(
      sets.FindEntries({kAssociated2, kPrimary2}, config),
      UnorderedElementsAre(
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt))));
}

// The primary of a policy set is also an associated site in an existing set.
// The policy set absorbs all sites in the existing set into its
// associated sites.
TEST_F(
    GlobalFirstPartySetsTest,
    ComputeConfig_Additions_PolicyPrimaryIsExistingAssociatedSite_PolicySetAbsorbsExistingSet) {
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/{},
      /*addition_sets=*/{
          {
              {kAssociated1,
               FirstPartySetEntry(kAssociated1, SiteType::kPrimary,
                                  absl::nullopt)},
              {kAssociated2,
               FirstPartySetEntry(kAssociated1, SiteType::kAssociated,
                                  absl::nullopt)},
              {kAssociated3,
               FirstPartySetEntry(kAssociated1, SiteType::kAssociated,
                                  absl::nullopt)},
          },
      });
  EXPECT_THAT(
      sets.FindEntries({kPrimary, kAssociated2, kAssociated3, kAssociated1},
                       config),
      UnorderedElementsAre(
          Pair(kPrimary, FirstPartySetEntry(kAssociated1, SiteType::kAssociated,
                                            absl::nullopt)),
          Pair(kAssociated2,
               FirstPartySetEntry(kAssociated1, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kAssociated3,
               FirstPartySetEntry(kAssociated1, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kAssociated1,
               FirstPartySetEntry(kAssociated1, SiteType::kPrimary,
                                  absl::nullopt))));
}

// The primary of a policy set is also a primary of an existing set.
// The policy set absorbs all of its primary's existing associated sites into
// its associated sites.
TEST_F(
    GlobalFirstPartySetsTest,
    ComputeConfig_Additions_PolicyPrimaryIsExistingPrimary_PolicySetAbsorbsExistingAssociatedSites) {
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
          {kAssociated3,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/{},
      /*addition_sets=*/{{
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated2,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, absl::nullopt)},
      }});
  EXPECT_THAT(
      sets.FindEntries({kAssociated1, kAssociated2, kAssociated3, kPrimary},
                       config),
      UnorderedElementsAre(
          Pair(kAssociated1, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kAssociated2, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kAssociated3, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kPrimary, FirstPartySetEntry(kPrimary, SiteType::kPrimary,
                                            absl::nullopt))));
}

// Existing set overlaps with both replacement and addition set.
TEST_F(
    GlobalFirstPartySetsTest,
    ComputeConfig_ReplacementsAndAdditions_SetListsOverlapWithSameExistingSet) {
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
          {kAssociated2,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 1)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt)},
              {kAssociated1,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)},
          },
      },
      /*addition_sets=*/{
          {
              {kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
              {kAssociated3, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)},
          },
      });
  EXPECT_THAT(
      sets.FindEntries(
          {kAssociated1, kAssociated2, kAssociated3, kPrimary, kPrimary2},
          config),
      UnorderedElementsAre(
          Pair(kAssociated1,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kAssociated2, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kAssociated3, FirstPartySetEntry(kPrimary, SiteType::kAssociated,
                                                absl::nullopt)),
          Pair(kPrimary,
               FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt))));
}

TEST_F(GlobalFirstPartySetsTest, TransitiveOverlap_TwoCommonPrimaries) {
  SchemefulSite primary0(GURL("https://primary0.test"));
  SchemefulSite associated_site0(GURL("https://associatedsite0.test"));
  SchemefulSite primary1(GURL("https://primary1.test"));
  SchemefulSite associated_site1(GURL("https://associatedsite1.test"));
  SchemefulSite primary2(GURL("https://primary2.test"));
  SchemefulSite associated_site2(GURL("https://associatedsite2.test"));
  SchemefulSite primary42(GURL("https://primary42.test"));
  SchemefulSite associated_site42(GURL("https://associatedsite42.test"));
  // {primary1, {associated_site1}} and {primary2, {associated_site2}}
  // transitively overlap with the existing set. primary1 takes primaryship of
  // the normalized addition set since it was provided first. The other addition
  // sets are unaffected.
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {primary1,
           FirstPartySetEntry(primary1, SiteType::kPrimary, absl::nullopt)},
          {primary2, FirstPartySetEntry(primary1, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/{},
      /*addition_sets=*/{
          {{primary0,
            FirstPartySetEntry(primary0, SiteType::kPrimary, absl::nullopt)},
           {associated_site0,
            FirstPartySetEntry(primary0, SiteType::kAssociated,
                               absl::nullopt)}},
          {{primary1,
            FirstPartySetEntry(primary1, SiteType::kPrimary, absl::nullopt)},
           {associated_site1,
            FirstPartySetEntry(primary1, SiteType::kAssociated,
                               absl::nullopt)}},
          {{primary2,
            FirstPartySetEntry(primary2, SiteType::kPrimary, absl::nullopt)},
           {associated_site2,
            FirstPartySetEntry(primary2, SiteType::kAssociated,
                               absl::nullopt)}},
          {{primary42,
            FirstPartySetEntry(primary42, SiteType::kPrimary, absl::nullopt)},
           {associated_site42,
            FirstPartySetEntry(primary42, SiteType::kAssociated,
                               absl::nullopt)}},
      });
  EXPECT_THAT(
      sets.FindEntries(
          {
              associated_site0,
              associated_site1,
              associated_site2,
              associated_site42,
              primary0,
              primary1,
              primary2,
              primary42,
          },
          config),
      UnorderedElementsAre(
          Pair(associated_site0,
               FirstPartySetEntry(primary0, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(associated_site1,
               FirstPartySetEntry(primary1, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(associated_site2,
               FirstPartySetEntry(primary1, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(associated_site42,
               FirstPartySetEntry(primary42, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(primary0,
               FirstPartySetEntry(primary0, SiteType::kPrimary, absl::nullopt)),
          Pair(primary1,
               FirstPartySetEntry(primary1, SiteType::kPrimary, absl::nullopt)),
          Pair(primary2, FirstPartySetEntry(primary1, SiteType::kAssociated,
                                            absl::nullopt)),
          Pair(primary42, FirstPartySetEntry(primary42, SiteType::kPrimary,
                                             absl::nullopt))));
}

TEST_F(GlobalFirstPartySetsTest, TransitiveOverlap_TwoCommonAssociatedSites) {
  SchemefulSite primary0(GURL("https://primary0.test"));
  SchemefulSite associated_site0(GURL("https://associatedsite0.test"));
  SchemefulSite primary1(GURL("https://primary1.test"));
  SchemefulSite associated_site1(GURL("https://associatedsite1.test"));
  SchemefulSite primary2(GURL("https://primary2.test"));
  SchemefulSite associated_site2(GURL("https://associatedsite2.test"));
  SchemefulSite primary42(GURL("https://primary42.test"));
  SchemefulSite associated_site42(GURL("https://associatedsite42.test"));
  // {primary1, {associated_site1}} and {primary2, {associated_site2}}
  // transitively overlap with the existing set. primary2 takes primaryship of
  // the normalized addition set since it was provided first. The other addition
  // sets are unaffected.
  GlobalFirstPartySets sets(
      kVersion,
      /*entries=*/
      {
          {primary2,
           FirstPartySetEntry(primary2, SiteType::kPrimary, absl::nullopt)},
          {primary1, FirstPartySetEntry(primary2, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});
  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/{},
      /*addition_sets=*/{
          {{primary0,
            FirstPartySetEntry(primary0, SiteType::kPrimary, absl::nullopt)},
           {associated_site0,
            FirstPartySetEntry(primary0, SiteType::kAssociated,
                               absl::nullopt)}},
          {{primary2,
            FirstPartySetEntry(primary2, SiteType::kPrimary, absl::nullopt)},
           {associated_site2,
            FirstPartySetEntry(primary2, SiteType::kAssociated,
                               absl::nullopt)}},
          {{primary1,
            FirstPartySetEntry(primary1, SiteType::kPrimary, absl::nullopt)},
           {associated_site1,
            FirstPartySetEntry(primary1, SiteType::kAssociated,
                               absl::nullopt)}},
          {{primary42,
            FirstPartySetEntry(primary42, SiteType::kPrimary, absl::nullopt)},
           {associated_site42,
            FirstPartySetEntry(primary42, SiteType::kAssociated,
                               absl::nullopt)}},
      });
  EXPECT_THAT(
      sets.FindEntries(
          {
              associated_site0,
              associated_site1,
              associated_site2,
              associated_site42,
              primary0,
              primary1,
              primary2,
              primary42,
          },
          config),
      UnorderedElementsAre(
          Pair(associated_site0,
               FirstPartySetEntry(primary0, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(associated_site1,
               FirstPartySetEntry(primary2, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(associated_site2,
               FirstPartySetEntry(primary2, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(associated_site42,
               FirstPartySetEntry(primary42, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(primary0,
               FirstPartySetEntry(primary0, SiteType::kPrimary, absl::nullopt)),
          Pair(primary1, FirstPartySetEntry(primary2, SiteType::kAssociated,
                                            absl::nullopt)),
          Pair(primary2,
               FirstPartySetEntry(primary2, SiteType::kPrimary, absl::nullopt)),
          Pair(primary42, FirstPartySetEntry(primary42, SiteType::kPrimary,
                                             absl::nullopt))));
}

TEST_F(GlobalFirstPartySetsTest, InvalidPublicSetsVersion_ComputeConfig) {
  const GlobalFirstPartySets sets(
      base::Version(), /*entries=*/
      {
          {kPrimary,
           FirstPartySetEntry(kPrimary, SiteType::kPrimary, absl::nullopt)},
          {kAssociated1,
           FirstPartySetEntry(kPrimary, SiteType::kAssociated, 0)},
      },
      /*aliases=*/{});
  ASSERT_TRUE(sets.empty());

  FirstPartySetsContextConfig config = sets.ComputeConfig(
      /*replacement_sets=*/
      {
          {
              {kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt)},
              {kAssociated2,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)},
          },
      },
      /*addition_sets=*/{});

  // The config should still be nonempty, even though the component was invalid.
  EXPECT_FALSE(config.empty());

  EXPECT_THAT(
      sets.FindEntries(
          {
              kPrimary,
              kPrimary2,
              kAssociated1,
              kAssociated2,
          },
          config),
      UnorderedElementsAre(
          Pair(kAssociated2,
               FirstPartySetEntry(kPrimary2, SiteType::kAssociated,
                                  absl::nullopt)),
          Pair(kPrimary2, FirstPartySetEntry(kPrimary2, SiteType::kPrimary,
                                             absl::nullopt))));
}

class GlobalFirstPartySetsWithConfigTest
    : public PopulatedGlobalFirstPartySetsTest {
 public:
  GlobalFirstPartySetsWithConfigTest()
      : config_({
            // New entry:
            {kPrimary3, net::FirstPartySetEntryOverride(
                            FirstPartySetEntry(kPrimary3,
                                               SiteType::kPrimary,
                                               absl::nullopt))},
            // Removed entry:
            {kAssociated1, net::FirstPartySetEntryOverride()},
            // Remapped entry:
            {kAssociated3,
             net::FirstPartySetEntryOverride(
                 FirstPartySetEntry(kPrimary3, SiteType::kAssociated, 0))},
            // Removed alias:
            {kAssociated1Cctld, net::FirstPartySetEntryOverride()},
        }) {}

  FirstPartySetsContextConfig& config() { return config_; }

 private:
  FirstPartySetsContextConfig config_;
};

TEST_F(GlobalFirstPartySetsWithConfigTest, ComputeMetadata) {
  FirstPartySetEntry example_primary_entry(kPrimary, SiteType::kPrimary,
                                           absl::nullopt);
  FirstPartySetEntry foo_primary_entry(kPrimary3, SiteType::kPrimary,
                                       absl::nullopt);
  FirstPartySetEntry foo_associated_entry(kPrimary3, SiteType::kAssociated, 0);

  // kAssociated1 has been removed from its set.
  EXPECT_EQ(
      global_sets().ComputeMetadata(kAssociated1, &kPrimary, {}, config()),
      FirstPartySetMetadata(
          SamePartyContext(SamePartyContext::Type::kCrossParty), nullptr,
          &example_primary_entry));

  // kAssociated3 and kPrimary3 are sites in a new set.
  EXPECT_EQ(
      global_sets().ComputeMetadata(kAssociated3, &kPrimary3, {}, config()),
      FirstPartySetMetadata(
          SamePartyContext(SamePartyContext::Type::kSameParty),
          &foo_associated_entry, &foo_primary_entry));
}

}  // namespace net
