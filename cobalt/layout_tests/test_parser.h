// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LAYOUT_TESTS_TEST_PARSER_H_
#define COBALT_LAYOUT_TESTS_TEST_PARSER_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/optional.h"
#include "cobalt/math/size.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace layout_tests {

// Final parsed information about an individual Layout Test entry.
struct TestInfo {
  TestInfo(const FilePath& base_file_path, const GURL& url,
           const base::optional<math::Size>& viewport_size)
      : base_file_path(base_file_path),
        url(url),
        viewport_size(viewport_size) {}

  // The base_file_path gives a path (relative to the root layout_tests
  // directory) to the test base filename from which all related files (such
  // as the source HTML file and the expected output PNG file) can be
  // derived.  This essentially acts as the test's identifier.
  FilePath base_file_path;

  // The URL is what the layout tests will load and render to produce the
  // actual output.  It is commonly computed from base_file_path (via
  // GetURLFromBaseFilePath()), but it can also be stated explicitly in the case
  // that it is external from the layout_tests.
  GURL url;

  // The viewport_size is the size of the viewport used during the test.  It
  // may not be specified, in which case it is intended that a default viewport
  // size be used.
  base::optional<math::Size> viewport_size;
};

// Define operator<< so that this test parameter can be printed by gtest if
// a test fails.
std::ostream& operator<<(std::ostream& out, const TestInfo& test_info);

// Load the file "layout_tests.txt" within the top-level directory and parse it.
// The file should contain a list of file paths (one per line) relative to the
// top level directory which the file lives.  These paths are returned as a
// vector of file paths relative to the input directory.
// The top_level parameter allows one to make different test case
// instantiations for different types of layout tests.  For example there may
// be an instantiation for Cobalt-specific layout tests, and another for
// CSS test suite tests.
std::vector<TestInfo> EnumerateLayoutTests(const std::string& top_level);

}  // namespace layout_tests
}  // namespace cobalt

#endif  // COBALT_LAYOUT_TESTS_TEST_PARSER_H_
