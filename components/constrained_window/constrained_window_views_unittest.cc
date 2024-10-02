// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/constrained_window/constrained_window_views.h"
#include "base/memory/raw_ptr.h"

#include <memory>

#include "build/build_config.h"
#include "components/constrained_window/constrained_window_views_client.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

using views::Widget;

namespace constrained_window {
namespace {

// Dummy client that returns a null modal dialog host and host view.
class TestConstrainedWindowViewsClient
    : public constrained_window::ConstrainedWindowViewsClient {
 public:
  TestConstrainedWindowViewsClient() = default;

  TestConstrainedWindowViewsClient(const TestConstrainedWindowViewsClient&) =
      delete;
  TestConstrainedWindowViewsClient& operator=(
      const TestConstrainedWindowViewsClient&) = delete;

  // ConstrainedWindowViewsClient:
  web_modal::ModalDialogHost* GetModalDialogHost(
      gfx::NativeWindow parent) override {
    return nullptr;
  }
  gfx::NativeView GetDialogHostView(gfx::NativeWindow parent) override {
    return nullptr;
  }
};

// ViewsDelegate to provide context to dialog creation functions such as
// CreateBrowserModalDialogViews() which do not allow InitParams to be set, and
// pass a null |context| argument to DialogDelegate::CreateDialogWidget().
class TestViewsDelegateWithContext : public views::TestViewsDelegate {
 public:
  TestViewsDelegateWithContext() = default;

  TestViewsDelegateWithContext(const TestViewsDelegateWithContext&) = delete;
  TestViewsDelegateWithContext& operator=(const TestViewsDelegateWithContext&) =
      delete;

  void set_context(gfx::NativeWindow context) { context_ = context; }

  // ViewsDelegate:
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    if (!params->context)
      params->context = context_;
    TestViewsDelegate::OnBeforeWidgetInit(params, delegate);
  }

 private:
  gfx::NativeWindow context_ = nullptr;
};

class ConstrainedWindowViewsTest : public views::ViewsTestBase {
 public:
  ConstrainedWindowViewsTest() = default;

  ConstrainedWindowViewsTest(const ConstrainedWindowViewsTest&) = delete;
  ConstrainedWindowViewsTest& operator=(const ConstrainedWindowViewsTest&) =
      delete;

  ~ConstrainedWindowViewsTest() override = default;

  void SetUp() override {
    auto views_delegate = std::make_unique<TestViewsDelegateWithContext>();

    // set_views_delegate() must be called before SetUp(), and GetContext() is
    // null before that, so take a reference.
    TestViewsDelegateWithContext* views_delegate_weak = views_delegate.get();
    set_views_delegate(std::move(views_delegate));
    views::ViewsTestBase::SetUp();
    views_delegate_weak->set_context(GetContext());

    delegate_ = std::make_unique<views::DialogDelegate>();
    auto contents = std::make_unique<views::StaticSizedView>();
    contents_ = delegate_->SetContentsView(std::move(contents));

    dialog_ = views::DialogDelegate::CreateDialogWidget(delegate_.get(),
                                                        GetContext(), nullptr);
    dialog_host_ = std::make_unique<web_modal::TestWebContentsModalDialogHost>(
        dialog_->GetNativeView());
    dialog_host_->set_max_dialog_size(gfx::Size(5000, 5000));

    // Make sure the dialog size is dominated by the preferred size of the
    // contents.
    gfx::Size preferred_size = dialog()->GetRootView()->GetPreferredSize();
    preferred_size.Enlarge(500, 500);
    contents_->SetPreferredSize(preferred_size);
  }

  void TearDown() override {
    contents_ = nullptr;
    dialog_host_.reset();
    dialog_->CloseNow();
    ViewsTestBase::TearDown();
  }

  gfx::Size GetDialogSize() {
    return dialog()->GetRootView()->GetBoundsInScreen().size();
  }

  views::DialogDelegate* delegate() { return delegate_.get(); }
  views::View* contents() { return contents_; }
  web_modal::TestWebContentsModalDialogHost* dialog_host() {
    return dialog_host_.get();
  }
  Widget* dialog() { return dialog_; }

 private:
  std::unique_ptr<views::DialogDelegate> delegate_;
  raw_ptr<views::View> contents_ = nullptr;
  std::unique_ptr<web_modal::TestWebContentsModalDialogHost> dialog_host_;
  raw_ptr<Widget> dialog_ = nullptr;
};

}  // namespace

// Make sure a dialog that increases its preferred size grows on the next
// position update.
TEST_F(ConstrainedWindowViewsTest, GrowModalDialogSize) {
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  gfx::Size expected_size = GetDialogSize();
  gfx::Size preferred_size = contents()->GetPreferredSize();
  expected_size.Enlarge(50, 50);
  preferred_size.Enlarge(50, 50);
  contents()->SetPreferredSize(preferred_size);
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());
}

// Make sure a dialog that reduces its preferred size shrinks on the next
// position update.
TEST_F(ConstrainedWindowViewsTest, ShrinkModalDialogSize) {
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  gfx::Size expected_size = GetDialogSize();
  gfx::Size preferred_size = contents()->GetPreferredSize();
  expected_size.Enlarge(-50, -50);
  preferred_size.Enlarge(-50, -50);
  contents()->SetPreferredSize(preferred_size);
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());
}

// Make sure browser modal dialogs are not affected by restrictions on web
// content modal dialog maximum sizes.
TEST_F(ConstrainedWindowViewsTest, MaximumBrowserDialogSize) {
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  gfx::Size dialog_size = GetDialogSize();
  gfx::Size max_dialog_size = dialog_size;
  max_dialog_size.Enlarge(-50, -50);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateWidgetModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(dialog_size.ToString(), GetDialogSize().ToString());
}

// Web content modal dialogs should not get a size larger than what the dialog
// host gives as the maximum size.
TEST_F(ConstrainedWindowViewsTest, MaximumWebContentsDialogSize) {
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  gfx::Size full_dialog_size = GetDialogSize();
  gfx::Size max_dialog_size = full_dialog_size;
  max_dialog_size.Enlarge(-50, -50);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  // The top border of the dialog is intentionally drawn outside the area
  // specified by the dialog host, so add it to the size the dialog is expected
  // to occupy.
  gfx::Size expected_size = max_dialog_size;
  expected_size.Enlarge(
      0, dialog()->non_client_view()->frame_view()->GetInsets().top());
  EXPECT_EQ(expected_size.ToString(), GetDialogSize().ToString());

  // Increasing the maximum dialog size should bring the dialog back to its
  // original size.
  max_dialog_size.Enlarge(100, 100);
  dialog_host()->set_max_dialog_size(max_dialog_size);
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());
  EXPECT_EQ(full_dialog_size.ToString(), GetDialogSize().ToString());
}

// Ensure CreateBrowserModalDialogViews() works correctly with a null parent.
// Flaky on Win10. https://crbug.com/1009182
#if BUILDFLAG(IS_WIN)
#define MAYBE_NullModalParent DISABLED_NullModalParent
#else
#define MAYBE_NullModalParent NullModalParent
#endif
TEST_F(ConstrainedWindowViewsTest, MAYBE_NullModalParent) {
  // Use desktop widgets (except on ChromeOS) for extra coverage.
  test_views_delegate()->set_use_desktop_native_widgets(true);

  SetConstrainedWindowViewsClient(
      std::make_unique<TestConstrainedWindowViewsClient>());
  auto delegate = std::make_unique<views::DialogDelegate>();
  delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
  views::Widget* widget =
      CreateBrowserModalDialogViews(delegate.get(), nullptr);
  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  widget->CloseNow();
}

// Make sure dialogs presented off-screen are properly clamped to the nearest
// screen.
TEST_F(ConstrainedWindowViewsTest, ClampDialogToNearestDisplay) {
  // Make sure the dialog will fit fully on the display
  contents()->SetPreferredSize(gfx::Size(200, 100));

  // First, make sure the host and dialog are sized and positioned.
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());

  const display::Screen* screen = display::Screen::GetScreen();
  const display::Display display = screen->GetPrimaryDisplay();
  // Within the tests there is only 1 display. Error if that ever changes.
  EXPECT_EQ(screen->GetNumDisplays(), 1);
  const gfx::Rect extents = display.work_area();

  // Move the host completely off the screen.
  views::Widget* host_widget =
      views::Widget::GetWidgetForNativeView(dialog_host()->GetHostView());
  gfx::Rect host_bounds = host_widget->GetWindowBoundsInScreen();
  host_bounds.set_origin(gfx::Point(extents.right(), extents.bottom()));
  host_widget->SetBounds(host_bounds);

  // Make sure the host is fully off the screen.
  EXPECT_FALSE(extents.Intersects(host_widget->GetWindowBoundsInScreen()));

  // Now reposition the modal dialog into the display.
  UpdateWebContentsModalDialogPosition(dialog(), dialog_host());

  const gfx::Rect dialog_bounds = dialog()->GetRootView()->GetBoundsInScreen();

  // The dialog should now be fully on the display.
  EXPECT_TRUE(extents.Contains(dialog_bounds));
}

}  // namespace constrained_window
