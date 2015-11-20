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

#include "cobalt/layout_tests/test_utils.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace layout_tests {

namespace {

// Returns the relative path to Cobalt layout tests.  This can be appended
// to either base::DIR_SOURCE_ROOT to get the input directory or
// base::DIR_COBALT_TEST_OUT to get the output directory.
FilePath GetDirSourceRootRelativePath() {
  return FilePath(FILE_PATH_LITERAL("cobalt"))
      .Append(FILE_PATH_LITERAL("layout_tests"));
}

}  // namespace

FilePath GetTestInputRootDirectory() {
  FilePath dir_source_root;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root));
  return dir_source_root.Append(GetDirSourceRootRelativePath());
}

FilePath GetTestOutputRootDirectory() {
  FilePath dir_cobalt_test_out;
  PathService::Get(cobalt::paths::DIR_COBALT_TEST_OUT, &dir_cobalt_test_out);
  return dir_cobalt_test_out.Append(GetDirSourceRootRelativePath());
}

}  // namespace layout_tests
}  // namespace cobalt
