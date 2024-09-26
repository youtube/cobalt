// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/accessibility/accessibility_focus_highlight.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/focus_changed_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

// To rebaseline this test on all platforms:
// 1. Run a CQ+1 dry run.
// 2. Click the failing bots for android, windows, mac, and linux.
// 3. Find the failing browser_tests step.
// 4. Click the "Deterministic failure" link for the failing test case.
// 5. Copy the "Actual pixels" data url and paste into browser.
// 6. Save the image into your chromium checkout in
//    chrome/test/data/accessibility.

class AccessibilityFocusHighlightBrowserTest : public InProcessBrowserTest {
 public:
  AccessibilityFocusHighlightBrowserTest() = default;
  ~AccessibilityFocusHighlightBrowserTest() override = default;
  AccessibilityFocusHighlightBrowserTest(
      const AccessibilityFocusHighlightBrowserTest&) = delete;
  AccessibilityFocusHighlightBrowserTest& operator=(
      const AccessibilityFocusHighlightBrowserTest&) = delete;

  // InProcessBrowserTest overrides:
  void SetUp() override {
    EnablePixelOutput();
    scoped_feature_list_.InitAndEnableFeature(
        features::kAccessibilityFocusHighlight);
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Force the CPU backend to use AAA. (https://crbug.com/1421297)
    command_line->AppendSwitch(switches::kForceSkiaAnalyticAntialiasing);
  }

  bool ColorsApproximatelyEqual(SkColor color1, SkColor color2) {
    return abs(static_cast<int>(SkColorGetR(color1)) -
               static_cast<int>(SkColorGetR(color2))) < 50 &&
           abs(static_cast<int>(SkColorGetG(color1)) -
               static_cast<int>(SkColorGetG(color2))) < 50 &&
           abs(static_cast<int>(SkColorGetB(color1)) -
               static_cast<int>(SkColorGetB(color2))) < 50;
  }

  float CountPercentPixelsWithColor(const gfx::Image& image, SkColor color) {
    SkBitmap bitmap = image.AsBitmap();
    int count = 0;
    for (int x = 0; x < bitmap.width(); ++x) {
      for (int y = 0; y < bitmap.height(); ++y) {
        if (ColorsApproximatelyEqual(color, bitmap.getColor(x, y)))
          count++;
      }
    }
    return count * 100.0f / (bitmap.width() * bitmap.height());
  }

  gfx::Image CaptureWindowContents() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::Widget* widget = browser_view->GetWidget();
    gfx::Rect bounds = widget->GetWindowBoundsInScreen();
    bounds.Offset(-bounds.OffsetFromOrigin());
    gfx::NativeView native_view = widget->GetNativeView();

    // Keep trying until we get a successful capture.
    while (true) {
      // First try sync. If that fails, try async.
      gfx::Image result_image;
      if (!ui::GrabViewSnapshot(native_view, bounds, &result_image)) {
        const auto on_got_snapshot = [](base::RunLoop* run_loop,
                                        gfx::Image* image,
                                        gfx::Image got_image) {
          *image = got_image;
          run_loop->Quit();
        };
        base::RunLoop run_loop;
        ui::GrabViewSnapshotAsync(
            native_view, bounds,
            base::BindOnce(on_got_snapshot, &run_loop, &result_image));
        run_loop.Run();
      }

      if (result_image.Size().IsEmpty()) {
        LOG(INFO) << "Bitmap not correct size, trying to capture again";
        continue;
      }

      // Skip through every 16th pixel (just for speed, no need to check
      // every single one). If we find at least one opaque pixel then we
      // assume we got a valid image. If the capture fails we sometimes get
      // an all transparent image, but when it succeeds there can be a
      // transparent edge.
      bool found_opaque_pixel = false;
      for (int x = 0; x < bounds.width() && !found_opaque_pixel; x += 16) {
        for (int y = 0; y < bounds.height() && !found_opaque_pixel; y += 16) {
          if (SkColorGetA(result_image.AsBitmap().getColor(x, y)) ==
              SK_AlphaOPAQUE) {
            found_opaque_pixel = true;
          }
        }
      }
      if (!found_opaque_pixel) {
        LOG(INFO) << "Bitmap not opaque, trying to capture again";
        continue;
      }

      return result_image;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Smoke test that ensures that when a node gets focus, the layer with the
// focus highlight actually gets drawn.
//
// Flaky on all platforms. TODO(crbug.com/1083806): Enable this test.
IN_PROC_BROWSER_TEST_F(AccessibilityFocusHighlightBrowserTest,
                       DISABLED_DrawsHighlight) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,"
                      "<body style='background-color: rgb(204, 255, 255);'>"
                      "<div tabindex=0 id='div'>Focusable div</div>")));

  AccessibilityFocusHighlight::SetNoFadeForTesting();
  AccessibilityFocusHighlight::SkipActivationCheckForTesting();
  AccessibilityFocusHighlight::UseDefaultColorForTesting();

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityFocusHighlightEnabled, true);

  // The web page has a background with a specific color. Keep looping until we
  // capture an image of the page that's more than 90% that color.
  gfx::Image image;
  do {
    base::RunLoop().RunUntilIdle();
    image = CaptureWindowContents();
  } while (CountPercentPixelsWithColor(image, SkColorSetRGB(204, 255, 255)) <
           90.0f);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SkColor highlight_color =
      browser_view->GetColorProvider()->GetColor(kColorFocusHighlightDefault);

  // Initially less than 0.05% of the image should be the focus ring's highlight
  // color.
  ASSERT_LT(CountPercentPixelsWithColor(image, highlight_color), 0.05f);

  // Focus something.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string script("document.getElementById('div').focus();");
  EXPECT_TRUE(content::ExecuteScript(web_contents, script));

  // Now wait until at least 0.1% of the image has the focus ring's highlight
  // color. If it never does, the test will time out.
  do {
    base::RunLoop().RunUntilIdle();
    image = CaptureWindowContents();
  } while (CountPercentPixelsWithColor(image, highlight_color) < 0.1f);
}

IN_PROC_BROWSER_TEST_F(AccessibilityFocusHighlightBrowserTest,
                       FocusBoundsIncludeImages) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,"
                      "<a id='link' href=''>"
                      "<img id='image' width='220' height='147' "
                      "style='vertical-align: middle;'>"
                      "</a>")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::FocusChangedObserver observer(web_contents);
  std::string script("document.getElementById('link').focus();");
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  auto details = observer.Wait();

  gfx::Rect bounds = details.node_bounds_in_screen;
  EXPECT_EQ(220, bounds.width());

  // Not sure where the extra px of height are coming from...
  EXPECT_EQ(147, bounds.height());
}

class ReadbackHolder : public base::RefCountedThreadSafe<ReadbackHolder> {
 public:
  ReadbackHolder() : run_loop_(std::make_unique<base::RunLoop>()) {}

  void OutputRequestCallback(std::unique_ptr<viz::CopyOutputResult> result) {
    if (result->IsEmpty()) {
      result_.reset();
    } else {
      auto scoped_sk_bitmap = result->ScopedAccessSkBitmap();
      result_ =
          std::make_unique<SkBitmap>(scoped_sk_bitmap.GetOutScopedBitmap());
      EXPECT_TRUE(result_->readyToDraw());
    }
    run_loop_->Quit();
  }

  void WaitForReadback() { run_loop_->Run(); }

  const SkBitmap& result() const { return *result_; }

 private:
  friend class base::RefCountedThreadSafe<ReadbackHolder>;

  virtual ~ReadbackHolder() = default;

  std::unique_ptr<SkBitmap> result_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

const cc::ExactPixelComparator pixel_comparator;

// Flaky on Lacros: https://crbug.com/1289366
#if (BUILDFLAG(IS_MAC) &&                                     \
     MAC_OS_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_15) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_FocusAppearance DISABLED_FocusAppearance
#else
#define MAYBE_FocusAppearance FocusAppearance
#endif

IN_PROC_BROWSER_TEST_F(AccessibilityFocusHighlightBrowserTest,
                       MAYBE_FocusAppearance) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  AccessibilityFocusHighlight::SetNoFadeForTesting();
  AccessibilityFocusHighlight::SkipActivationCheckForTesting();

  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityFocusHighlightEnabled, true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("data:text/html,"
                                                "<a id='link' href=''>"
                                                "<img width='10' height='10'>"
                                                "</a>")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::FocusChangedObserver observer(web_contents);
  std::string script("document.getElementById('link').focus();");
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  observer.Wait();
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  AccessibilityFocusHighlight* highlight =
      browser_view->GetAccessibilityFocusHighlightForTesting();
  ui::Layer* layer = highlight->GetLayerForTesting();

  gfx::Rect source_rect = gfx::Rect(layer->size());
  scoped_refptr<ReadbackHolder> holder(new ReadbackHolder);
  std::unique_ptr<viz::CopyOutputRequest> request =
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(&ReadbackHolder::OutputRequestCallback, holder));
  request->set_area(source_rect);
  layer->RequestCopyOfOutput(std::move(request));
  holder->WaitForReadback();
  SkBitmap bitmap(holder->result());
  ASSERT_FALSE(bitmap.drawsNothing());

  base::FilePath dir_test_data;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &dir_test_data));

  std::string screenshot_filename = "focus_highlight_appearance";
  std::string platform_suffix;
#if BUILDFLAG(IS_MAC)
  platform_suffix = "_mac";
#elif BUILDFLAG(IS_WIN)
  platform_suffix = "_win";
#endif

  base::FilePath golden_filepath =
      dir_test_data.AppendASCII("accessibility")
          .AppendASCII(screenshot_filename + ".png");
  base::FilePath golden_filepath_platform =
      golden_filepath.InsertBeforeExtensionASCII(platform_suffix);
  if (base::PathExists(golden_filepath_platform)) {
    golden_filepath = golden_filepath_platform;
  }

  bool snapshot_matches =
      cc::MatchesPNGFile(bitmap, golden_filepath, pixel_comparator);
  EXPECT_TRUE(snapshot_matches);
}
