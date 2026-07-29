// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cobalt/browser/metrics/cobalt_detailed_metrics_delegate.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kTestSmaps[] =
    "00400000-004be000 r-xp 00000000 fc:01 1234              /file/1\n"
    "Size:                760 kB\n"
    "Rss:                 296 kB\n"
    "Pss:                 162 kB\n"
    "Shared_Clean:        228 kB\n"
    "Shared_Dirty:          0 kB\n"
    "Private_Clean:         0 kB\n"
    "Private_Dirty:        68 kB\n"
    "Referenced:          296 kB\n"
    "Anonymous:            68 kB\n"
    "AnonHugePages:         0 kB\n"
    "Swap:                  4 kB\n"
    "KernelPageSize:        4 kB\n"
    "MMUPageSize:           4 kB\n"
    "Locked:                0 kB\n"
    "VmFlags: rd ex mr mw me dw sd\n";

const char kTestSmapsRollup[] =
    "Pss:                 162 kB\n"
    "SwapPss:               0 kB\n";

void CreateTempFileWithContents(const char* contents, base::ScopedFILE* file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath temp_path;
  *file = base::CreateAndOpenTemporaryStream(&temp_path);
  ASSERT_TRUE(*file);
  ASSERT_TRUE(base::WriteFileDescriptor(fileno(file->get()), contents));
}

class TestDetailedMetricsDelegate
    : public memory_instrumentation::DetailedMetricsDelegate {
 public:
  TestDetailedMetricsDelegate() = default;
  ~TestDetailedMetricsDelegate() override = default;

  void OnSmapsEntry(
      absl::string_view name,
      const memory_instrumentation::SmapsMetrics& metrics) override {
    stats_["Test"] += metrics.pss_kb;
    total_pss_kb_ += metrics.pss_kb;
  }

  void GetAndResetStats(base::flat_map<std::string, uint64_t>* stats) override {
    *stats = std::move(stats_);
    (*stats)["total_pss"] = total_pss_kb_;
    stats_.clear();
    total_pss_kb_ = 0;
  }

  base::WeakPtr<memory_instrumentation::DetailedMetricsDelegate> GetWeakPtr()
      override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::flat_map<std::string, uint64_t> stats_;
  uint64_t total_pss_kb_ = 0;
  base::WeakPtrFactory<TestDetailedMetricsDelegate> weak_ptr_factory_{this};
};

}  // namespace

class GranularMemoryBrowsertest : public ContentBrowserTest {
 public:
  GranularMemoryBrowsertest() = default;

  void SetUpOnMainThread() override {
    BrowserTestBase::SetUpOnMainThread();
    memory_instrumentation::MemoryInstrumentation::GetInstance()
        ->SetDetailedMetricsDelegate(&delegate_);

    CreateTempFileWithContents(kTestSmaps, &temp_smaps_);
    CreateTempFileWithContents(kTestSmapsRollup, &temp_rollup_);
    memory_instrumentation::OSMetrics::SetProcSmapsForTesting(
        temp_smaps_.get());
    memory_instrumentation::OSMetrics::SetSmapsRollupForTesting(
        temp_rollup_.get());
  }

  void TearDownOnMainThread() override {
    memory_instrumentation::OSMetrics::SetProcSmapsForTesting(nullptr);
    memory_instrumentation::OSMetrics::SetSmapsRollupForTesting(nullptr);
    memory_instrumentation::MemoryInstrumentation::GetInstance()
        ->SetDetailedMetricsDelegate(nullptr);
    BrowserTestBase::TearDownOnMainThread();
  }

 private:
  TestDetailedMetricsDelegate delegate_;
  base::ScopedFILE temp_smaps_;
  base::ScopedFILE temp_rollup_;
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GranularMemoryBrowsertest, DetailedDump) {
  // Ensure at least one renderer process is active.
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  base::RunLoop run_loop;
  base::ProcessId browser_pid = base::GetCurrentProcId();

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->GetCoordinator()
      ->RequestGlobalMemoryDump(
          base::trace_event::MemoryDumpType::kExplicitlyTriggered,
          base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
          base::trace_event::MemoryDumpDeterminism::kNone, {},
          base::BindOnce(
              [](base::OnceClosure quit_closure, base::ProcessId browser_pid,
                 bool success,
                 memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
                EXPECT_TRUE(success);
                ASSERT_TRUE(dump);
                bool found_browser_detailed_stats = false;
                for (const auto& process_dump : dump->process_dumps) {
                  if (process_dump->pid == browser_pid) {
                    ASSERT_TRUE(process_dump->os_dump->detailed_stats_kb);
                    if (!process_dump->os_dump->detailed_stats_kb->empty()) {
                      found_browser_detailed_stats = true;
                      // Verify the content matches our
                      // TestDetailedMetricsDelegate which should have
                      // accumulated 162 KB from kTestSmaps.
                      EXPECT_EQ(
                          process_dump->os_dump->detailed_stats_kb->at("Test"),
                          162u);
                    }
                    break;
                  }
                }
                EXPECT_TRUE(found_browser_detailed_stats);
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), browser_pid));
  run_loop.Run();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GranularMemoryBrowsertest,
                       BackgroundDumpSkipsDetailedStats) {
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  base::RunLoop run_loop;
  base::ProcessId browser_pid = base::GetCurrentProcId();

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->GetCoordinator()
      ->RequestGlobalMemoryDump(
          base::trace_event::MemoryDumpType::kExplicitlyTriggered,
          base::trace_event::MemoryDumpLevelOfDetail::kBackground,
          base::trace_event::MemoryDumpDeterminism::kNone, {},
          base::BindOnce(
              [](base::OnceClosure quit_closure, base::ProcessId browser_pid,
                 bool success,
                 memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
                EXPECT_TRUE(success);
                ASSERT_TRUE(dump);
                for (const auto& process_dump : dump->process_dumps) {
                  if (process_dump->pid == browser_pid) {
                    if (process_dump->os_dump->detailed_stats_kb) {
                      EXPECT_TRUE(
                          process_dump->os_dump->detailed_stats_kb->find(
                              "Test") ==
                          process_dump->os_dump->detailed_stats_kb->end());
                    }
                    break;
                  }
                }
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), browser_pid));
  run_loop.Run();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GranularMemoryBrowsertest, CobaltSpecificMetrics) {
  // Ensure at least one renderer process is active.
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();

  base::RunLoop run_loop;
  base::ProcessId renderer_pid = rph->GetProcess().Pid();
  ASSERT_NE(renderer_pid, base::kNullProcessId);

  // Instantiate the production Cobalt delegate.
  cobalt::CobaltDetailedMetricsDelegate cobalt_delegate;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->SetDetailedMetricsDelegate(&cobalt_delegate);

  // Create mock smaps data that triggers Cobalt categorization.
  const char kCobaltSmaps[] =
      "00400000-004be000 r-xp 00000000 fc:01 1234              "
      "/path/to/libchrobalt.so\n"
      "Pss:                 100 kB\n"
      "Private_Dirty:         0 kB\n"
      "Private_Clean:         0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Swap:                  0 kB\n"
      "Locked:                0 kB\n"
      "00500000-005be000 r-xp 00000000 fc:01 1235              "
      "/path/to/libcobalt.so\n"
      "Pss:                  50 kB\n"
      "Private_Dirty:         0 kB\n"
      "Private_Clean:         0 kB\n"
      "Shared_Dirty:          0 kB\n"
      "Shared_Clean:          0 kB\n"
      "Swap:                  0 kB\n"
      "Locked:                0 kB\n";

  base::ScopedFILE temp_smaps;
  CreateTempFileWithContents(kCobaltSmaps, &temp_smaps);
  memory_instrumentation::OSMetrics::SetProcSmapsForTesting(temp_smaps.get());

  const char kCobaltRollup[] =
      "Pss:                 150 kB\n"
      "SwapPss:               0 kB\n";
  base::ScopedFILE temp_rollup;
  CreateTempFileWithContents(kCobaltRollup, &temp_rollup);
  memory_instrumentation::OSMetrics::SetSmapsRollupForTesting(
      temp_rollup.get());

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->GetCoordinator()
      ->RequestGlobalMemoryDump(
          base::trace_event::MemoryDumpType::kExplicitlyTriggered,
          base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
          base::trace_event::MemoryDumpDeterminism::kNone, {},
          base::BindOnce(
              [](base::OnceClosure quit_closure, base::ProcessId renderer_pid,
                 base::ScopedFILE file, bool success,
                 memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
                EXPECT_TRUE(success);
                ASSERT_TRUE(dump);
                bool found_renderer_detailed_stats = false;
                for (const auto& process_dump : dump->process_dumps) {
                  if (process_dump->pid == renderer_pid) {
                    ASSERT_TRUE(process_dump->os_dump->detailed_stats_kb);
                    if (!process_dump->os_dump->detailed_stats_kb->empty()) {
                      found_renderer_detailed_stats = true;
                      // Verify Cobalt categories are present.
                      EXPECT_EQ(process_dump->os_dump->detailed_stats_kb->at(
                                    "pss:lib_chrobalt"),
                                150u);
                    }
                    break;
                  }
                }
                EXPECT_TRUE(found_renderer_detailed_stats);
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), renderer_pid, std::move(temp_smaps)));
  run_loop.Run();

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->SetDetailedMetricsDelegate(nullptr);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

}  // namespace content
