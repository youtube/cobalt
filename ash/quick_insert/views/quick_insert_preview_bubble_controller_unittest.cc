// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_preview_bubble_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/quick_insert/views/quick_insert_preview_bubble.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class QuickInsertPreviewBubbleControllerTest : public views::ViewsTestBase {
 public:
  QuickInsertPreviewBubbleControllerTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

// Creates a basic widget and view that acts as the anchor view for the preview
// bubble.
std::unique_ptr<views::Widget> CreateAnchorWidget(gfx::NativeWindow context) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::Type::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 400, 400);
  params.context = context;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<views::View>());
  widget->Show();
  return widget;
}

HoldingSpaceImage CreateUnresolvedAsyncImage() {
  return HoldingSpaceImage(QuickInsertPreviewBubbleView::kPreviewImageSize,
                           base::FilePath(), base::DoNothing());
}

TEST_F(QuickInsertPreviewBubbleControllerTest, ShowsBubbleAfterDelay) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleAfterDelay(&async_preview_image, base::FilePath(),
                                  anchor_widget->GetContentsView());
  task_environment()->FastForwardBy(base::Milliseconds(600));

  views::View* bubble_view = controller.bubble_view_for_testing();
  ASSERT_NE(bubble_view, nullptr);
  ViewDrawnWaiter().Wait(bubble_view);
  ASSERT_NE(bubble_view->GetWidget(), nullptr);
  views::test::WidgetVisibleWaiter(bubble_view->GetWidget()).Wait();
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       DoesNotShowBubbleIfCanceledBeforeDelay) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleAfterDelay(&async_preview_image, base::FilePath(),
                                  anchor_widget->GetContentsView());
  controller.CloseBubble();
  task_environment()->FastForwardBy(base::Milliseconds(600));

  ASSERT_EQ(controller.bubble_view_for_testing(), nullptr);
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       DoesNotShowBubbleIfAnchorWidgetClosedBeforeDelay) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleAfterDelay(&async_preview_image, base::FilePath(),
                                  anchor_widget->GetContentsView());
  task_environment()->FastForwardBy(base::Milliseconds(300));
  anchor_widget->CloseNow();
  task_environment()->FastForwardBy(base::Milliseconds(400));

  ASSERT_EQ(controller.bubble_view_for_testing(), nullptr);
}

TEST_F(QuickInsertPreviewBubbleControllerTest, CloseBubbleClosesBubbleWidget) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  ASSERT_NE(controller.bubble_view_for_testing(), nullptr);
  views::Widget* bubble_widget =
      controller.bubble_view_for_testing()->GetWidget();

  controller.CloseBubble();

  views::test::WidgetDestroyedWaiter(bubble_widget).Wait();
  EXPECT_EQ(controller.bubble_view_for_testing(), nullptr);
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       DestroyingAnchorWidgetDestroysBubbleWidget) {
  QuickInsertPreviewBubbleController controller;
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  ASSERT_NE(controller.bubble_view_for_testing(), nullptr);
  views::Widget* bubble_widget =
      controller.bubble_view_for_testing()->GetWidget();

  anchor_widget->Close();

  views::test::WidgetDestroyedWaiter(bubble_widget).Wait();
  EXPECT_EQ(controller.bubble_view_for_testing(), nullptr);
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       DestroyingAnchorWidgetImmediatelyDoesNotCrash) {
  QuickInsertPreviewBubbleController controller;
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());

  anchor_widget->CloseNow();

  EXPECT_EQ(controller.bubble_view_for_testing(), nullptr);
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       ShowBubbleWhileShownKeepsSameBubble) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  views::View* bubble_view = controller.bubble_view_for_testing();
  ViewDrawnWaiter().Wait(bubble_view);

  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());

  ASSERT_EQ(controller.bubble_view_for_testing(), bubble_view);
  EXPECT_EQ(controller.bubble_view_for_testing()->GetWidget(),
            bubble_view->GetWidget());
}

TEST_F(QuickInsertPreviewBubbleControllerTest, CloseBubbleWithoutShowing) {
  QuickInsertPreviewBubbleController controller;

  controller.CloseBubble();

  EXPECT_EQ(controller.bubble_view_for_testing(), nullptr);
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       ShowingBubbleWhileClosingOldBubble) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());

  // CloseBubble is asynchronous.
  controller.CloseBubble();
  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  views::View* bubble_view = controller.bubble_view_for_testing();
  ViewDrawnWaiter().Wait(bubble_view);

  ASSERT_EQ(controller.bubble_view_for_testing(), bubble_view);
  EXPECT_EQ(controller.bubble_view_for_testing()->GetWidget(),
            bubble_view->GetWidget());
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       ShowBubbleUsesPlaceholderBeforeBitmapResolves) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;

  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();
  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  QuickInsertPreviewBubbleView* bubble_view =
      controller.bubble_view_for_testing();
  ViewDrawnWaiter().Wait(bubble_view);

  EXPECT_EQ(bubble_view->GetPreviewImage().GetImage().AsBitmap().getColor(5, 5),
            SK_ColorTRANSPARENT);
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       ShowBubbleUpdatesPreviewAfterBitmapResolves) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  base::RunLoop run_loop;
  SkBitmap bitmap = gfx::test::CreateBitmap(100, SK_ColorBLUE);
  HoldingSpaceImage async_preview_image(
      QuickInsertPreviewBubbleView::kPreviewImageSize, base::FilePath(),
      base::BindLambdaForTesting(
          [&](const base::FilePath& file_path, const gfx::Size& size,
              HoldingSpaceImage::BitmapCallback callback) {
            std::move(callback).Run(&bitmap, base::File::Error::FILE_OK);
            run_loop.Quit();
          }));
  QuickInsertPreviewBubbleController controller;

  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  QuickInsertPreviewBubbleView* bubble_view =
      controller.bubble_view_for_testing();
  ViewDrawnWaiter().Wait(bubble_view);

  run_loop.Run();
  EXPECT_EQ(bubble_view->GetPreviewImage().GetImage().AsBitmap().getColor(5, 5),
            SK_ColorBLUE);
}

TEST_F(QuickInsertPreviewBubbleControllerTest, ShowBubbleHidesLabelsByDefault) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();

  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  QuickInsertPreviewBubbleView* bubble_view =
      controller.bubble_view_for_testing();
  ViewDrawnWaiter().Wait(bubble_view);

  EXPECT_FALSE(bubble_view->GetLabelVisibleForTesting());
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       SetBubbleMainTextHidesLabelsWithEmptyText) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();

  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  controller.SetBubbleMainText(u"");
  QuickInsertPreviewBubbleView* bubble_view =
      controller.bubble_view_for_testing();
  ViewDrawnWaiter().Wait(bubble_view);

  EXPECT_FALSE(bubble_view->GetLabelVisibleForTesting());
}

TEST_F(QuickInsertPreviewBubbleControllerTest,
       SetBubbleMainTextUpdatesBubbleText) {
  std::unique_ptr<views::Widget> anchor_widget =
      CreateAnchorWidget(GetContext());
  QuickInsertPreviewBubbleController controller;
  HoldingSpaceImage async_preview_image = CreateUnresolvedAsyncImage();

  controller.ShowBubbleImmediatelyForTesting(&async_preview_image,
                                             anchor_widget->GetContentsView());
  controller.SetBubbleMainText(u"Edited Dec 23");
  QuickInsertPreviewBubbleView* bubble_view =
      controller.bubble_view_for_testing();
  ViewDrawnWaiter().Wait(bubble_view);

  EXPECT_TRUE(bubble_view->GetLabelVisibleForTesting());
  EXPECT_EQ(bubble_view->GetMainTextForTesting(), u"Edited Dec 23");
}

}  // namespace
}  // namespace ash
