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

#include "cobalt/renderer/cobalt_render_frame_observer.h"

#include "starboard/extension/graphics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockGraphicsExtension {
 public:
  MOCK_METHOD(void, ReportFullyDrawn, ());
};

MockGraphicsExtension* g_mock_graphics_extension = nullptr;

void MockReportFullyDrawn() {
  g_mock_graphics_extension->ReportFullyDrawn();
}

const CobaltExtensionGraphicsApi kMockGraphicsApi = {
    kCobaltExtensionGraphicsName,
    6,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &MockReportFullyDrawn,
};

}  // namespace

namespace cobalt {
namespace renderer {

class CobaltRenderFrameObserverTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_mock_graphics_extension = &mock_graphics_extension_;
    // Passing nullptr for RenderFrame is acceptable here because the
    // DidMeaningfulLayout method doesn't use the render_frame member.
    observer_ = std::make_unique<CobaltRenderFrameObserver>(nullptr);
  }

  void TearDown() override { g_mock_graphics_extension = nullptr; }

  MockGraphicsExtension mock_graphics_extension_;
  std::unique_ptr<CobaltRenderFrameObserver> observer_;
};

TEST_F(CobaltRenderFrameObserverTest,
       CallsReportFullyDrawnOnVisuallyNonEmptyLayout) {
  EXPECT_CALL(mock_graphics_extension_, ReportFullyDrawn()).Times(1);
  observer_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kVisuallyNonEmpty);
}

TEST_F(CobaltRenderFrameObserverTest,
       DoesNotCallReportFullyDrawnOnOtherLayouts) {
  EXPECT_CALL(mock_graphics_extension_, ReportFullyDrawn()).Times(0);
  observer_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kFinishedParsing);
  observer_->DidMeaningfulLayout(blink::WebMeaningfulLayout::kFinishedLoading);
}

}  // namespace renderer
}  // namespace cobalt

const void* SbSystemGetExtension(const char* name) {
  if (strcmp(name, kCobaltExtensionGraphicsName) == 0) {
    return &kMockGraphicsApi;
  }
  return nullptr;
}
