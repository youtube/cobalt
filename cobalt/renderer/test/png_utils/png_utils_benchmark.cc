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

#include "base/file_path.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/renderer/test/png_utils/png_decode.h"
#include "cobalt/trace_event/benchmark.h"

namespace {
FilePath GetBenchmarkImagePath() {
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_TEST_DATA, &data_directory));
  return data_directory.Append(FILE_PATH_LITERAL("test"))
                       .Append(FILE_PATH_LITERAL("png_utils"))
                       .Append(FILE_PATH_LITERAL("png_benchmark_image.png"));
}
}  // namespace

TRACE_EVENT_BENCHMARK2(
    DecodePNGToRGBABenchmark,
    "PNGFileReadContext::PNGFileReadContext()",
        cobalt::trace_event::IN_SCOPE_DURATION,
    "PNGFileReadContext::DecodeImageTo()",
        cobalt::trace_event::IN_SCOPE_DURATION) {
  const int kIterationCount = 20;
  for (int i = 0; i < kIterationCount; ++i) {
    int width;
    int height;
    scoped_array<uint8_t> pixel_data =
        cobalt::renderer::test::png_utils::DecodePNGToRGBA(
            GetBenchmarkImagePath(),
            &width, &height);
  }
}

TRACE_EVENT_BENCHMARK2(
    DecodePNGToPremultipliedAlphaRGBABenchmark,
    "PNGFileReadContext::PNGFileReadContext()",
        cobalt::trace_event::IN_SCOPE_DURATION,
    "PNGFileReadContext::DecodeImageTo()",
        cobalt::trace_event::IN_SCOPE_DURATION) {
  FilePath data_directory;
  CHECK(PathService::Get(base::DIR_TEST_DATA, &data_directory));

  const int kIterationCount = 20;
  for (int i = 0; i < kIterationCount; ++i) {
    int width;
    int height;
    scoped_array<uint8_t> pixel_data =
        cobalt::renderer::test::png_utils::DecodePNGToPremultipliedAlphaRGBA(
            GetBenchmarkImagePath(),
            &width, &height);
  }
}
