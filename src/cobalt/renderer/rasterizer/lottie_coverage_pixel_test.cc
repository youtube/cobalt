// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "cobalt/render_tree/lottie_node.h"
#include "cobalt/renderer/rasterizer/pixel_test_fixture.h"

using cobalt::math::RectF;
using cobalt::render_tree::LottieAnimation;
using cobalt::render_tree::LottieNode;

namespace cobalt {
namespace renderer {
namespace rasterizer {

namespace {

struct GetLottieTestName {
  std::string operator()(
      const ::testing::TestParamInfo<base::FilePath>& filepath) const {
    std::string name = filepath.param.BaseName().value();
    for (size_t i = 0; i < name.size(); ++i) {
      char ch = name[i];
      if (ch >= 'A' && ch <= 'Z') {
        continue;
      }
      if (ch >= 'a' && ch <= 'z') {
        continue;
      }
      if (ch >= '0' && ch <= '9') {
        continue;
      }
      name[i] = '_';
    }
    return name;
  }
};

std::vector<base::FilePath> EnumerateLottieTestData() {
  base::FilePath data_directory;
  CHECK(base::PathService::Get(base::DIR_TEST_DATA, &data_directory));
  base::FilePath lottie_dir = data_directory.Append(FILE_PATH_LITERAL("cobalt"))
                                  .Append(FILE_PATH_LITERAL("renderer"))
                                  .Append(FILE_PATH_LITERAL("rasterizer"))
                                  .Append(FILE_PATH_LITERAL("testdata"))
                                  .Append(FILE_PATH_LITERAL("lottie_coverage"));
  base::FileEnumerator lottie_enum(lottie_dir, false,
                                   base::FileEnumerator::FILES);
  std::vector<base::FilePath> lottie_files;
  for (base::FilePath name = lottie_enum.Next(); !name.empty();
       name = lottie_enum.Next()) {
    // FileEnumerator patterns are not supported in Starboard so we check here
    // if the file is Lottie JSON
    if (strcmp(name.FinalExtension().c_str(), ".json") == 0) {
      lottie_files.push_back(name);
    }
  }
  return lottie_files;
}

}  // namespace

// Blitter does not support Skottie.
#if !SB_HAS(BLITTER)

class LottiePixelTest : public PixelTest,
                        public testing::WithParamInterface<base::FilePath> {};

TEST_P(LottiePixelTest, Run) {
  std::vector<uint8> animation_data = GetFileData(GetParam());
  scoped_refptr<LottieAnimation> animation =
      GetResourceProvider()->CreateLottieAnimation(
          reinterpret_cast<char*>(&animation_data[0]), animation_data.size());
  LottieAnimation::LottieProperties lottie_properties;
  lottie_properties.UpdateState(LottieAnimation::LottieState::kPlaying);
  lottie_properties.UpdateLoop(true);
  animation->BeginRenderFrame(lottie_properties);

  LottieNode::Builder node_builder =
      LottieNode::Builder(animation, RectF(output_surface_size()));

  // Seek animation to arbitrary timestamp, confirm that rendering looks okay
  node_builder.animation_time = base::TimeDelta::FromSecondsD(1);
  scoped_refptr<LottieNode> lottie_node = new LottieNode(node_builder);

  // Account for the fact that lottie coverage testdata is in a separate folder
  base::FilePath lottie_base_filename;
  TestTree(lottie_node,
           lottie_base_filename.Append(FILE_PATH_LITERAL("lottie_coverage"))
               .Append(GetParam().BaseName().RemoveExtension()));
}

INSTANTIATE_TEST_CASE_P(LottieCoveragePixelTest, LottiePixelTest,
                        ::testing::ValuesIn(EnumerateLottieTestData()),
                        GetLottieTestName());

#endif  // !SB_HAS(BLITTER)

}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
