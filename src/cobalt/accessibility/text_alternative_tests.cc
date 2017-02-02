/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "cobalt/accessibility/text_alternative.h"
#include "cobalt/test/document_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace accessibility {
namespace {
struct TestInfo {
  TestInfo(const std::string& html_file_name,
           const std::string& expected_result)
      : html_file_name(html_file_name), expected_result(expected_result) {}
  std::string html_file_name;
  std::string expected_result;
};

// Enumerate the *.html files in the accessibility_test directory and build
// a list of input .html files and expected results.
std::vector<TestInfo> EnumerateTests() {
  std::vector<TestInfo> infos;
  FilePath root_directory;
  PathService::Get(base::DIR_SOURCE_ROOT, &root_directory);
  root_directory = root_directory.Append("cobalt").Append("accessibility_test");
  file_util::FileEnumerator enumerator(root_directory, false,
                                       file_util::FileEnumerator::FILES);
  for (FilePath html_file = enumerator.Next(); !html_file.empty();
       html_file = enumerator.Next()) {
    if (html_file.Extension() == ".html") {
      FilePath expected_results_file = html_file.ReplaceExtension(".expected");
      std::string results;
      if (!file_util::ReadFileToString(expected_results_file, &results)) {
        DLOG(WARNING) << "Failed to read results from file: "
                      << expected_results_file.value();
        continue;
      }
      TrimWhitespaceASCII(results, TRIM_ALL, &results);
      infos.push_back(TestInfo(html_file.BaseName().value(), results));
    }
  }
  return infos;
}

class TextAlternativeTest : public ::testing::TestWithParam<TestInfo> {
 protected:
  test::DocumentLoader document_loader_;
};
}  // namespace

TEST_P(TextAlternativeTest, TextAlternativeTest) {
  GURL url(std::string("file:///cobalt/accessibility_test/" +
                       GetParam().html_file_name));
  document_loader_.Load(url);
  ASSERT_TRUE(document_loader_.document());
  scoped_refptr<dom::Element> element =
      document_loader_.document()->GetElementById("element_to_test");
  ASSERT_TRUE(element);
  EXPECT_EQ(GetParam().expected_result, ComputeTextAlternative(element));
}
INSTANTIATE_TEST_CASE_P(TextAlternativeTest, TextAlternativeTest,
                        ::testing::ValuesIn(EnumerateTests()));
}  // namespace accessibility
}  // namespace cobalt
