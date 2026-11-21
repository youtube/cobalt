// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_support.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_features.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "partition_alloc/partition_alloc_base/cpu.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/superpage_region_32bit.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::allocator {

using testing::AllOf;
using testing::HasSubstr;

TEST(PartitionAllocSupportTest,
     ProposeSyntheticFinchTrials_DanglingPointerDetector) {
  std::string dpd_group =
      ProposeSyntheticFinchTrials()["DanglingPointerDetector"];

#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  EXPECT_EQ(dpd_group, "Enabled");
#else
  EXPECT_EQ(dpd_group, "Disabled");
#endif
}

// - Death tests misbehave on Android, http://crbug.com/643760.
#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) && !BUILDFLAG(IS_ANDROID) && \
    defined(GTEST_HAS_DEATH_TEST)

namespace {

// Install dangling raw_ptr handler and restore them when going out of scope.
class ScopedInstallDanglingRawPtrChecks {
 public:
  struct ConstructorParams {
    std::string mode = "crash";
    std::string type = "all";
  };
  explicit ScopedInstallDanglingRawPtrChecks(ConstructorParams params) {
    enabled_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPartitionAllocDanglingPtr,
          {{"mode", params.mode}, {"type", params.type}}}},
        {/* disabled_features */});

    old_detected_fn_ = partition_alloc::GetDanglingRawPtrDetectedFn();
    old_dereferenced_fn_ = partition_alloc::GetDanglingRawPtrReleasedFn();
    InstallDanglingRawPtrChecks();
  }
  ScopedInstallDanglingRawPtrChecks()
      : ScopedInstallDanglingRawPtrChecks(ConstructorParams{}) {}
  ~ScopedInstallDanglingRawPtrChecks() {
    InstallDanglingRawPtrChecks();  // Check for leaks.
    partition_alloc::SetDanglingRawPtrDetectedFn(old_detected_fn_);
    partition_alloc::SetDanglingRawPtrReleasedFn(old_dereferenced_fn_);
  }

 private:
  test::ScopedFeatureList enabled_feature_list_;
  partition_alloc::DanglingRawPtrDetectedFn* old_detected_fn_;
  partition_alloc::DanglingRawPtrReleasedFn* old_dereferenced_fn_;
};

}  // namespace

TEST(PartitionAllocDanglingPtrChecks, Basic) {
  EXPECT_DEATH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
        partition_alloc::GetDanglingRawPtrDetectedFn()(42);
        partition_alloc::GetDanglingRawPtrReleasedFn()(42);
      },
      AllOf(HasSubstr("[DanglingSignature]\t"),
            HasSubstr("[DanglingPtr](1/3) A raw_ptr/raw_ref is dangling."),
            HasSubstr("[DanglingPtr](2/3) First, the memory was freed at:"),
            HasSubstr("[DanglingPtr](3/3) Later, the dangling raw_ptr was "
                      "released at:")));
}

// The StackTrace buffer might run out of storage and not record where the
// memory was freed. Anyway, it must still report the error.
TEST(PartitionAllocDanglingPtrChecks, FreeNotRecorded) {
  EXPECT_DEATH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
        partition_alloc::GetDanglingRawPtrReleasedFn()(42);
      },
      AllOf(HasSubstr("[DanglingSignature]\tmissing\tmissing\t"),
            HasSubstr("[DanglingPtr](1/3) A raw_ptr/raw_ref is dangling."),
            HasSubstr("[DanglingPtr](2/3) It was not recorded where the memory "
                      "was freed."),
            HasSubstr("[DanglingPtr](3/3) Later, the dangling raw_ptr was "
                      "released at:")));
}

// TODO(crbug.com/40260713): Check for leaked refcount on Android.
#if BUILDFLAG(IS_ANDROID)
// Some raw_ptr might never release their refcount. Make sure this cause a
// crash on exit.
TEST(PartitionAllocDanglingPtrChecks, ReleaseNotRecorded) {
  EXPECT_DEATH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
        partition_alloc::GetDanglingRawPtrDetectedFn()(42);
      },
      HasSubstr("A freed allocation is still referenced by a dangling pointer "
                "at exit, or at test end. Leaked raw_ptr/raw_ref "
                "could cause PartitionAlloc's quarantine memory bloat."
                "\n\n"
                "Memory was released on:"));
}
#endif

// Getting the same allocation reported twice in a row, without matching
// `DanglingRawPtrReleased` in between is unexpected. Make sure this kind of
// potential regression would be detected.
TEST(PartitionAllocDanglingPtrChecks, DoubleDetection) {
  EXPECT_DCHECK_DEATH_WITH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks;
        partition_alloc::GetDanglingRawPtrDetectedFn()(42);
        partition_alloc::GetDanglingRawPtrDetectedFn()(42);
      },
      "Check failed: !entry \\|\\| entry->id != id");
}

// Free and release from two different tasks with cross task dangling pointer
// detection enabled.
TEST(PartitionAllocDanglingPtrChecks, CrossTask) {
  BASE_EXPECT_DEATH(
      {
        ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
            .type = "cross_task",
        });

        base::test::TaskEnvironment task_environment;
        task_environment.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(partition_alloc::GetDanglingRawPtrDetectedFn(), 42));
        task_environment.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(partition_alloc::GetDanglingRawPtrReleasedFn(), 42));

        task_environment.RunUntilIdle();
      },
      AllOf(HasSubstr("[DanglingSignature]\t"),
            HasSubstr("[DanglingPtr](1/3) A raw_ptr/raw_ref is dangling."),
            HasSubstr("[DanglingPtr](2/3) First, the memory was freed at:"),
            HasSubstr("[DanglingPtr](3/3) Later, the dangling raw_ptr was "
                      "released at:")));
}

TEST(PartitionAllocDanglingPtrChecks, CrossTaskIgnoredFailuresClearsCache) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
      .type = "cross_task",
  });

  base::test::TaskEnvironment task_environment;
  partition_alloc::GetDanglingRawPtrDetectedFn()(42);
  partition_alloc::GetDanglingRawPtrReleasedFn()(42);
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(partition_alloc::GetDanglingRawPtrReleasedFn(), 42));
  task_environment.RunUntilIdle();
}

TEST(PartitionAllocDanglingPtrChecks, CrossTaskIgnoresNoTask) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
      .type = "cross_task",
  });

  partition_alloc::GetDanglingRawPtrDetectedFn()(42);
  partition_alloc::GetDanglingRawPtrReleasedFn()(42);
}

TEST(PartitionAllocDanglingPtrChecks, CrossTaskIgnoresSameTask) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
      .type = "cross_task",
  });

  base::test::TaskEnvironment task_environment;
  task_environment.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce([] {
        partition_alloc::GetDanglingRawPtrDetectedFn()(37);
        partition_alloc::GetDanglingRawPtrReleasedFn()(37);
      }));
  task_environment.RunUntilIdle();
}

TEST(PartitionAllocDanglingPtrChecks, CrossTaskNoFreeConsideredCrossTask) {
  ScopedInstallDanglingRawPtrChecks scoped_install_dangling_checks({
      .type = "cross_task",
  });
  partition_alloc::GetDanglingRawPtrReleasedFn()(42);
}

TEST(PartitionAllocDanglingPtrChecks,
     ExtractDanglingPtrSignatureMacStackTrace) {
  const std::string stack_trace_output =
      "0   lib_1  0x0000000115fdfa12 base::F1(**) + 18\r\n"
      "1   lib_1  0x0000000115ec0043 base::F2() + 19\r\n"
      "2   lib_1  0x000000011601fb01 "
      "allocator_shim::internal::PartitionFree(foo) + 13265\r\n"
      "3   lib_1  0x0000000114831027 base::F3(bar) + 42\r\n"
      "4   lib_2  0x00000001148eae35 base::F4() + 437\r\n";
  EXPECT_EQ("base::F3(bar)",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                stack_trace_output));
}

TEST(PartitionAllocDanglingPtrChecks, ExtractDanglingPtrSignatureMacTaskTrace) {
  const std::string task_trace_output =
      "Task trace:\r\n"
      "0   lib_1  0x00000001161fd431 base::F1() + 257\r\n"
      "1   lib_1  0x0000000115a49404 base::F2() + 68\r\n";
  EXPECT_EQ("base::F1()",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                task_trace_output));
}

TEST(PartitionAllocDanglingPtrChecks,
     ExtractDanglingPtrSignatureWindowsStackTrace) {
  const std::string stack_trace_output =
      "\tbase::F1 [0x055643C3+19] (o:\\base\\F1.cc:329)\r\n"
      "\tallocator_shim::internal::PartitionFree [0x0648F87B+5243] "
      "(o:\\path.cc:441)\r\n"
      "\t_free_base [0x0558475D+29] (o:\\file_path.cc:142)\r\n"
      "\tbase::F2 [0x04E5B317+23] (o:\\base\\F2.cc:91)\r\n"
      "\tbase::F3 [0x04897800+544] (o:\\base\\F3.cc:638)\r\n";
  EXPECT_EQ("base::F2",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                stack_trace_output));
}

TEST(PartitionAllocDanglingPtrChecks,
     ExtractDanglingPtrSignatureWindowsTaskTrace) {
  const std::string task_trace_output =
      "Task trace:\r\n"
      "\tbase::F1 [0x049068A3+813] (o:\\base\\F1.cc:207)\r\n"
      "\tbase::F2 [0x0490614C+192] (o:\\base\\F2.cc:116)\r\n";
  EXPECT_EQ("base::F1",
            PartitionAllocSupport::ExtractDanglingPtrSignatureForTests(
                task_trace_output));
}

#endif

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
TEST(PartitionAllocSupportTest,
     ProposeSyntheticFinchTrials_MemoryTaggingDogfood) {
  {
    test::ScopedFeatureList scope;
    scope.InitWithFeatures({}, {features::kPartitionAllocMemoryTagging});

    auto trials = ProposeSyntheticFinchTrials();

    auto group_iter = trials.find("MemoryTaggingDogfood");
    EXPECT_EQ(group_iter, trials.end());
  }

  {
    test::ScopedFeatureList scope;
    scope.InitWithFeatures({features::kPartitionAllocMemoryTagging}, {});

    auto trials = ProposeSyntheticFinchTrials();

    std::string expectation =
        partition_alloc::internal::base::CPU::GetInstanceNoAllocation()
                .has_mte()
            ? "Enabled"
            : "Disabled";
    auto group_iter = trials.find("MemoryTaggingDogfood");
    EXPECT_NE(group_iter, trials.end());
    EXPECT_EQ(group_iter->second, expectation);
  }
}
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

class MemoryReclaimerSupportTest : public ::testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {base::features::kPartitionAllocMemoryReclaimer,
         base::allocator::kDisableMemoryReclaimerInBackground},
        {});
    MemoryReclaimerSupport::Instance().ResetForTesting();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::ScopedFeatureList feature_list_;
};

TEST_F(MemoryReclaimerSupportTest, StartSeveralTimes) {
  test::ScopedFeatureList feature_list{
      base::features::kPartitionAllocMemoryReclaimer};
  auto& instance = MemoryReclaimerSupport::Instance();
  EXPECT_FALSE(instance.has_pending_task_for_testing());
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  // Only one task.
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
}

TEST_F(MemoryReclaimerSupportTest, ForegroundToBackground) {
  test::ScopedFeatureList feature_list{
      base::features::kPartitionAllocMemoryReclaimer};
  auto& instance = MemoryReclaimerSupport::Instance();
  EXPECT_FALSE(instance.has_pending_task_for_testing());
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  task_environment_.FastForwardBy(
      MemoryReclaimerSupport::kFirstPAPurgeOrReclaimDelay);
  // Task gets reposted.
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  instance.SetForegrounded(false);
  task_environment_.FastForwardBy(MemoryReclaimerSupport::GetInterval());
  // But not once in background.
  EXPECT_FALSE(instance.has_pending_task_for_testing());
  EXPECT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());
}

TEST_F(MemoryReclaimerSupportTest, ForegroundToBackgroundAndBack) {
  test::ScopedFeatureList feature_list{
      base::features::kPartitionAllocMemoryReclaimer};
  auto& instance = MemoryReclaimerSupport::Instance();
  instance.Start(task_environment_.GetMainThreadTaskRunner());
  task_environment_.FastForwardBy(
      MemoryReclaimerSupport::kFirstPAPurgeOrReclaimDelay);

  // Task gets reposted.
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());

  instance.SetForegrounded(false);
  task_environment_.FastForwardBy(MemoryReclaimerSupport::GetInterval());
  // But not once in background.
  EXPECT_FALSE(instance.has_pending_task_for_testing());
  EXPECT_EQ(0u, task_environment_.GetPendingMainThreadTaskCount());

  // Until we go to foreground again.
  instance.SetForegrounded(true);
  EXPECT_TRUE(instance.has_pending_task_for_testing());
  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
}

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS) && BUILDFLAG(IS_POSIX)
class SuperPagePlacementFor32BitTest : public ::testing::Test {
 protected:
  static constexpr uintptr_t kMiB = 1024 * 1024;
  static constexpr char kPrefix[] = "CobaltPartitionAllocSuperPageRegions_";

  void SetUp() override {
    partition_alloc::internal::ResetSuperPageRegionsForTesting();
    partition_alloc::SetRandomHintsDisabledFor32Bit(false);
  }
  void TearDown() override {
    partition_alloc::internal::ResetSuperPageRegionsForTesting();
    partition_alloc::SetRandomHintsDisabledFor32Bit(false);
  }

  static std::string P(const char* name) {
    return std::string(kPrefix) + name;
  }

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // A super-page-aligned hint outside every placement the tests configure.
  static constexpr uintptr_t kSeedNextSuperPage = 3500 * kMiB;

  static void SeedMallocRootNextSuperPage() {
    partition_alloc::PartitionRoot* root =
        allocator_shim::internal::PartitionAllocMalloc::Allocator();
    partition_alloc::internal::ScopedGuard guard(
        partition_alloc::internal::PartitionRootLock(root));
    root->next_super_page = kSeedNextSuperPage;
  }

  // The malloc root's chain hint after configuration: 0, or an address that
  // a super page reserved since then set.
  static uintptr_t MallocRootNextSuperPage() {
    partition_alloc::PartitionRoot* root =
        allocator_shim::internal::PartitionAllocMalloc::Allocator();
    partition_alloc::internal::ScopedGuard guard(
        partition_alloc::internal::PartitionRootLock(root));
    return root->next_super_page;
  }
#endif
};

TEST_F(SuperPagePlacementFor32BitTest, DisabledByDefault) {
  EXPECT_FALSE(ConfigureSuperPagePlacementFor32Bit());
  EXPECT_EQ(partition_alloc::GetSuperPageRegionStats().bottom_up_cursor, 0u);
  EXPECT_FALSE(partition_alloc::AreRandomHintsDisabledFor32Bit());
}

TEST_F(SuperPagePlacementFor32BitTest, RegionsModeUsesParams) {
  // ResetSuperPageRegionsForTesting() leaves a region mapped if the test
  // process's own allocations took a super page from it meanwhile.
  test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kCobaltPartitionAllocSuperPageRegions,
      {{P("mode"), "regions"},
       {P("initial_mb"), "4"},
       {P("step_mb"), "16"},
       {P("max_mb"), "32"}});
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  SeedMallocRootNextSuperPage();
#endif
  EXPECT_TRUE(ConfigureSuperPagePlacementFor32Bit());
  const partition_alloc::SuperPageRegionStats stats =
      partition_alloc::GetSuperPageRegionStats();
  EXPECT_GE(stats.regions, 1u);
  // The exact-size try may keep the aligned part, one super page less.
  EXPECT_GE(stats.reserved_bytes, 2 * kMiB);
  EXPECT_LE(stats.reserved_bytes, 4 * kMiB);
  // Regions mode leaves random hints on for other mappings (direct maps,
  // V8).
  EXPECT_FALSE(partition_alloc::AreRandomHintsDisabledFor32Bit());
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // The root restarted its chain.
  const uintptr_t next = MallocRootNextSuperPage();
  EXPECT_NE(next, kSeedNextSuperPage);
  EXPECT_TRUE(next == 0 || stats.used_bytes > 0 ||
              stats.fallback_super_pages > 0)
      << next;
#endif
}

// Bottom-up mode is implemented on these platforms only.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(SuperPagePlacementFor32BitTest, BottomUpModeUsesBaseAndLimit) {
  // Configuring maps nothing; super pages that the test process's own
  // allocations need from here on go to the band (MAP_FIXED_NOREPLACE never
  // replaces a mapping), which can move the cursor up.
  test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kCobaltPartitionAllocSuperPageRegions,
      {{P("mode"), "bottom-up"}, {P("base_mb"), "16"}, {P("limit_mb"), "3000"}});
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  SeedMallocRootNextSuperPage();
#endif
  EXPECT_TRUE(ConfigureSuperPagePlacementFor32Bit());
  const partition_alloc::SuperPageRegionStats stats =
      partition_alloc::GetSuperPageRegionStats();
  EXPECT_GE(stats.bottom_up_cursor, 16 * kMiB);
  EXPECT_LE(stats.bottom_up_cursor, 3000 * kMiB);
  EXPECT_FALSE(partition_alloc::internal::SuperPageRegionShouldDropHint(
      16 * kMiB - partition_alloc::internal::kSuperPageSize));
  EXPECT_TRUE(partition_alloc::internal::SuperPageRegionShouldDropHint(
      16 * kMiB));
  // Above 2 GiB.
  EXPECT_TRUE(partition_alloc::internal::SuperPageRegionShouldDropHint(
      3000 * kMiB));
  // b1 is on by default in bottom-up mode.
  EXPECT_TRUE(partition_alloc::AreRandomHintsDisabledFor32Bit());
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // The root restarted its chain.
  const uintptr_t next = MallocRootNextSuperPage();
  EXPECT_NE(next, kSeedNextSuperPage);
  EXPECT_TRUE(next == 0 || (next >= 16 * kMiB && next <= 3000 * kMiB) ||
              stats.fallback_super_pages > 0)
      << next;
#endif
}

TEST_F(SuperPagePlacementFor32BitTest, BottomUpModeCanKeepRandomHints) {
  test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kCobaltPartitionAllocSuperPageRegions,
      {{P("mode"), "bottom-up"},
       {P("base_mb"), "16"},
       {P("limit_mb"), "64"},
       {P("no_random_hint"), "false"}});
  EXPECT_TRUE(ConfigureSuperPagePlacementFor32Bit());
  EXPECT_FALSE(partition_alloc::AreRandomHintsDisabledFor32Bit());
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

TEST_F(SuperPagePlacementFor32BitTest, BottomUpModeRejectsEmptyRange) {
  test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kCobaltPartitionAllocSuperPageRegions,
      {{P("mode"), "bottom-up"}, {P("base_mb"), "100"}, {P("limit_mb"), "50"}});
  EXPECT_FALSE(ConfigureSuperPagePlacementFor32Bit());
  EXPECT_EQ(partition_alloc::GetSuperPageRegionStats().bottom_up_cursor, 0u);
  // A rejected configuration leaves the hints alone.
  EXPECT_FALSE(partition_alloc::AreRandomHintsDisabledFor32Bit());
}

TEST_F(SuperPagePlacementFor32BitTest, UnprefixedParamsAreIgnored) {
  // Cobalt's Finch loader merges all features' params into one map, so only
  // the prefixed names count.
  test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kCobaltPartitionAllocSuperPageRegions,
      {{"mode", "bottom-up"}, {"base_mb", "16"}, {"limit_mb", "64"}});
  EXPECT_TRUE(ConfigureSuperPagePlacementFor32Bit());
  // Default mode (regions), not bottom-up.
  EXPECT_EQ(partition_alloc::GetSuperPageRegionStats().bottom_up_cursor, 0u);
}
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS) && BUILDFLAG(IS_POSIX)

}  // namespace base::allocator
