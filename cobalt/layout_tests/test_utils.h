/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef LAYOUT_TESTS_TEST_UTILS_H_
#define LAYOUT_TESTS_TEST_UTILS_H_

#include "base/file_path.h"

namespace cobalt {
namespace layout_tests {

// Returns the root directory that all test input can be found in (e.g.
// the HTML files that define the tests, and the PNG/TXT files that define
// the expected output).
FilePath GetTestInputRootDirectory();

// Returns the root directory that all output will be placed within.  Output
// is generated when rebaselining test expected output, or when test details
// have been chosen to be output.
FilePath GetTestOutputRootDirectory();

}  // namespace layout_tests
}  // namespace cobalt

#endif  // LAYOUT_TESTS_TEST_UTILS_H_
