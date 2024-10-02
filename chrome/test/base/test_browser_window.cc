// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window.h"

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/sharing/sharing_dialog_data.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/geometry/rect.h"

// Helpers --------------------------------------------------------------------

std::unique_ptr<Browser> CreateBrowserWithTestWindowForParams(
    Browser::CreateParams params) {
  DCHECK(!params.window);
  TestBrowserWindow* window = new TestBrowserWindow;
  new TestBrowserWindowOwner(window);
  params.window = window;
  window->set_is_minimized(params.initial_show_state ==
                           ui::SHOW_STATE_MINIMIZED);
  // Tests generally expect TestBrowserWindows not to be active.
  window->set_is_active(params.initial_show_state != ui::SHOW_STATE_INACTIVE &&
                        params.initial_show_state != ui::SHOW_STATE_DEFAULT &&
                        params.initial_show_state != ui::SHOW_STATE_MINIMIZED);

  return std::unique_ptr<Browser>(Browser::Create(params));
}

// TestBrowserWindow::TestLocationBar -----------------------------------------

GURL TestBrowserWindow::TestLocationBar::GetDestinationURL() const {
  return GURL();
}

WindowOpenDisposition
    TestBrowserWindow::TestLocationBar::GetWindowOpenDisposition() const {
  return WindowOpenDisposition::CURRENT_TAB;
}

ui::PageTransition
    TestBrowserWindow::TestLocationBar::GetPageTransition() const {
  return ui::PAGE_TRANSITION_LINK;
}

base::TimeTicks TestBrowserWindow::TestLocationBar::GetMatchSelectionTimestamp()
    const {
  return base::TimeTicks();
}

const OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() const {
  return nullptr;
}

OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() {
  return nullptr;
}

LocationBarTesting*
    TestBrowserWindow::TestLocationBar::GetLocationBarForTesting() {
  return nullptr;
}

bool TestBrowserWindow::TestLocationBar::IsInputTypedUrlWithoutScheme() const {
  return false;
}

// TestBrowserWindow ----------------------------------------------------------

TestBrowserWindow::TestBrowserWindow() = default;

TestBrowserWindow::~TestBrowserWindow() = default;

void TestBrowserWindow::Close() {
  if (close_callback_) {
    std::move(close_callback_).Run();
  }
  is_closed_ = true;
}

bool TestBrowserWindow::IsActive() const {
  return is_active_;
}

ui::ZOrderLevel TestBrowserWindow::GetZOrderLevel() const {
  return ui::ZOrderLevel::kNormal;
}

gfx::NativeWindow TestBrowserWindow::GetNativeWindow() const {
  return native_window_;
}

bool TestBrowserWindow::IsOnCurrentWorkspace() const {
  return true;
}

void TestBrowserWindow::SetTopControlsShownRatio(
    content::WebContents* web_contents,
    float ratio) {}

bool TestBrowserWindow::DoBrowserControlsShrinkRendererSize(
    const content::WebContents* contents) const {
  return false;
}

ui::NativeTheme* TestBrowserWindow::GetNativeTheme() {
  return nullptr;
}

const ui::ThemeProvider* TestBrowserWindow::GetThemeProvider() const {
  return nullptr;
}

const ui::ColorProvider* TestBrowserWindow::GetColorProvider() const {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      {ui::ColorProviderManager::ColorMode::kLight,
       ui::ColorProviderManager::ContrastMode::kNormal,
       ui::SystemTheme::kDefault,
       ui::ColorProviderManager::FrameType::kChromium});
}

ui::ElementContext TestBrowserWindow::GetElementContext() {
  return element_context_;
}

int TestBrowserWindow::GetTopControlsHeight() const {
  return 0;
}

void TestBrowserWindow::SetTopControlsGestureScrollInProgress(
    bool in_progress) {}

StatusBubble* TestBrowserWindow::GetStatusBubble() {
  return nullptr;
}

gfx::Rect TestBrowserWindow::GetRestoredBounds() const {
  return gfx::Rect();
}

ui::WindowShowState TestBrowserWindow::GetRestoredState() const {
  return ui::SHOW_STATE_DEFAULT;
}

gfx::Rect TestBrowserWindow::GetBounds() const {
  return gfx::Rect();
}

gfx::Size TestBrowserWindow::GetContentsSize() const {
  return gfx::Size();
}

void TestBrowserWindow::SetContentsSize(const gfx::Size& size) {}

bool TestBrowserWindow::IsMaximized() const {
  return false;
}

bool TestBrowserWindow::IsMinimized() const {
  return is_minimized_;
}

bool TestBrowserWindow::ShouldHideUIForFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsFullscreenBubbleVisible() const {
  return false;
}

bool TestBrowserWindow::IsForceFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsVisible() const {
  return true;
}

LocationBar* TestBrowserWindow::GetLocationBar() const {
  return const_cast<TestLocationBar*>(&location_bar_);
}

autofill::AutofillBubbleHandler* TestBrowserWindow::GetAutofillBubbleHandler() {
  return &autofill_bubble_handler_;
}

ExtensionsContainer* TestBrowserWindow::GetExtensionsContainer() {
  return nullptr;
}

content::KeyboardEventProcessingResult
TestBrowserWindow::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool TestBrowserWindow::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

bool TestBrowserWindow::IsBookmarkBarVisible() const {
  return false;
}

bool TestBrowserWindow::IsBookmarkBarAnimating() const {
  return false;
}

bool TestBrowserWindow::IsTabStripEditable() const {
  return is_tab_strip_editable_;
}

void TestBrowserWindow::SetIsTabStripEditable(bool is_editable) {
  is_tab_strip_editable_ = is_editable;
}

bool TestBrowserWindow::IsToolbarVisible() const {
  return false;
}

bool TestBrowserWindow::IsToolbarShowing() const {
  return false;
}

bool TestBrowserWindow::IsLocationBarVisible() const {
  return false;
}

bool TestBrowserWindow::IsBorderlessModeEnabled() const {
  return false;
}

ShowTranslateBubbleResult TestBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool is_user_gesture) {
  return ShowTranslateBubbleResult::SUCCESS;
}

void TestBrowserWindow::StartPartialTranslate(
    const std::string& source_language,
    const std::string& target_language,
    const std::u16string& text_selection) {}

qrcode_generator::QRCodeGeneratorBubbleView*
TestBrowserWindow::ShowQRCodeGeneratorBubble(content::WebContents* contents,
                                             const GURL& url,
                                             bool show_back_button) {
  return nullptr;
}

SharingDialog* TestBrowserWindow::ShowSharingDialog(
    content::WebContents* web_contents,
    SharingDialogData data) {
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
sharing_hub::ScreenshotCapturedBubble*
TestBrowserWindow::ShowScreenshotCapturedBubble(content::WebContents* contents,
                                                const gfx::Image& image) {
  return nullptr;
}
#endif

send_tab_to_self::SendTabToSelfBubbleView*
TestBrowserWindow::ShowSendTabToSelfDevicePickerBubble(
    content::WebContents* contents) {
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
TestBrowserWindow::ShowSendTabToSelfPromoBubble(content::WebContents* contents,
                                                bool show_signin_button) {
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
views::Button* TestBrowserWindow::GetSharingHubIconButton() {
  return nullptr;
}
void TestBrowserWindow::ToggleMultitaskMenu() const {
  return;
}
#else
sharing_hub::SharingHubBubbleView* TestBrowserWindow::ShowSharingHubBubble(
    share::ShareAttempt attempt) {
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool TestBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* TestBrowserWindow::GetDownloadShelf() {
  return &download_shelf_;
}

DownloadBubbleUIController* TestBrowserWindow::GetDownloadBubbleUIController() {
  return nullptr;
}

std::unique_ptr<FindBar> TestBrowserWindow::CreateFindBar() {
  return nullptr;
}

web_modal::WebContentsModalDialogHost*
    TestBrowserWindow::GetWebContentsModalDialogHost() {
  return nullptr;
}

ExclusiveAccessContext* TestBrowserWindow::GetExclusiveAccessContext() {
  return nullptr;
}

std::string TestBrowserWindow::GetWorkspace() const {
  return workspace_;
}

bool TestBrowserWindow::IsVisibleOnAllWorkspaces() const {
  return visible_on_all_workspaces_;
}

std::unique_ptr<content::EyeDropper> TestBrowserWindow::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return nullptr;
}

void TestBrowserWindow::SetNativeWindow(gfx::NativeWindow window) {
  native_window_ = window;
}

void TestBrowserWindow::SetCloseCallback(base::OnceClosure close_callback) {
  close_callback_ = std::move(close_callback);
}

user_education::FeaturePromoController*
TestBrowserWindow::GetFeaturePromoController() {
  return feature_promo_controller_.get();
}

bool TestBrowserWindow::IsFeaturePromoActive(
    const base::Feature& iph_feature) const {
  return feature_promo_controller_ &&
         feature_promo_controller_->IsPromoActive(
             iph_feature, user_education::FeaturePromoStatus::kContinued);
}

bool TestBrowserWindow::MaybeShowFeaturePromo(
    const base::Feature& iph_feature,
    user_education::FeaturePromoSpecification::StringReplacements
        body_text_replacements,
    user_education::FeaturePromoController::BubbleCloseCallback
        close_callback) {
  return feature_promo_controller_ &&
         feature_promo_controller_->MaybeShowPromo(
             iph_feature, body_text_replacements, std::move(close_callback));
}

bool TestBrowserWindow::MaybeShowStartupFeaturePromo(
    const base::Feature& iph_feature,
    user_education::FeaturePromoSpecification::StringReplacements
        body_text_replacements,
    user_education::FeaturePromoController::StartupPromoCallback promo_callback,
    user_education::FeaturePromoController::BubbleCloseCallback
        close_callback) {
  if (!feature_promo_controller_)
    return false;
  return feature_promo_controller_->MaybeShowStartupPromo(
      iph_feature, body_text_replacements, std::move(promo_callback),
      std::move(close_callback));
}

bool TestBrowserWindow::CloseFeaturePromo(const base::Feature& iph_feature) {
  return feature_promo_controller_ &&
         feature_promo_controller_->EndPromo(iph_feature);
}

user_education::FeaturePromoHandle
TestBrowserWindow::CloseFeaturePromoAndContinue(
    const base::Feature& iph_feature) {
  return feature_promo_controller_
             ? feature_promo_controller_->CloseBubbleAndContinuePromo(
                   iph_feature)
             : user_education::FeaturePromoHandle();
}

void TestBrowserWindow::NotifyFeatureEngagementEvent(const char* event_name) {}

user_education::FeaturePromoController*
TestBrowserWindow::SetFeaturePromoController(
    std::unique_ptr<user_education::FeaturePromoController>
        feature_promo_controller) {
  feature_promo_controller_ = std::move(feature_promo_controller);
  return feature_promo_controller_.get();
}

// TestBrowserWindowOwner -----------------------------------------------------

TestBrowserWindowOwner::TestBrowserWindowOwner(TestBrowserWindow* window)
    : window_(window) {
  BrowserList::AddObserver(this);
}

TestBrowserWindowOwner::~TestBrowserWindowOwner() {
  BrowserList::RemoveObserver(this);
}

void TestBrowserWindowOwner::OnBrowserRemoved(Browser* browser) {
  if (browser->window() == window_.get())
    delete this;
}
