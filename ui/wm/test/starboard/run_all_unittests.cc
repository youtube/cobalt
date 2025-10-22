// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

class WMTestSuite : public base::TestSuite {
 public:
  WMTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  WMTestSuite(const WMTestSuite&) = delete;
  WMTestSuite& operator=(const WMTestSuite&) = delete;

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();
    gl::GLSurfaceTestSupport::InitializeOneOff();
    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    if (ui::IsScaleFactorSupported(ui::k200Percent)) {
      base::FilePath ui_test_resources_200 = ui_test_pak_path.DirName().Append(
          FILE_PATH_LITERAL("ui_test_200_percent.pak"));
      ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
          ui_test_resources_200, ui::k200Percent);
    }

    env_ = aura::Env::CreateInstance();

    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
  }

  void Shutdown() override {
    env_.reset();
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

 private:
  std::unique_ptr<aura::Env> env_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

static int InitAndRunAllTests(int argc, char** argv) {
  WMTestSuite test_suite(argc, argv);

  mojo::core::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&WMTestSuite::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
