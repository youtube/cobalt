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

#include "cobalt/shell/browser/splash/splash_wasm_runner.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "cobalt/shell/browser/splash/splash_wasm_selector.h"
#include "cobalt/shell/browser/splash/testdata/test_selector_wasm.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(5);

// gin::V8Test initializes V8 the way the other V8 tests in this binary
// expect. RunSplashSelectorWasm() creates its own isolate on top.
class SplashWasmRunnerTest : public gin::V8Test {
 protected:
  base::expected<std::string, std::string> Run(
      const std::string& input_json,
      base::TimeDelta timeout = kTimeout) {
    return RunSplashSelectorWasm(kTestSelectorWasm, input_json, timeout);
  }

  // Returns the selected key, or the error.
  std::string Select(const std::string& input_json) {
    auto result = Run(input_json);
    return result.has_value() ? result.value() : "error: " + result.error();
  }
};

TEST_F(SplashWasmRunnerTest, SelectsByScreenHeight) {
  EXPECT_EQ("splash_1080.webm", Select(R"({"screen_height":1080})"));
  EXPECT_EQ("splash_2160.webm", Select(R"({"screen_height":2160})"));
}

TEST_F(SplashWasmRunnerTest, SelectsByTopic) {
  EXPECT_EQ("music.webm", Select(R"({"topic":"music","screen_height":2160})"));
}

TEST_F(SplashWasmRunnerTest, SelectsFromHostInput) {
  SplashSelectorInput input;
  input.topic = "music";
  input.screen_width = 1920;
  input.screen_height = 1080;
  EXPECT_EQ("music.webm", Select(SplashWasmSelector::BuildInputJson(
                              input, base::Time::Now())));
  input.topic = "";
  input.screen_width = 3840;
  input.screen_height = 2160;
  EXPECT_EQ("splash_2160.webm", Select(SplashWasmSelector::BuildInputJson(
                                    input, base::Time::Now())));
}

TEST_F(SplashWasmRunnerTest, RejectsInvalidModule) {
  const uint8_t kGarbage[] = {'n', 'o', 't', 'w', 'a', 's', 'm'};
  auto result = RunSplashSelectorWasm(kGarbage, "{}", kTimeout);
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(std::string::npos, result.error().find("compile"))
      << result.error();
}

TEST_F(SplashWasmRunnerTest, RejectsModuleWithoutAbi) {
  // An empty but valid module.
  const uint8_t kEmptyModule[] = {0x00, 0x61, 0x73, 0x6d,
                                  0x01, 0x00, 0x00, 0x00};
  auto result = RunSplashSelectorWasm(kEmptyModule, "{}", kTimeout);
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(std::string::npos, result.error().find("doesn't export"))
      << result.error();
}

TEST_F(SplashWasmRunnerTest, ReportsTrap) {
  auto result = Run(R"({"test_mode":"trap"})");
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(std::string::npos, result.error().find("splash_select() failed"))
      << result.error();
}

TEST_F(SplashWasmRunnerTest, RejectsOutOfBoundsOutput) {
  auto result = Run(R"({"test_mode":"bad_output"})");
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(std::string::npos, result.error().find("out-of-bounds"))
      << result.error();
}

TEST_F(SplashWasmRunnerTest, TerminatesOnTimeout) {
  const base::TimeTicks start = base::TimeTicks::Now();
  auto result = Run(R"({"test_mode":"loop"})", base::Milliseconds(50));
  const base::TimeDelta elapsed = base::TimeTicks::Now() - start;
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(std::string::npos, result.error().find("terminated"))
      << result.error();
  EXPECT_LT(elapsed, base::Seconds(5));
}

TEST_F(SplashWasmRunnerTest, RejectsOversizedInput) {
  auto result = Run(std::string(kMaxSplashSelectorInputSize + 1, ' '));
  ASSERT_FALSE(result.has_value());
}

TEST_F(SplashWasmRunnerTest, ReportsCost) {
  // Not a benchmark: logs the cost of a run (isolate creation, compilation,
  // instantiation, selection and teardown) on this machine.
  constexpr int kRuns = 20;
  const base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kRuns; ++i) {
    ASSERT_TRUE(Run(R"({"screen_height":1080})").has_value());
  }
  LOG(INFO) << "RunSplashSelectorWasm: "
            << (base::TimeTicks::Now() - start) / kRuns << " per run, "
            << sizeof(kTestSelectorWasm) << " byte module";
}

TEST(SplashWasmSelectorTest, BuildInputJson) {
  SplashSelectorInput input;
  input.topic = "music";
  input.locale = "en-US";
  input.screen_width = 1920;
  input.screen_height = 1080;
  std::optional<base::Value> value = base::JSONReader::Read(
      SplashWasmSelector::BuildInputJson(input, base::Time::UnixEpoch()));
  ASSERT_TRUE(value && value->is_dict());
  const base::Value::Dict& dict = value->GetDict();
  EXPECT_EQ(1, dict.FindInt("abi_version"));
  EXPECT_EQ(0.0, dict.FindDouble("now_ms"));
  EXPECT_EQ("music", *dict.FindString("topic"));
  EXPECT_EQ("en-US", *dict.FindString("locale"));
  EXPECT_EQ(1920, dict.FindInt("screen_width"));
  EXPECT_EQ(1080, dict.FindInt("screen_height"));
}

TEST(SplashWasmSelectorTest, ValidatesOutput) {
  EXPECT_TRUE(IsValidSplashSelectorOutput("splash_1080.webm"));
  EXPECT_TRUE(IsValidSplashSelectorOutput("ads/2026-11/a.webm"));
  EXPECT_FALSE(IsValidSplashSelectorOutput(""));
  EXPECT_FALSE(IsValidSplashSelectorOutput("a b.webm"));
  EXPECT_FALSE(IsValidSplashSelectorOutput("https://evil.com/x.webm"));
  EXPECT_FALSE(IsValidSplashSelectorOutput(
      std::string(kMaxSplashSelectorOutputSize + 1, 'a')));
}

}  // namespace
}  // namespace content
