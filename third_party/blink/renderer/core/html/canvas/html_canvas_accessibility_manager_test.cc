// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/html_canvas_accessibility_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

class HTMLCanvasAccessibilityManagerTest : public PageTestBase {
 public:
  HTMLCanvasAccessibilityManagerTest() = default;

  void SetUpCanvas(const char* html_content) {
    GetDocument().GetSettings()->SetScriptEnabled(true);
    GetDocument().documentElement()->SetInnerHTMLWithoutTrustedTypes(
        html_content);
    canvas_element_ =
        To<HTMLCanvasElement>(GetDocument().getElementById(AtomicString("c")));
    UpdateAllLifecyclePhasesForTest();
  }

  void WaitForAccessibilityManagerUpdate() {
    HTMLCanvasAccessibilityManager* manager =
        canvas_element_->GetAccessibilityManagerForTesting();
    ASSERT_TRUE(manager);
    manager->FlushUmaIfNeeded();
  }

 protected:
  Persistent<HTMLCanvasElement> canvas_element_;
};

TEST_F(HTMLCanvasAccessibilityManagerTest, NoAccessibilityService) {
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");

  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  EXPECT_EQ(manager, nullptr);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, IsIgnored) {
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);

  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  EXPECT_EQ(manager, nullptr);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, AriaHiddenIsIgnored) {
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200 "
      "aria-hidden='true'></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);

  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  EXPECT_FALSE(manager);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, TooSmall) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=5 height=5></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kTooSmall, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasLayoutSubtree) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200 layoutsubtree></"
      "canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasLayoutSubtree, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasNonElementFallbackContent) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200>Comment</"
      "canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasFallbackContent) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200><button>Click</button></"
      "canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasFallbackContent, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasAriaRole) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200 role='img'></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasAriaAttributes, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, HasAriaLabel) {
  base::HistogramTester histogram_tester;
  SetUpCanvas(
      "<body><canvas id='c' width=300 height=200 "
      "aria-label='chart'></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasAriaAttributes, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, NeedsA11ySupport) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  histogram_tester.ExpectUniqueSample(
      "Accessibility.Canvas.HeuristicResult",
      HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport, 1);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, DynamicAriaAttributeAdded) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport);

  // Dynamically add an aria attribute.
  canvas_element_->setAttribute(html_names::kAriaLabelAttr,
                                AtomicString("chart"));
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(
      canvas_element_->GetAccessibilityManagerForTesting()
          ->GetHeuristicResultForTesting(),
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasAriaAttributes);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, IgnoredStateChanged) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport);

  // Simulate AXObject notifying the canvas that its ignored state changed.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kIsIgnored);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, DynamicFallbackContentAdded) {
  base::HistogramTester histogram_tester;
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport);

  // Dynamically add fallback element content.
  auto* button = GetDocument().CreateRawElement(html_names::kButtonTag);
  canvas_element_->AppendChild(button);
  UpdateAllLifecyclePhasesForTest();
  WaitForAccessibilityManagerUpdate();

  EXPECT_EQ(
      canvas_element_->GetAccessibilityManagerForTesting()
          ->GetHeuristicResultForTesting(),
      HTMLCanvasAccessibilityManager::HeuristicResult::kHasFallbackContent);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, InitiallyIgnoredBecomesVisible) {
  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/true);
  EXPECT_FALSE(canvas_element_->GetAccessibilityManagerForTesting());

  // Simulate AXObject notifying the canvas that it is no longer ignored.
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  EXPECT_TRUE(canvas_element_->GetAccessibilityManagerForTesting());
  EXPECT_EQ(canvas_element_->GetAccessibilityManagerForTesting()
                ->GetHeuristicResultForTesting(),
            HTMLCanvasAccessibilityManager::HeuristicResult::kNeedsA11ySupport);
}

TEST_F(HTMLCanvasAccessibilityManagerTest, RecordAndSortRenderedText) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(::features::kAccessibilityCanvas);

  SetUpCanvas("<body><canvas id='c' width=300 height=200></canvas></body>");
  canvas_element_->OnAxObjectIgnoredStateChanged(/*is_ignored=*/false);
  WaitForAccessibilityManagerUpdate();

  HTMLCanvasAccessibilityManager* manager =
      canvas_element_->GetAccessibilityManagerForTesting();
  ASSERT_TRUE(manager);
  EXPECT_TRUE(manager->ShouldCaptureRenderedText());

  // Record text runs out of order.
  // "World" is lower (larger Y) so it should come second.
  manager->RecordRenderedText("World", gfx::RectF(0, 50, 100, 20), 12.0f);
  // "Hello" is higher (smaller Y) so it should come first.
  manager->RecordRenderedText("Hello", gfx::RectF(0, 0, 100, 20), 12.0f);
  manager->UpdateAnnotation();
  EXPECT_EQ(manager->CanvasAnnotation(), "Hello World");

  // Record another run on the same line as "World" to test horizontal sorting.
  // "Again" is to the right of "World" (bounds.x() = 110).
  manager->RecordRenderedText("Again", gfx::RectF(110, 50, 100, 20), 12.0f);
  manager->UpdateAnnotation();
  EXPECT_EQ(manager->CanvasAnnotation(), "Hello World Again");

  // Test overwrite: recording text in a sufficiently overlapping area should
  // overwrite the existing run instead of adding a new one.
  manager->RecordRenderedText("Hi", gfx::RectF(0, 0, 100, 20), 12.0f);
  manager->UpdateAnnotation();
  EXPECT_EQ(manager->CanvasAnnotation(), "Hi World Again");

  // Test clearing a specific region.
  // This rect intersects mainly with the "World" run (at Y=50).
  manager->ClearRenderedText(gfx::RectF(-10, 40, 120, 30));
  manager->UpdateAnnotation();
  EXPECT_EQ(manager->CanvasAnnotation(), "Hi Again");

  // Test full clear.
  manager->ClearRenderedText();
  manager->UpdateAnnotation();
  EXPECT_TRUE(manager->CanvasAnnotation().empty());
}

}  // namespace blink
