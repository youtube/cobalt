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

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/fake_localized_strings.h"
#include "media/base/media.h"
#include "media/base/media_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "starboard/client_porting/wrap_main/wrap_main.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(USE_STARBOARD_MEDIA)
#include "media/starboard/decoder_buffer_allocator.h"
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)

class TestSuiteNoAtExit : public base::TestSuite {
 public:
  TestSuiteNoAtExit(int argc, char** argv) : TestSuite(argc, argv) {}
  ~TestSuiteNoAtExit() override = default;

 protected:
  void Initialize() override;

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  // Defining starboard decoder buffer allocator makes DecoderBuffer use it.
  media::DecoderBufferAllocator decoder_buffer_allocator_;
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)
};

void TestSuiteNoAtExit::Initialize() {
  // Run TestSuite::Initialize first so that logging is initialized.
  base::TestSuite::Initialize();

#if BUILDFLAG(IS_ANDROID)
  media::MediaCodecBridgeImpl::SetupCallbackHandlerForTesting();
#endif

  // Run this here instead of main() to ensure an AtExitManager is already
  // present.
  media::InitializeMediaLibrary();
  media::SetUpFakeLocalizedStrings();

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
}

static int InitAndRunAllTests(int argc, char** argv) {
  mojo::core::Init();
  TestSuiteNoAtExit test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&TestSuiteNoAtExit::Run, base::Unretained(&test_suite)));
}

SB_EXPORT STARBOARD_WRAP_SIMPLE_MAIN(InitAndRunAllTests)

#if !SB_IS(EVERGREEN)
int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
#endif
