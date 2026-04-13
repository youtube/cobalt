// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/render_process_host_watcher.h"
#include "content/shell/browser/shell.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation_features.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"
#include "base/files/file_util.h"
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

  bool OnSmapsBuffer(std::string_view buffer) override {
    // Just categorize everything as "Test" to satisfy the test requirement
    // that detailed_stats_kb is not empty.
    for (const auto& line : base::SplitStringPiece(
             buffer, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      if (line.starts_with("Pss:")) {
        uint64_t pss_kb = 0;
        if (base::StringToUint64(
                base::SplitStringPiece(line, " ", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_NONEMPTY)[1],
                &pss_kb)) {
          metrics_.categories_kb["Test"] += pss_kb;
          metrics_.total_pss_kb += pss_kb;
        }
      }
    }
    return true;
  }

  memory_instrumentation::DetailedMetrics GetAndResetStats() override {
    memory_instrumentation::DetailedMetrics result = std::move(metrics_);
    metrics_ = memory_instrumentation::DetailedMetrics();
    return result;
  }

 private:
  memory_instrumentation::DetailedMetrics metrics_;
};

}  // namespace

class GranularMemoryBrowsertest : public ContentBrowserTest {
 public:
  GranularMemoryBrowsertest() {
    feature_list_.InitAndEnableFeature(
        memory_instrumentation::features::kGranularMemoryThrottling);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    memory_instrumentation::OSMetrics::SetDetailedMetricsDelegate(&delegate_);

    CreateTempFileWithContents(kTestSmaps, &temp_smaps_);
    CreateTempFileWithContents(kTestSmapsRollup, &temp_rollup_);
    memory_instrumentation::OSMetrics::SetProcSmapsForTesting(temp_smaps_.get());
    memory_instrumentation::OSMetrics::SetSmapsRollupForTesting(
        temp_rollup_.get());
  }

  void TearDownOnMainThread() override {
    memory_instrumentation::OSMetrics::SetProcSmapsForTesting(nullptr);
    memory_instrumentation::OSMetrics::SetSmapsRollupForTesting(nullptr);
    memory_instrumentation::OSMetrics::SetDetailedMetricsDelegate(nullptr);
    ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestDetailedMetricsDelegate delegate_;
  base::ScopedFILE temp_smaps_;
  base::ScopedFILE temp_rollup_;
};

#if BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(GranularMemoryBrowsertest, DetailedDump) {
  // Ensure at least one renderer process is active.
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), GURL("about:blank")));

  RenderProcessHost* rph =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHostWatcher watcher(
      rph, RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY);
  watcher.Wait();

  base::RunLoop run_loop;
  base::ProcessId renderer_pid = rph->GetProcess().Pid();
  ASSERT_NE(renderer_pid, base::kNullProcessId);

  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(
          base::trace_event::MemoryDumpType::kExplicitlyTriggered,
          base::trace_event::MemoryDumpLevelOfDetail::kDetailed,
          base::trace_event::MemoryDumpDeterminism::kNone, {},
          base::BindOnce(
              [](base::OnceClosure quit_closure,
                 base::ProcessId renderer_pid,
                 bool success,
                 std::unique_ptr<memory_instrumentation::GlobalMemoryDump>
                     dump) {
                EXPECT_TRUE(success);
                ASSERT_TRUE(dump);
                bool found_renderer_detailed_stats = false;
                for (const auto& process_dump : dump->process_dumps()) {
                  if (process_dump.pid() == renderer_pid) {
                    if (!process_dump.os_dump().detailed_stats_kb->empty()) {
                      found_renderer_detailed_stats = true;
                      // Verify the content matches our TestDetailedMetricsDelegate
                      // which should have accumulated 162 KB from kTestSmaps.
                      EXPECT_EQ(process_dump.os_dump().detailed_stats_kb->at("Test"), 162u);
                    }
                    break;
                  }
                }
                EXPECT_TRUE(found_renderer_detailed_stats);
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), renderer_pid));
  run_loop.Run();
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace content
