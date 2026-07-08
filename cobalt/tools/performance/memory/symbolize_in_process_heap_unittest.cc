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

TEST(SymbolizeInProcessHeapTest, RunSymbolizerPipeline) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // 1. Create a mock Trace JSON File
  base::FilePath trace_path = temp_dir.GetPath().AppendASCII("trace.json");
  std::string trace_content = R"({
    "traceEvents": [
      {
        "name": "periodic_interval",
        "args": {
          "dumps": "{\"heaps_v2\":{\"maps\":{\"strings\":[{\"string\":\"pc:7f801000\"},{\"string\":\"pc:7f802000\"},{\"string\":\"pc:7F801000\"},{\"string\":\"pc:7f890000\"},{\"string\":\"normal_string\"}]}}}"
        }
      }
    ],
    "metadata": "mapping: \"mf\":\"libchrobalt.so\",\"pf\":5,\"sa\":\"7f800000\",\"sz\":\"20000\""
  })";
  ASSERT_TRUE(base::WriteFile(trace_path, trace_content));

  // 2. Create an empty mock library file
  base::FilePath lib_path = temp_dir.GetPath().AppendASCII("libchrobalt.so");
  ASSERT_TRUE(base::WriteFile(lib_path, ""));

  // 3. Create a mock llvm-symbolizer executable python script
  base::FilePath symbolizer_path =
      temp_dir.GetPath().AppendASCII("mock_symbolizer.py");
  std::string symbolizer_content = R"(#!/usr/bin/env python3
import sys
for line in sys.stdin:
  arg = line.strip()
  if arg == "0x1000":
    print("my_func_1")
    print("/home/user/cobalt/cobalt/dom/document.cc:42")
    print("")
  elif arg == "0x2000":
    print("??")
    print("??:0")
    print("")
)";
  ASSERT_TRUE(base::WriteFile(symbolizer_path, symbolizer_content));
  ASSERT_TRUE(base::SetPosixFilePermissions(
      symbolizer_path, base::FILE_PERMISSION_READ_BY_USER |
                           base::FILE_PERMISSION_WRITE_BY_USER |
                           base::FILE_PERMISSION_EXECUTE_BY_USER));

  // 4. Resolve the path of symbolize_in_process_heap.py
  base::FilePath source_root;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));
  base::FilePath script_path = source_root.AppendASCII("cobalt")
                                   .AppendASCII("tools")
                                   .AppendASCII("performance")
                                   .AppendASCII("memory")
                                   .AppendASCII("symbolize_in_process_heap.py");

  // 5. Execute symbolize_in_process_heap.py using Command Line
  base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("python3")));
  cmd.AppendArgPath(script_path);
  cmd.AppendArgPath(trace_path);
  cmd.AppendArg("-l");
  cmd.AppendArgPath(lib_path);
  cmd.AppendArg("-s");
  cmd.AppendArgPath(symbolizer_path);

  std::string output;
  int exit_code = -1;
  bool success = base::GetAppOutputWithExitCode(cmd, &output, &exit_code);

  EXPECT_TRUE(success) << "Failed to run python symbolization script.";
  EXPECT_EQ(0, exit_code) << "Python script failed with output:\n" << output;

  // 6. Read back trace and verify contents
  std::string result_content;
  ASSERT_TRUE(base::ReadFileToString(trace_path, &result_content));

  auto parsed = base::JSONReader::ReadAndReturnValueWithError(result_content);
  ASSERT_TRUE(parsed.has_value()) << parsed.error().message;
  ASSERT_TRUE(parsed->is_dict());

  const base::Value::Dict& dict = parsed->GetDict();
  const base::Value::List* events = dict.FindList("traceEvents");
  ASSERT_TRUE(events != nullptr);
  ASSERT_EQ(events->size(), 1u);

  const base::Value::Dict& event = (*events)[0].GetDict();
  const base::Value::Dict* args = event.FindDict("args");
  ASSERT_TRUE(args != nullptr);

  const std::string* dumps_str = args->FindString("dumps");
  ASSERT_TRUE(dumps_str != nullptr);

  auto dumps_parsed = base::JSONReader::ReadAndReturnValueWithError(*dumps_str);
  ASSERT_TRUE(dumps_parsed.has_value()) << dumps_parsed.error().message;
  ASSERT_TRUE(dumps_parsed->is_dict());
  const base::Value::Dict& dumps = dumps_parsed->GetDict();

  const base::Value::Dict* heaps_v2 = dumps.FindDict("heaps_v2");
  ASSERT_TRUE(heaps_v2 != nullptr);

  const base::Value::Dict* maps = heaps_v2->FindDict("maps");
  ASSERT_TRUE(maps != nullptr);

  const base::Value::List* strings = maps->FindList("strings");
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

}  // namespace
}  // namespace cobalt

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
