// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/crash_handler/crash_analyzer.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/common/lightweight_detector.h"
#include "components/gwp_asan/crash_handler/crash.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/snapshot/annotation_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_architecture.h"
#include "third_party/crashpad/crashpad/snapshot/cpu_context.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_exception_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_module_snapshot.h"
#include "third_party/crashpad/crashpad/snapshot/test/test_process_snapshot.h"
#include "third_party/crashpad/crashpad/test/process_type.h"
#include "third_party/crashpad/crashpad/util/process/process_memory_native.h"

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "third_party/crashpad/crashpad/test/linux/fake_ptrace_connection.h"
#endif

namespace gwp_asan {
namespace internal {

namespace {

void SetNonCanonicalAccessAddress(
    crashpad::test::TestExceptionSnapshot& exception,
    crashpad::VMAddress address) {
#if defined(ARCH_CPU_X86_64)
  auto* context = exception.MutableContext();
  context->architecture = crashpad::kCPUArchitectureX86_64;
  memset(context->x86_64, 0, sizeof(*context->x86_64));
#endif  // defined(ARCH_CPU_X86_64)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  exception.SetException(SIGSEGV);
#if defined(ARCH_CPU_X86_64)
  exception.SetExceptionInfo(SI_KERNEL);
  exception.SetExceptionAddress(0);
  context->x86_64->rax = address;
#else   // defined(ARCH_CPU_X86_64)
  exception.SetExceptionAddress(address);
#endif  // defined(ARCH_CPU_X86_64)

#elif BUILDFLAG(IS_APPLE)
  exception.SetException(EXC_BAD_ACCESS);
#if defined(ARCH_CPU_X86_64)
  exception.SetExceptionInfo(EXC_I386_GPFLT);
  exception.SetExceptionAddress(0);
  context->x86_64->rax = address;
#else   // defined(ARCH_CPU_X86_64)
  exception.SetExceptionAddress(address);
#endif  // defined(ARCH_CPU_X86_64)

#elif BUILDFLAG(IS_WIN)
  exception.SetException(EXCEPTION_ACCESS_VIOLATION);
#if defined(ARCH_CPU_X86_64)
  exception.SetCodes({0, std::numeric_limits<uint64_t>::max()});
  context->x86_64->rax = address;
#else   // defined(ARCH_CPU_X86_64)
  exception.SetCodes({0, address});
#endif  // defined(ARCH_CPU_X86_64)
#else
#error "Unknown platform"
#endif
}

constexpr const char* kMallocHistogramName =
    "Security.GwpAsan.CrashAnalysisResult.Malloc";
constexpr const char* kPartitionAllocHistogramName =
    "Security.GwpAsan.CrashAnalysisResult.PartitionAlloc";

}  // namespace

class BaseCrashAnalyzerTest : public testing::Test {
 protected:
  using testing::Test::SetUp;

  void SetUp(bool is_partition_alloc,
             LightweightDetector::State lightweight_detector_state,
             size_t num_lightweight_detector_metadata) {
    is_partition_alloc_ = is_partition_alloc;
    gpa_.Init(1, 1, 1, base::DoNothing(), is_partition_alloc,
              lightweight_detector_state, num_lightweight_detector_metadata);
  }

  // Initializes the ProcessSnapshot so that it appears the given allocator was
  // used for backing either malloc or PartitionAlloc, depending on
  // `is_partition_alloc_`.
  void InitializeSnapshot(crashpad::VMAddress exception_address) {
    std::string crash_key_value = gpa_.GetCrashKey();
    std::vector<uint8_t> crash_key_vector(crash_key_value.begin(),
                                          crash_key_value.end());

    std::vector<crashpad::AnnotationSnapshot> annotations;
    annotations.emplace_back(
        is_partition_alloc_ ? kPartitionAllocCrashKey : kMallocCrashKey,
        static_cast<uint16_t>(crashpad::Annotation::Type::kString),
        crash_key_vector);

    auto module = std::make_unique<crashpad::test::TestModuleSnapshot>();
    module->SetAnnotationObjects(annotations);

    auto exception = std::make_unique<crashpad::test::TestExceptionSnapshot>();
    // Just match the bitness, the actual architecture doesn't matter.
#if defined(ARCH_CPU_64_BITS)
    exception->MutableContext()->architecture =
        crashpad::CPUArchitecture::kCPUArchitectureX86_64;
#else
    exception->MutableContext()->architecture =
        crashpad::CPUArchitecture::kCPUArchitectureX86;
#endif
    SetNonCanonicalAccessAddress(*exception, exception_address);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    ASSERT_TRUE(connection_.Initialize(getpid()));
    auto memory = std::make_unique<crashpad::ProcessMemoryLinux>(&connection_);
#else
    auto memory = std::make_unique<crashpad::ProcessMemoryNative>();
    ASSERT_TRUE(memory->Initialize(crashpad::test::GetSelfProcess()));
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

    process_snapshot_.AddModule(std::move(module));
    process_snapshot_.SetException(std::move(exception));
    process_snapshot_.SetProcessMemory(std::move(memory));
  }

  GuardedPageAllocator gpa_;
  crashpad::test::TestProcessSnapshot process_snapshot_;
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  crashpad::test::FakePtraceConnection connection_;
#endif

  bool is_partition_alloc_ = false;
};

class CrashAnalyzerTest : public BaseCrashAnalyzerTest {
  void SetUp() final {
    BaseCrashAnalyzerTest::SetUp(/* is_partition_alloc = */ false,
                                 LightweightDetector::State::kDisabled, 0);
    InitializeSnapshot(0);
  }
};

// Stack trace collection on Android builds with frame pointers enabled does
// not use base::debug::StackTrace, so the stack traces may vary slightly and
// break this test.
#if !BUILDFLAG(IS_ANDROID) || !BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
TEST_F(CrashAnalyzerTest, StackTraceCollection) {
  void* ptr = gpa_.Allocate(10);
  ASSERT_NE(ptr, nullptr);
  gpa_.Deallocate(ptr);

  // Lets pretend a double free() occurred on the allocation we saw previously.
  gpa_.state_.double_free_address = reinterpret_cast<uintptr_t>(ptr);

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  bool proto_present =
      CrashAnalyzer::GetExceptionInfo(process_snapshot_, &proto);
  ASSERT_TRUE(proto_present);

  int result = static_cast<int>(GwpAsanCrashAnalysisResult::kGwpAsanCrash);
  EXPECT_THAT(histogram_tester.GetAllSamples(kMallocHistogramName),
              testing::ElementsAre(base::Bucket(result, 1)));
  histogram_tester.ExpectTotalCount(kPartitionAllocHistogramName, 0);

  ASSERT_TRUE(proto.has_allocation());
  ASSERT_TRUE(proto.has_deallocation());

  base::debug::StackTrace st;
  size_t trace_len;
  const void* const* trace = st.Addresses(&trace_len);
  ASSERT_NE(trace, nullptr);
  ASSERT_GT(trace_len, 0U);

  // Adjust the stack trace to point to the entry above the current frame.
  while (trace_len > 0) {
    if (trace[0] == __builtin_return_address(0))
      break;

    trace++;
    trace_len--;
  }

  ASSERT_GT(proto.allocation().stack_trace_size(), (int)trace_len);
  ASSERT_GT(proto.deallocation().stack_trace_size(), (int)trace_len);

  // Ensure that the allocation and deallocation stack traces match the stack
  // frames that we collected above the current frame.
  for (size_t i = 1; i <= trace_len; i++) {
    SCOPED_TRACE(i);
    ASSERT_EQ(proto.allocation().stack_trace(
                  proto.allocation().stack_trace_size() - i),
              reinterpret_cast<uintptr_t>(trace[trace_len - i]));
    ASSERT_EQ(proto.deallocation().stack_trace(
                  proto.deallocation().stack_trace_size() - i),
              reinterpret_cast<uintptr_t>(trace[trace_len - i]));
  }
}
#endif

TEST_F(CrashAnalyzerTest, InternalError) {
  // Lets pretend an invalid free() occurred in the allocator region.
  gpa_.state_.free_invalid_address =
      reinterpret_cast<uintptr_t>(gpa_.state_.first_page_addr);
  // Out of bounds slot_to_metadata_idx, allocator was initialized with only a
  // single entry slot/metadata entry.
  gpa_.slot_to_metadata_idx_[0] = 5;

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  bool proto_present =
      CrashAnalyzer::GetExceptionInfo(process_snapshot_, &proto);
  ASSERT_TRUE(proto_present);

  int result =
      static_cast<int>(GwpAsanCrashAnalysisResult::kErrorBadMetadataIndex);
  EXPECT_THAT(histogram_tester.GetAllSamples(kMallocHistogramName),
              testing::ElementsAre(base::Bucket(result, 1)));
  histogram_tester.ExpectTotalCount(kPartitionAllocHistogramName, 0);

  EXPECT_TRUE(proto.has_internal_error());
  ASSERT_TRUE(proto.has_missing_metadata());
  EXPECT_TRUE(proto.missing_metadata());
}

// The detector is not used on 32-bit systems because pointers there aren't big
// enough to safely store metadata IDs.
#if defined(ARCH_CPU_64_BITS)
class LightweightDetectorAnalyzerTest : public BaseCrashAnalyzerTest {
  void SetUp() final {
    BaseCrashAnalyzerTest::SetUp(/* is_partition_alloc = */ true,
                                 LightweightDetector::State::kEnabled, 1);
  }
};

TEST_F(LightweightDetectorAnalyzerTest, UseAfterFree) {
  uint64_t alloc;
  gpa_.RecordLightweightDeallocation(&alloc, sizeof(alloc));
  InitializeSnapshot(alloc);

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  bool proto_present =
      CrashAnalyzer::GetExceptionInfo(process_snapshot_, &proto);
  ASSERT_TRUE(proto_present);

  int result =
      static_cast<int>(GwpAsanCrashAnalysisResult::kLightweightDetectorCrash);
  EXPECT_THAT(histogram_tester.GetAllSamples(kPartitionAllocHistogramName),
              testing::ElementsAre(base::Bucket(result, 1)));
  histogram_tester.ExpectTotalCount(kMallocHistogramName, 0);

  ASSERT_FALSE(proto.has_allocation());
  ASSERT_TRUE(proto.has_deallocation());
  EXPECT_FALSE(proto.has_internal_error());
  ASSERT_TRUE(proto.has_missing_metadata());
  EXPECT_FALSE(proto.missing_metadata());
}

TEST_F(LightweightDetectorAnalyzerTest, InternalError) {
  uint64_t alloc;
  gpa_.RecordLightweightDeallocation(&alloc, sizeof(alloc));
  InitializeSnapshot(alloc);

  // Corrupt the metadata ID.
  ++gpa_.lightweight_detector_metadata_[0].lightweight_id;

  base::HistogramTester histogram_tester;
  gwp_asan::Crash proto;
  bool proto_present =
      CrashAnalyzer::GetExceptionInfo(process_snapshot_, &proto);
  ASSERT_TRUE(proto_present);

  int result =
      static_cast<int>(GwpAsanCrashAnalysisResult::
                           kErrorInvalidOrOutdatedLightweightMetadataIndex);
  EXPECT_THAT(histogram_tester.GetAllSamples(kPartitionAllocHistogramName),
              testing::ElementsAre(base::Bucket(result, 1)));
  histogram_tester.ExpectTotalCount(kMallocHistogramName, 0);

  ASSERT_FALSE(proto.has_allocation());
  ASSERT_FALSE(proto.has_deallocation());
  EXPECT_TRUE(proto.has_internal_error());
  ASSERT_TRUE(proto.has_missing_metadata());
  EXPECT_TRUE(proto.missing_metadata());
}
#endif  // defined(ARCH_CPU_64_BITS)

}  // namespace internal
}  // namespace gwp_asan
