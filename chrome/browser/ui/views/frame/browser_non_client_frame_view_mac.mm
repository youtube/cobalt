// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mac.h"

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#include "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/caption_button_placeholder_container.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_utils.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom-shared.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"

namespace {

// Keep in sync with web_app_frame_toolbar_browsertest.cc
constexpr double kTitlePaddingWidthFraction = 0.1;

FullscreenToolbarStyle GetUserPreferredToolbarStyle(bool always_show) {
  // In Kiosk mode, we don't show top Chrome UI.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return FullscreenToolbarStyle::TOOLBAR_NONE;
  return always_show ? FullscreenToolbarStyle::TOOLBAR_PRESENT
                     : FullscreenToolbarStyle::TOOLBAR_HIDDEN;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, public:

BrowserNonClientFrameViewMac::BrowserNonClientFrameViewMac(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view) {
  if (web_app::AppBrowserController::IsWebApp(browser_view->browser())) {
    auto* provider =
        web_app::WebAppProvider::GetForWebApps(browser_view->GetProfile());
    always_show_toolbar_in_fullscreen_observation_.Observe(
        &provider->registrar_unsafe());
  } else {
    show_fullscreen_toolbar_.Init(
        prefs::kShowFullscreenToolbar, browser_view->GetProfile()->GetPrefs(),
        base::BindRepeating(
            &BrowserNonClientFrameViewMac::UpdateFullscreenTopUI,
            base::Unretained(this)));
  }
  if (!browser_view->UsesImmersiveFullscreenMode()) {
    fullscreen_toolbar_controller_.reset(
        [[FullscreenToolbarController alloc] initWithBrowserView:browser_view]);
    [fullscreen_toolbar_controller_
        setToolbarStyle:GetUserPreferredToolbarStyle(
                            AlwaysShowToolbarInFullscreen())];
  }

  if (browser_view->GetIsWebAppType()) {
    if (browser_view->IsWindowControlsOverlayEnabled()) {
      caption_button_placeholder_container_ =
          AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>());
    }
  }
}

BrowserNonClientFrameViewMac::~BrowserNonClientFrameViewMac() {
  if ([fullscreen_toolbar_controller_ isInFullscreen])
    [fullscreen_toolbar_controller_ exitFullscreenMode];
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, BrowserNonClientFrameView implementation:

void BrowserNonClientFrameViewMac::OnFullscreenStateChanged() {
  if (browser_view()->UsesImmersiveFullscreenMode()) {
    browser_view()->immersive_mode_controller()->SetEnabled(
        browser_view()->IsFullscreen());
    UpdateFullscreenTopUI();

    // browser_view()->Layout() is not needed since top chrome is in another
    // widget.
    return;
  }

  if (browser_view()->IsFullscreen()) {
    [fullscreen_toolbar_controller_ enterFullscreenMode];
  } else {
    // Exiting tab fullscreen requires updating Top UI.
    // Called from here so we can capture exiting tab fullscreen both by
    // pressing 'ESC' key and by clicking green traffic light button.

    UpdateFullscreenTopUI();
    [fullscreen_toolbar_controller_ exitFullscreenMode];
  }
  browser_view()->Layout();
}

bool BrowserNonClientFrameViewMac::CaptionButtonsOnLeadingEdge() const {
  // TODO(https://crbug.com/860627): In "partial" RTL mode (where the OS is in
  // LTR mode while Chrome is in RTL mode, or vice versa) this should return
  // false rather than true.
  return true;
}

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  // TODO(weili): In the future, we should hide the title bar, and show the
  // tab strip directly under the menu bar. For now, just lay our content
  // under the native title bar. Use the default title bar height to avoid
  // calling through private APIs.
  const bool restored = !frame()->IsMaximized() && !frame()->IsFullscreen();
  gfx::Rect bounds(0, GetTopInset(restored), width(),
                   tabstrip_minimum_size.height());

  // Do not draw caption buttons on fullscreen.
  if (!frame()->IsFullscreen()) {
    bounds.Inset(GetCaptionButtonInsets());
  }

  return bounds;
}

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  if (ShouldHideTopUIForFullscreen()) {
    return gfx::Rect();
  }
  gfx::Rect bounds(0, 0, width(),
                   toolbar_preferred_size.height() + kWebAppMenuMargin * 2);

  // Do not draw caption buttons on fullscreen.
  if (!frame()->IsFullscreen()) {
    bounds.Inset(GetCaptionButtonInsets());
  }

  return bounds;
}

void BrowserNonClientFrameViewMac::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  gfx::Rect toolbar_bounds(0, 0, width(), available_space.height());
  gfx::Rect title_bounds = available_space;
  const int title_padding =
      base::ClampRound(width() * kTitlePaddingWidthFraction);
  title_bounds.Inset(gfx::Insets::VH(0, title_padding));
  window_title_label.SetBoundsRect(GetCenteredTitleBounds(
      toolbar_bounds, title_bounds,
      window_title_label.CalculatePreferredSize().width()));
  // The background of the title area is always opaquely drawn, but when in
  // immersive fullscreen, it is drawn in a way that isn't detected by the
  // DCHECK in Label. As such, disable the DCHECK.
  window_title_label.SetSkipSubpixelRenderingOpacityCheck(
      browser_view()->IsImmersiveModeEnabled());
}

int BrowserNonClientFrameViewMac::GetTopInset(bool restored) const {
  if (!browser_view()->GetTabStripVisible())
    return 0;

  // Mac seems to reserve 1 DIP of the top inset as a resize handle.
  constexpr int kResizeHandleHeight = 1;
  constexpr int kTabstripTopInset = 8;
  int top_inset = kTabstripTopInset;
  if (EverHasVisibleBackgroundTabShapes()) {
    top_inset =
        std::max(top_inset, BrowserNonClientFrameView::kMinimumDragHeight +
                                kResizeHandleHeight);
  }

  // Immersive fullscreen attaches the tab strip to the title bar, no need to
  // calculate the y_offset below.
  if (browser_view()->UsesImmersiveFullscreenMode()) {
    return top_inset;
  }

  // Calculate the y offset for the tab strip because in fullscreen mode the tab
  // strip may need to move under the slide down menu bar.
  CGFloat y_offset = TopUIFullscreenYOffset();
  if (y_offset > 0) {
    // When menubar shows up, we need to update mouse tracking area.
    NSWindow* window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
    NSRect content_bounds = [[window contentView] bounds];
    // Backing bar tracking area uses native coordinates.
    CGFloat tracking_height =
        FullscreenBackingBarHeight() + top_inset + y_offset;
    NSRect backing_bar_area =
        NSMakeRect(0, NSMaxY(content_bounds) - tracking_height,
                   NSWidth(content_bounds), tracking_height);
    [fullscreen_toolbar_controller_ updateToolbarFrame:backing_bar_area];
  }

  return y_offset + top_inset;
}

int BrowserNonClientFrameViewMac::GetThemeBackgroundXInset() const {
  return 0;
}

void BrowserNonClientFrameViewMac::UpdateFullscreenTopUI() {
  // Update to the new toolbar style if needed.
  FullscreenToolbarStyle new_style;
  FullscreenController* controller =
      browser_view()->GetExclusiveAccessManager()->fullscreen_controller();
  if ((controller->IsWindowFullscreenForTabOrPending() ||
       controller->IsExtensionFullscreenOrPending())) {
    browser_view()->HideDownloadShelf();
    new_style = FullscreenToolbarStyle::TOOLBAR_NONE;
  } else {
    new_style = GetUserPreferredToolbarStyle(AlwaysShowToolbarInFullscreen());
    browser_view()->UnhideDownloadShelf();
  }

  if (browser_view()->UsesImmersiveFullscreenMode()) {
    remote_cocoa::mojom::NativeWidgetNSWindow* ns_window_mojo =
        views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
            browser_view()->GetWidget()->GetNativeWindow())
            ->GetNSWindowMojo();
    static constexpr auto kStyleMap =
        base::MakeFixedFlatMap<FullscreenToolbarStyle,
                               remote_cocoa::mojom::ToolbarVisibilityStyle>(
            {{FullscreenToolbarStyle::TOOLBAR_PRESENT,
              remote_cocoa::mojom::ToolbarVisibilityStyle::kAlways},
             {FullscreenToolbarStyle::TOOLBAR_HIDDEN,
              remote_cocoa::mojom::ToolbarVisibilityStyle::kAutohide},
             {FullscreenToolbarStyle::TOOLBAR_NONE,
              remote_cocoa::mojom::ToolbarVisibilityStyle::kNone}});
    const auto* it = kStyleMap.find(new_style);
    remote_cocoa::mojom::ToolbarVisibilityStyle mapped_style =
        it != kStyleMap.end()
            ? it->second
            : remote_cocoa::mojom::ToolbarVisibilityStyle::kAutohide;
    ns_window_mojo->UpdateToolbarVisibility(mapped_style);
    // The layout changes further down are not needed in immersive fullscreen.
    return;
  }

  FullscreenToolbarStyle old_style =
      [fullscreen_toolbar_controller_ toolbarStyle];
  [fullscreen_toolbar_controller_ setToolbarStyle:new_style];
  if (![fullscreen_toolbar_controller_ isInFullscreen] ||
      old_style == new_style) {
    return;
  }

  // Notify browser that top ui state has been changed so that we can update
  // the bookmark bar state as well.
  browser_view()->browser()->FullscreenTopUIStateChanged();

  // Re-layout if toolbar style changes in fullscreen mode.
  if (frame()->IsFullscreen()) {
    browser_view()->Layout();
  }
}

void BrowserNonClientFrameViewMac::OnAlwaysShowToolbarInFullscreenChanged(
    const web_app::AppId& app_id,
    bool show) {
  if (web_app::AppBrowserController::IsForWebApp(browser_view()->browser(),
                                                 app_id)) {
    UpdateFullscreenTopUI();
  }
}

void BrowserNonClientFrameViewMac::OnAppRegistrarDestroyed() {
  always_show_toolbar_in_fullscreen_observation_.Reset();
}

bool BrowserNonClientFrameViewMac::ShouldHideTopUIForFullscreen() const {
  if (frame()->IsFullscreen()) {
    return [fullscreen_toolbar_controller_ toolbarStyle] !=
           FullscreenToolbarStyle::TOOLBAR_PRESENT;
  }
  return false;
}

void BrowserNonClientFrameViewMac::UpdateThrobber(bool running) {
}

void BrowserNonClientFrameViewMac::PaintAsActiveChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserNonClientFrameView::PaintAsActiveChanged();
}

void BrowserNonClientFrameViewMac::OnThemeChanged() {
  UpdateCaptionButtonPlaceholderContainerBackground();
  BrowserNonClientFrameView::OnThemeChanged();
}
///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::NonClientFrameView implementation:

gfx::Rect BrowserNonClientFrameViewMac::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect BrowserNonClientFrameViewMac::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_inset = GetTopInset(false);

  // If the operating system is handling drawing the window titlebar then the
  // titlebar height will not be included in |GetTopInset|, so we have to
  // explicitly add it. If a custom titlebar is being drawn, this calculation
  // will be zero.
  NSWindow* window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  DCHECK(window);
  top_inset += window.frame.size.height -
               [window contentRectForFrameRect:window.frame].size.height;

  return gfx::Rect(client_bounds.x(), client_bounds.y() - top_inset,
                   client_bounds.width(), client_bounds.height() + top_inset);
}

int BrowserNonClientFrameViewMac::NonClientHitTest(const gfx::Point& point) {
  int super_component = BrowserNonClientFrameView::NonClientHitTest(point);
  if (super_component != HTNOWHERE)
    return super_component;

  // BrowserView::NonClientHitTest will return HTNOWHERE for points that hit
  // the native title bar. On Mac, we need to explicitly return HTCAPTION for
  // those points.
  const int component = frame()->client_view()->NonClientHitTest(point);
  return (component == HTNOWHERE && bounds().Contains(point)) ? HTCAPTION
                                                              : component;
}

void BrowserNonClientFrameViewMac::GetWindowMask(const gfx::Size& size,
                                                 SkPath* window_mask) {}

void BrowserNonClientFrameViewMac::UpdateWindowIcon() {
}

void BrowserNonClientFrameViewMac::SizeConstraintsChanged() {
}

void BrowserNonClientFrameViewMac::UpdateMinimumSize() {
  GetWidget()->OnSizeConstraintsChanged();
}

void BrowserNonClientFrameViewMac::WindowControlsOverlayEnabledChanged() {
  if (browser_view()->IsWindowControlsOverlayEnabled()) {
    caption_button_placeholder_container_ =
        AddChildView(std::make_unique<CaptionButtonPlaceholderContainer>());
    UpdateCaptionButtonPlaceholderContainerBackground();
  } else {
    RemoveChildView(caption_button_placeholder_container_);
    caption_button_placeholder_container_ = nullptr;
  }
}
///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, views::View implementation:

gfx::Size BrowserNonClientFrameViewMac::GetMinimumSize() const {
  gfx::Size client_size = frame()->client_view()->GetMinimumSize();
  if (browser_view()->browser()->is_type_normal())
    client_size.SetToMax(
        browser_view()->tab_strip_region_view()->GetMinimumSize());

  // macOS apps generally don't allow their windows to get shorter than a
  // certain height, which empirically seems to be related to their *minimum*
  // width rather than their current width. This 4:3 ratio was chosen
  // empirically because it looks decent for both tabbed and untabbed browsers.
  client_size.SetToMax(gfx::Size(0, (client_size.width() * 3) / 4));

  return client_size;
}

void BrowserNonClientFrameViewMac::PaintChildren(const views::PaintInfo& info) {
  // In immersive fullscreen, the browser view's top container relies on the
  // non-client frame view to paint the frame (see comment in
  // TopContainerView::PaintChildren). We want the frame view to paint *only*
  // the frame but not its child (i.e. the BrowserView).
  // TODO(kerenzhu): we need this workaround due to the design of NonClientView,
  // that the frame part is not an independent child view. If it is an
  // independent view, overriding PaintChildren() will not be necessary.
  if (!browser_view()->immersive_mode_controller()->IsRevealed())
    BrowserNonClientFrameView::PaintChildren(info);
}

gfx::Insets BrowserNonClientFrameViewMac::GetCaptionButtonInsets() const {
  const int kCaptionWidth = base::mac::IsAtMostOS10_15() ? 70 : 85;
  if (CaptionButtonsOnLeadingEdge()) {
    return gfx::Insets::TLBR(0, kCaptionWidth, 0, 0);
  } else {
    return gfx::Insets::TLBR(0, 0, 0, kCaptionWidth);
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, protected:

// views::View:

void BrowserNonClientFrameViewMac::OnPaint(gfx::Canvas* canvas) {
  if (!browser_view()->GetIsNormalType() &&
      !browser_view()->GetIsWebAppType()) {
    return;
  }

  SkColor frame_color = GetFrameColor(BrowserFrameActiveState::kUseCurrent);
  canvas->DrawColor(frame_color);

  auto* theme_service =
      ThemeServiceFactory::GetForProfile(browser_view()->browser()->profile());
  if (!theme_service->UsingSystemTheme())
    PaintThemedFrame(canvas);
}

void BrowserNonClientFrameViewMac::Layout() {
  if (browser_view()->IsWindowControlsOverlayEnabled())
    LayoutWindowControlsOverlay();
  NonClientFrameView::Layout();
}

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameViewMac, private:

gfx::Rect BrowserNonClientFrameViewMac::GetCenteredTitleBounds(
    gfx::Rect frame,
    gfx::Rect available_space,
    int preferred_title_width) {
  // Center in container.
  frame.ClampToCenteredSize(gfx::Size(preferred_title_width, frame.height()));

  // Make it fit in available space.
  frame.AdjustToFit(available_space);

  return frame;
}

void BrowserNonClientFrameViewMac::PaintThemedFrame(gfx::Canvas* canvas) {
  gfx::ImageSkia image = GetFrameImage();
  canvas->TileImageInt(image, 0, TopUIFullscreenYOffset(), width(),
                       image.height());
  gfx::ImageSkia overlay = GetFrameOverlayImage();
  canvas->DrawImageInt(overlay, 0, 0);
}

CGFloat BrowserNonClientFrameViewMac::FullscreenBackingBarHeight() const {
  BrowserView* browser_view = this->browser_view();
  DCHECK(browser_view->IsFullscreen());

  CGFloat total_height = 0;
  if (browser_view->GetTabStripVisible())
    total_height += browser_view->GetTabStripHeight();

  if (browser_view->IsToolbarVisible())
    total_height += browser_view->toolbar()->bounds().height();

  return total_height;
}

int BrowserNonClientFrameViewMac::TopUIFullscreenYOffset() const {
  if (!browser_view()->GetTabStripVisible() || !browser_view()->IsFullscreen())
    return 0;

  CGFloat menu_bar_height =
      [[[NSApplication sharedApplication] mainMenu] menuBarHeight];
  // If there's a camera notch, the window is already below where the menu bar
  // will be, so we shouldn't account for it.
  if (@available(macos 12.0.1, *)) {
    id screen = [GetWidget()->GetNativeWindow().GetNativeNSWindow() screen];
    NSEdgeInsets insets = [screen safeAreaInsets];
    if (insets.top != 0)
      menu_bar_height = 0;
  }
  CGFloat title_bar_height =
      NSHeight([NSWindow frameRectForContentRect:NSZeroRect
                                       styleMask:NSWindowStyleMaskTitled]);
  if (browser_view()->UsesImmersiveFullscreenMode())
    return menu_bar_height == 0 ? 0 : menu_bar_height + title_bar_height;
  return [[fullscreen_toolbar_controller_ menubarTracker] menubarFraction] *
         (menu_bar_height + title_bar_height);
}

gfx::Rect BrowserNonClientFrameViewMac::GetCaptionButtonPlaceholderBounds(
    const gfx::Rect& frame,
    const gfx::Insets& caption_button_insets) {
  DCHECK(caption_button_insets.left() == 0 ||
         caption_button_insets.right() == 0);
  gfx::Rect non_caption_bounds = frame;
  non_caption_bounds.Inset(caption_button_insets);
  gfx::Rect bounds = frame;
  bounds.Subtract(non_caption_bounds);
  return bounds;
}

void BrowserNonClientFrameViewMac::LayoutWindowControlsOverlay() {
  int frame_available_height =
      browser_view()->GetWebAppFrameToolbarPreferredSize().height() +
      2 * kWebAppMenuMargin;
  gfx::Rect frame_available_bounds(0, 0, width(), frame_available_height);

  // Pad the width of caption_button_placeholder_container so the button on the
  // inner edge doesn't look like it's touching the overlay, but rather has a
  // little bit of space between them.
  gfx::Insets caption_button_insets = GetCaptionButtonInsets();
  gfx::Rect caption_button_container_bounds = GetCaptionButtonPlaceholderBounds(
      frame_available_bounds, caption_button_insets);

  // Layout CaptionButtonPlaceholderContainer which would have the traffic
  // lights.
  caption_button_placeholder_container_->SetBoundsRect(
      caption_button_container_bounds);
}

void BrowserNonClientFrameViewMac::
    UpdateCaptionButtonPlaceholderContainerBackground() {
  if (caption_button_placeholder_container_) {
    caption_button_placeholder_container_->SetBackground(
        views::CreateSolidBackground(
            GetFrameColor(BrowserFrameActiveState::kUseCurrent)));
  }
}

bool BrowserNonClientFrameViewMac::AlwaysShowToolbarInFullscreen() const {
  if (web_app::AppBrowserController::IsWebApp(browser_view()->browser())) {
    web_app::AppBrowserController* controller =
        browser_view()->browser()->app_controller();
    return controller->AlwaysShowToolbarInFullscreen();
  } else {
    return *show_fullscreen_toolbar_;
  }
}
