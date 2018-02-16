// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/samples/simple_example/simple_example.h"

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace samples {

class SimpleExampleTest : public ::testing::Test {
 protected:
  SimpleExampleTest() : example_(1) {}

  SimpleExample example_;
};

TEST_F(SimpleExampleTest, AddNumbers) {
  EXPECT_EQ(4, example_.MultiplyAdd(2, 2));
  EXPECT_EQ(5, example_.MultiplyAdd(-1, 6));
  EXPECT_EQ(2, example_.MultiplyAdd(7, -5));
  EXPECT_EQ(-8, example_.MultiplyAdd(-2, -6));
}

TEST_F(SimpleExampleTest, MultiplyPositiveAndAddNumbers) {
  example_.set_multiplier(10);

  EXPECT_EQ(22, example_.MultiplyAdd(2, 2));
  EXPECT_EQ(-4, example_.MultiplyAdd(-1, 6));
  EXPECT_EQ(65, example_.MultiplyAdd(7, -5));
  EXPECT_EQ(-26, example_.MultiplyAdd(-2, -6));
}

TEST_F(SimpleExampleTest, MultiplyNegativeAndAddNumbers) {
  example_.set_multiplier(-5);

  EXPECT_EQ(-8, example_.MultiplyAdd(2, 2));
  EXPECT_EQ(11, example_.MultiplyAdd(-1, 6));
  EXPECT_EQ(-40, example_.MultiplyAdd(7, -5));
  EXPECT_EQ(4, example_.MultiplyAdd(-2, -6));
}

TEST_F(SimpleExampleTest, MultiplyZeroAndAddNumbers) {
  example_.set_multiplier(0);

  EXPECT_EQ(2, example_.MultiplyAdd(2, 2));
  EXPECT_EQ(6, example_.MultiplyAdd(-1, 6));
  EXPECT_EQ(-5, example_.MultiplyAdd(7, -5));
  EXPECT_EQ(-6, example_.MultiplyAdd(-2, -6));
}

TEST_F(SimpleExampleTest, MultiplyAndAddDataFromFile) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("cobalt"))
                 .Append(FILE_PATH_LITERAL("samples"))
                 .Append(FILE_PATH_LITERAL("testdata"))
                 .Append(FILE_PATH_LITERAL("test_cases"));
  ASSERT_TRUE(file_util::PathExists(data_dir));

  const char* kCaseFiles[] = {
      "case1.txt", "case2.txt", "case3.txt",
  };
  const int kNumInputArgs = 3;

  for (int i = 0; i < arraysize(kCaseFiles); ++i) {
    // Read test input.
    std::string test_input_string;
    ASSERT_TRUE(file_util::ReadFileToString(data_dir.AppendASCII(kCaseFiles[i]),
                                            &test_input_string));
    std::vector<std::string> test_input;
    base::SplitString(test_input_string, ' ', &test_input);
    ASSERT_EQ(kNumInputArgs, test_input.size());

    // Convert input arguments from strings to integers.
    int test_input_args[kNumInputArgs] = {0};
    for (int j = 0; j < kNumInputArgs; ++j) {
      ASSERT_TRUE(base::StringToInt(test_input[j], &test_input_args[j]));
    }

    // Read expected test result.
    std::string expected_string;
    ASSERT_TRUE(file_util::ReadFileToString(
        data_dir.AppendASCII(std::string(kCaseFiles[i]) + ".expected"),
        &expected_string));

    // Perform test.
    int expected_result;
    ASSERT_TRUE(base::StringToInt(expected_string, &expected_result));

    example_.set_multiplier(test_input_args[0]);
    const int result =
        example_.MultiplyAdd(test_input_args[1], test_input_args[2]);

    // Check the result.
    EXPECT_EQ(result, expected_result);
  }
}

TEST_F(SimpleExampleTest, PrintData) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("cobalt"))
                 .Append(FILE_PATH_LITERAL("samples"))
                 .Append(FILE_PATH_LITERAL("testdata"));
  ASSERT_TRUE(file_util::PathExists(data_dir));

  std::string data_string;
  EXPECT_TRUE(file_util::ReadFileToString(
      data_dir.Append(FILE_PATH_LITERAL("data.txt")), &data_string));

  EXPECT_LT(0, data_string.size());
}

}  // namespace samples
}  // namespace cobalt
