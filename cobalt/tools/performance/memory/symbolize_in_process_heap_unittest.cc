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
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

base::FilePath GetSymbolizerScriptPath() {
  base::FilePath source_root;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));
  return source_root.AppendASCII("cobalt")
      .AppendASCII("tools")
      .AppendASCII("performance")
      .AppendASCII("memory")
      .AppendASCII("symbolize_in_process_heap.py");
}

void CreateMockSymbolizer(const base::FilePath& symbolizer_path) {
  const std::string symbolizer_content = R"(#!/usr/bin/env python3
import sys
for line in sys.stdin:
  arg = line.strip()
  if arg == "0x1000" or arg == "0x0":
    print("my_func_1")
    print("/home/user/cobalt/cobalt/dom/document.cc:42")
    print("")
  elif arg == "0x2000" or arg == "0x1000":
    print("??")
    print("??:0")
    print("")
  else:
    print("generic_symbol")
    print("base/memory/ref_counted.cc:10")
    print("")
)";
  ASSERT_TRUE(base::WriteFile(symbolizer_path, symbolizer_content));
  ASSERT_TRUE(base::SetPosixFilePermissions(
      symbolizer_path, base::FILE_PERMISSION_READ_BY_USER |
                           base::FILE_PERMISSION_WRITE_BY_USER |
                           base::FILE_PERMISSION_EXECUTE_BY_USER));
}

// Test 1: Android TV / Desktop Linux named mapped file in process_mmaps
TEST(SymbolizeInProcessHeapTest, NamedMappingAndroidTV) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path = temp_dir.GetPath().AppendASCII("trace.json");
  const std::string trace_content = R"({
    "traceEvents": [
      {
        "name": "periodic_interval",
        "args": {
          "dumps": {
            "heaps_v2": {
              "maps": {
                "strings": [
                  {"string": "pc:7f801000"},
                  {"string": "pc:7f802000"},
                  {"string": "pc:7f801000"},
                  {"string": "pc:7f890000"},
                  {"string": "normal_string"}
                ]
              }
            },
            "process_mmaps": {
              "vm_regions": [
                {
                  "sa": "7f800000",
                  "sz": "20000",
                  "pf": 5,
                  "mf": "/data/app/dev.cobalt/lib/arm64/libcobalt.so"
                }
              ]
            }
          }
        }
      }
    ]
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath lib_path = temp_dir.GetPath().AppendASCII("libcobalt.so");
  ASSERT_TRUE(base::WriteFile(lib_path, ""));

  base::FilePath symbolizer_path =
      temp_dir.GetPath().AppendASCII("mock_symbolizer.py");
  CreateMockSymbolizer(symbolizer_path);

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(GetSymbolizerScriptPath());
  cmd.AppendArgPath(trace_path);
  cmd.AppendArg("-l");
  cmd.AppendArgPath(lib_path);
  cmd.AppendArg("-s");
  cmd.AppendArgPath(symbolizer_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success) << "Failed to run python symbolization script.";
  EXPECT_EQ(0, exit_code) << "Script output:\n" << output;

  std::string result_content;
  ASSERT_TRUE(base::ReadFileToString(trace_path, &result_content));

  auto parsed = base::JSONReader::ReadAndReturnValueWithError(result_content);
  ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
  ASSERT_TRUE(parsed->is_dict());

  const base::Value::List* events = parsed->GetDict().FindList("traceEvents");
  ASSERT_TRUE(events != nullptr);
  ASSERT_EQ(events->size(), 1u);

  const base::Value::Dict* dumps =
      (*events)[0].GetDict().FindDictByDottedPath("args.dumps");
  ASSERT_TRUE(dumps != nullptr);

  const base::Value::List* strings =
      dumps->FindListByDottedPath("heaps_v2.maps.strings");
  ASSERT_TRUE(strings != nullptr);
  ASSERT_EQ(strings->size(), 5u);

  EXPECT_EQ(*((*strings)[0].GetDict().FindString("string")),
            "my_func_1 (cobalt/dom/document.cc:42)");
  EXPECT_EQ(*((*strings)[1].GetDict().FindString("string")),
            "Unresolved [offset: 0x2000]");
  EXPECT_EQ(*((*strings)[2].GetDict().FindString("string")),
            "my_func_1 (cobalt/dom/document.cc:42)");
  EXPECT_EQ(*((*strings)[3].GetDict().FindString("string")), "pc:7f890000");
  EXPECT_EQ(*((*strings)[4].GetDict().FindString("string")), "normal_string");
}

// Test 2: RDK Evergreen anonymous memory correlation (mf: "")
TEST(SymbolizeInProcessHeapTest, EvergreenAnonymousMappingRDK) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path = temp_dir.GetPath().AppendASCII("rdk_trace.json");
  const std::string trace_content = R"({
    "traceEvents": [
      {
        "name": "periodic_interval",
        "args": {
          "dumps": {
            "heaps_v2": {
              "maps": {
                "strings": [
                  {"string": "pc:e5bb8000"},
                  {"string": "pc:e5bb9000"},
                  {"string": "normal_unaffected_string"}
                ]
              }
            },
            "process_mmaps": {
              "vm_regions": [
                {
                  "sa": "e5bb8000",
                  "sz": "5000000",
                  "pf": 5,
                  "mf": ""
                }
              ]
            }
          }
        }
      }
    ]
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath lib_path = temp_dir.GetPath().AppendASCII("libcobalt.so");
  ASSERT_TRUE(base::WriteFile(lib_path, ""));

  base::FilePath symbolizer_path =
      temp_dir.GetPath().AppendASCII("mock_symbolizer.py");
  CreateMockSymbolizer(symbolizer_path);

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(GetSymbolizerScriptPath());
  cmd.AppendArgPath(trace_path);
  cmd.AppendArg("-l");
  cmd.AppendArgPath(lib_path);
  cmd.AppendArg("-s");
  cmd.AppendArgPath(symbolizer_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success) << "Failed to run python symbolization script.";
  EXPECT_EQ(0, exit_code) << "Script output:\n" << output;

  std::string result_content;
  ASSERT_TRUE(base::ReadFileToString(trace_path, &result_content));

  auto parsed = base::JSONReader::ReadAndReturnValueWithError(result_content);
  ASSERT_TRUE(parsed.has_value()) << parsed.error().message;

  const base::Value::List* strings = parsed->GetDict().FindListByDottedPath(
      "traceEvents[0].args.dumps.heaps_v2.maps.strings");
  ASSERT_TRUE(strings != nullptr);
  ASSERT_EQ(strings->size(), 3u);

  EXPECT_EQ(*((*strings)[0].GetDict().FindString("string")),
            "my_func_1 (cobalt/dom/document.cc:42)");
  EXPECT_EQ(*((*strings)[2].GetDict().FindString("string")),
            "normal_unaffected_string");
}

// Test 3: Already symbolized or empty trace exits with code 0
TEST(SymbolizeInProcessHeapTest, AlreadySymbolizedTrace) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("resolved_trace.json");
  const std::string trace_content =
      "{\"traceEvents\": [{\"name\": \"periodic_interval\", \"args\": "
      "{\"dumps\": {\"heaps_v2\": {\"maps\": {\"strings\": ["
      "{\"string\": \"malloc (base/allocator/allocator.cc:10)\"},"
      "{\"string\": \"v8::internal::Heap::Allocate()\"}"
      "]}}}}}]}";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath lib_path = temp_dir.GetPath().AppendASCII("libcobalt.so");
  ASSERT_TRUE(base::WriteFile(lib_path, ""));

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(GetSymbolizerScriptPath());
  cmd.AppendArgPath(trace_path);
  cmd.AppendArg("-l");
  cmd.AppendArgPath(lib_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success);
  EXPECT_EQ(0, exit_code);
  EXPECT_NE(output.find("No unresolved raw program counters"),
            std::string::npos);
}

// Test 4: Multi-segment library mappings and serialized JSON string dumps
TEST(SymbolizeInProcessHeapTest, MultiSegmentAndSerializedStringDumps) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("multiseg_trace.json");
  const std::string trace_content = R"({
    "traceEvents": [
      {
        "name": "periodic_interval",
        "args": {
          "dumps": "{\"heaps_v2\":{\"maps\":{\"strings\":[{\"string\":\"pc:e1397000\"},{\"string\":\"pc:e505b000\"}]}},\"process_mmaps\":{\"vm_regions\":[{\"sa\":\"e505a000\",\"sz\":\"1e1000\",\"mf\":\"/data/app/libchrobalt.so\"},{\"sa\":\"e1396000\",\"sz\":\"3cc4000\",\"mf\":\"/data/app/libchrobalt.so\"}]}}"
        }
      }
    ]
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath lib_path = temp_dir.GetPath().AppendASCII("libchrobalt.so");
  ASSERT_TRUE(base::WriteFile(lib_path, ""));

  base::FilePath symbolizer_path =
      temp_dir.GetPath().AppendASCII("mock_symbolizer.py");
  CreateMockSymbolizer(symbolizer_path);

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(GetSymbolizerScriptPath());
  cmd.AppendArgPath(trace_path);
  cmd.AppendArg("-l");
  cmd.AppendArgPath(lib_path);
  cmd.AppendArg("-s");
  cmd.AppendArgPath(symbolizer_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success);
  EXPECT_EQ(0, exit_code) << "Script output:\n" << output;

  std::string result_content;
  ASSERT_TRUE(base::ReadFileToString(trace_path, &result_content));
  auto parsed = base::JSONReader::ReadAndReturnValueWithError(result_content);
  ASSERT_TRUE(parsed.has_value()) << parsed.error().message;

  const base::Value::List* strings = parsed->GetDict().FindListByDottedPath(
      "traceEvents[0].args.dumps.heaps_v2.maps.strings");
  ASSERT_TRUE(strings != nullptr);
  ASSERT_EQ(strings->size(), 2u);
  EXPECT_EQ(*((*strings)[0].GetDict().FindString("string")),
            "my_func_1 (cobalt/dom/document.cc:42)");
}

// Test 5: Custom output path (-o) and machine-readable summary export
TEST(SymbolizeInProcessHeapTest, CustomOutputPathAndSummaryExport) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath trace_path =
      temp_dir.GetPath().AppendASCII("input_trace.json");
  base::FilePath out_trace_path =
      temp_dir.GetPath().AppendASCII("symbolized_trace.json");
  base::FilePath summary_path = temp_dir.GetPath().AppendASCII("summary.json");

  const std::string trace_content = R"({
    "traceEvents": [
      {
        "name": "periodic_interval",
        "args": {
          "dumps": {
            "heaps_v2": {
              "maps": {
                "strings": [
                  {"string": "pc:7f801000"},
                  {"string": "normal_string"}
                ]
              }
            },
            "process_mmaps": {
              "vm_regions": [
                {
                  "sa": "7f800000",
                  "sz": "20000",
                  "pf": 5,
                  "mf": "/data/app/libcobalt.so"
                }
              ]
            }
          }
        }
      }
    ]
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  base::FilePath lib_path = temp_dir.GetPath().AppendASCII("libcobalt.so");
  ASSERT_TRUE(base::WriteFile(lib_path, ""));

  base::FilePath symbolizer_path =
      temp_dir.GetPath().AppendASCII("mock_symbolizer.py");
  CreateMockSymbolizer(symbolizer_path);

  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(GetSymbolizerScriptPath());
  cmd.AppendArgPath(trace_path);
  cmd.AppendArg("-l");
  cmd.AppendArgPath(lib_path);
  cmd.AppendArg("-o");
  cmd.AppendArgPath(out_trace_path);
  cmd.AppendArg("-s");
  cmd.AppendArgPath(symbolizer_path);
  cmd.AppendArg("--export_summary_json");
  cmd.AppendArgPath(summary_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success);
  EXPECT_EQ(0, exit_code) << "Script output:\n" << output;

  // Verify input trace was preserved (not mutated in-place)
  std::string in_content;
  ASSERT_TRUE(base::ReadFileToString(trace_path, &in_content));
  EXPECT_NE(in_content.find("pc:7f801000"), std::string::npos);

  // Verify output trace exists and is symbolized
  std::string out_content;
  ASSERT_TRUE(base::ReadFileToString(out_trace_path, &out_content));
  EXPECT_NE(out_content.find("my_func_1"), std::string::npos);

  // Verify summary JSON exists and contains expected metrics
  std::string summary_content;
  ASSERT_TRUE(base::ReadFileToString(summary_path, &summary_content));
  auto parsed_summary =
      base::JSONReader::ReadAndReturnValueWithError(summary_content);
  ASSERT_TRUE(parsed_summary.has_value());
  EXPECT_EQ(parsed_summary->GetDict().FindInt("total_snapshots"), 1);
  EXPECT_EQ(parsed_summary->GetDict().FindInt("symbolized_pcs"), 2);
}

}  // namespace
}  // namespace cobalt

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
