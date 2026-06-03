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

#include "third_party/blink/renderer/core/cobalt/html_video_element/html_video_element_extensions.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#if !BUILDFLAG(USE_STARBOARD_MEDIA)
#error "This file only works with Starboard media"
#endif  // !BUILDFLAG(USE_STARBOARD_MEDIA)

namespace blink {

TEST(HTMLVideoElementExtensionsTest, canGetAndSetMaxVideoCapabilities) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      std::make_unique<DummyPageHolder>();
  V8TestingScope dummy_exception_state;
  HTMLVideoElement* video =
      MakeGarbageCollected<HTMLVideoElement>(dummy_page_holder->GetDocument());
  ASSERT_EQ(video->GetMaxVideoCapabilities(), "");
  HTMLVideoElementExtensions::setMaxVideoCapabilities(
      *video, String("testString"), dummy_exception_state.GetExceptionState());
  ASSERT_EQ(video->GetMaxVideoCapabilities(), "testString");
}

}  // namespace blink
