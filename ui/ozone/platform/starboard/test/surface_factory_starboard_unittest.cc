// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "ui/ozone/platform/starboard/surface_factory_starboard.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {
static const std::map<gl::GLImplementation, std::string>
    kGLImplementationToString = {
        {gl::kGLImplementationNone, "none"},
        {gl::kGLImplementationEGLGLES2, "eglgles2"},
        {gl::kGLImplementationMockGL, "mockgl"},
        {gl::kGLImplementationStubGL, "stubgl"},
        {gl::kGLImplementationDisabled, "disabled"},
        {gl::kGLImplementationEGLANGLE, "eglangle"},
};

class SurfaceFactoryStarboardSupportTest
    : public testing::TestWithParam<
          testing::tuple<gl::GLImplementation, bool>> {
 public:
  SurfaceFactoryStarboardSupportTest() : surface_factory_() {}

 protected:
  SurfaceFactoryStarboard* surface_factory() { return &surface_factory_; }

 private:
  SurfaceFactoryStarboard surface_factory_;
};

TEST_P(SurfaceFactoryStarboardSupportTest, GetGLOzoneSupports) {
  gl::GLImplementation gl_implementation_type = testing::get<0>(GetParam());
  bool is_supported = testing::get<1>(GetParam());
  auto gl_implementation = surface_factory()->GetGLOzone(
      gl::GLImplementationParts(gl_implementation_type));

  EXPECT_EQ(!!gl_implementation, is_supported);
}

// Params: gl_implementation_type, is_supported
INSTANTIATE_TEST_SUITE_P(
    ImplementationTypes,
    SurfaceFactoryStarboardSupportTest,
    testing::Values(std::make_tuple(gl::kGLImplementationNone, false),
                    std::make_tuple(gl::kGLImplementationEGLGLES2, false),
                    std::make_tuple(gl::kGLImplementationMockGL, false),
                    std::make_tuple(gl::kGLImplementationStubGL, false),
                    std::make_tuple(gl::kGLImplementationDisabled, false),
                    std::make_tuple(gl::kGLImplementationEGLANGLE, true)),
    [](const testing::TestParamInfo<
        SurfaceFactoryStarboardSupportTest::ParamType>& info) {
      return kGLImplementationToString.at(testing::get<0>(info.param));
    });
}  // namespace
}  // namespace ui
