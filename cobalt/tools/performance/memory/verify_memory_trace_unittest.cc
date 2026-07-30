// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "build/build_config.h"

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)) && !BUILDFLAG(IS_STARBOARD)

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

base::FilePath GetMemoryToolsDir() {
  base::FilePath source_root;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));
  return source_root.AppendASCII("cobalt")
      .AppendASCII("tools")
      .AppendASCII("performance")
      .AppendASCII("memory");
}

TEST(VerifyMemoryTraceTest, CleanTraceProceeds) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("clean_trace.json");
  std::string trace_content = R"({
    "traceEvents": [
      {
        "cat": "__metadata",
        "name": "process_name",
        "args": {"name": "Cobalt"}
      },
      {
        "cat": "disabled-by-default-memory-infra",
        "name": "periodic_interval",
        "args": {
          "dumps": {
            "heaps_v2": {"allocators": {"malloc": {}}},
            "process_mmaps": {"vm_regions": []},
            "process_totals": {
              "private_footprint_bytes": "5025000",
              "peak_resident_set_size": "a5b6000"
            }
          }
        }
      }
    ]
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath script_path =
      GetMemoryToolsDir().AppendASCII("verify_memory_trace.py");

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(script_path);
  cmd.AppendArgPath(trace_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success) << "Failed to run verify_memory_trace.py.";
  EXPECT_EQ(0, exit_code) << "Script failed with output:\n" << output;
  EXPECT_NE(output.find("[SIGNAL: PROCEED]"), std::string::npos)
      << "Expected [SIGNAL: PROCEED] in output:\n"
      << output;
  EXPECT_NE(output.find("Private: 80.1 MB"), std::string::npos)
      << "Expected parsed Private Footprint in scorecard:\n"
      << output;
}

TEST(VerifyMemoryTraceTest, NoisyTraceWarns) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("noisy_trace.json");
  std::string trace_content = R"({
    "traceEvents": [
      {
        "cat": "toplevel,blink",
        "name": "TaskQueue::RunTask"
      },
      {
        "cat": "disabled-by-default-memory-infra",
        "name": "periodic_interval",
        "args": {
          "dumps": {
            "heaps_v2": {"allocators": {}},
            "process_mmaps": {"vm_regions": []}
          }
        }
      }
    ]
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath script_path =
      GetMemoryToolsDir().AppendASCII("verify_memory_trace.py");

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(script_path);
  cmd.AppendArgPath(trace_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success) << "Failed to run verify_memory_trace.py.";
  EXPECT_EQ(0, exit_code) << "Script failed with output:\n" << output;
  EXPECT_NE(output.find("[SIGNAL: PROCEED_WITH_WARNING]"), std::string::npos)
      << "Expected [SIGNAL: PROCEED_WITH_WARNING] in output:\n"
      << output;
  EXPECT_NE(output.find("🔴 NOISY (blink, toplevel)"), std::string::npos)
      << "Expected noise categories in scorecard:\n"
      << output;
}

TEST(VerifyMemoryTraceTest, IncompleteTraceAborts) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("incomplete_trace.json");
  std::string trace_content = R"({
    "traceEvents": [
      {
        "cat": "disabled-by-default-memory-infra",
        "name": "periodic_interval",
        "args": {
          "dumps": {
            "process_mmaps": {"vm_regions": []}
          }
        }
      }
    ]
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath script_path =
      GetMemoryToolsDir().AppendASCII("verify_memory_trace.py");

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(script_path);
  cmd.AppendArgPath(trace_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success) << "Failed to run verify_memory_trace.py.";
  EXPECT_EQ(1, exit_code) << "Expected exit code 1 for incomplete trace.";
  EXPECT_NE(output.find("[SIGNAL: ABORT]"), std::string::npos)
      << "Expected [SIGNAL: ABORT] in output:\n"
      << output;
}

TEST(VerifyMemoryTraceTest, MissingProcessMmapsAborts) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("missing_mmaps.json");
  std::string trace_content = R"({
    "traceEvents": [
      {
        "cat": "disabled-by-default-memory-infra",
        "name": "periodic_interval",
        "args": {
          "dumps": {
            "heaps_v2": {"allocators": {}}
          }
        }
      }
    ]
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath script_path =
      GetMemoryToolsDir().AppendASCII("verify_memory_trace.py");

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(script_path);
  cmd.AppendArgPath(trace_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success);
  EXPECT_EQ(1, exit_code);
  EXPECT_NE(output.find("[SIGNAL: ABORT]"), std::string::npos);
}

TEST(VerifyMemoryTraceTest, CorruptedJsonAborts) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("corrupted_trace.json");
  std::string trace_content = "{ this is invalid json ::: ";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath script_path =
      GetMemoryToolsDir().AppendASCII("verify_memory_trace.py");

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(script_path);
  cmd.AppendArgPath(trace_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success);
  EXPECT_EQ(1, exit_code);
  EXPECT_NE(output.find("[SIGNAL: ABORT]"), std::string::npos);
}

TEST(VerifyMemoryTraceTest, NonExistentFileAborts) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("non_existent_file.json");

  base::FilePath script_path =
      GetMemoryToolsDir().AppendASCII("verify_memory_trace.py");

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(script_path);
  cmd.AppendArgPath(trace_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success);
  EXPECT_EQ(1, exit_code);
  EXPECT_NE(output.find("[SIGNAL: ABORT]"), std::string::npos);
}

}  // namespace
}  // namespace cobalt

#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)) &&
        // !BUILDFLAG(IS_STARBOARD)
