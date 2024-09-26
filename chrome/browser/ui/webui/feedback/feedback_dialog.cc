// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/feedback/feedback_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/api/feedback_private.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Default width/height of the Feedback Window.
const int kDefaultWidth = 500;
const int kDefaultHeight = 628;

}  // namespace

using extensions::api::feedback_private::FeedbackFlow;
using extensions::api::feedback_private::FeedbackInfo;

// static
FeedbackDialog* FeedbackDialog::current_instance_ = nullptr;

// static
FeedbackDialog* FeedbackDialog::GetInstanceForTest() {
  return current_instance_;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(FeedbackDialog,
                                      kFeedbackDialogForTesting);

// static
void FeedbackDialog::CreateOrShow(
    Profile* profile,
    const extensions::api::feedback_private::FeedbackInfo& info) {
  if (current_instance_) {
    DCHECK(current_instance_->widget_);
    const Profile* current_profile =
        current_instance_->profile_keep_alive_.profile();
    if (profile == current_profile) {
      // Focus the window hosting the dialog that has already been created.
      current_instance_->widget_->Show();
      return;
    } else {
      // Close the existing window and create a new one for `profile`.
      VLOG(1) << "Re-opening the feedback dialog for profile \""
              << profile->GetDebugName() << "\" (was \""
              << current_profile->GetDebugName() << "\")";
      current_instance_->attached_to_current_instance_ = false;
      current_instance_->widget_->Close();
    }
  }

  current_instance_ = new FeedbackDialog(profile, info);
  gfx::NativeWindow window =
      chrome::ShowWebDialog(nullptr, profile, current_instance_,
                            /*show=*/false);
  current_instance_->widget_ = views::Widget::GetWidgetForNativeWindow(window);
  views::View* root = current_instance_->widget_->GetRootView();
  if (root != nullptr) {
    root->SetProperty(views::kElementIdentifierKey, kFeedbackDialogForTesting);
  }
}

FeedbackDialog::FeedbackDialog(
    Profile* profile,
    const extensions::api::feedback_private::FeedbackInfo& info)
    : feedback_info_(info.ToValue()),
      feedback_flow_(info.flow),
      widget_(nullptr),
      // We need to use GetOriginalProfile() here because `profile` may be an
      // OTR Profile (when opening Feedback dialog on ChromeOS login screen, for
      // example), and ScopedProfileKeepAlive only supports non-OTR Profiles.
      // Trying to acquire a keepalive on the OTR Profile would trigger a
      // DCHECK.
      //
      // TODO(crbug.com/1153922): Once OTR Profiles use refcounting, remove the
      // call to GetOriginalProfile(). The OTR Profile will hold a keepalive on
      // the regular Profile, so the ownership model will be more
      // straightforward.
      profile_keep_alive_(profile->GetOriginalProfile(),
                          ProfileKeepAliveOrigin::kFeedbackDialog) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  set_can_resize(false);
  set_can_minimize(true);
}

FeedbackDialog::~FeedbackDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (attached_to_current_instance_)
    current_instance_ = nullptr;
}

ui::ModalType FeedbackDialog::GetDialogModalType() const {
  // On the login screen, set to Modal mode. Otherwise, this is not visible.
  // For other cases, set to none Modal mode so the user can navigate to
  // other windows.
  return (feedback_flow_ == FeedbackFlow::kLogin) ? ui::MODAL_TYPE_SYSTEM
                                                  : ui::MODAL_TYPE_NONE;
}

std::u16string FeedbackDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(
      (feedback_flow_ == FeedbackFlow::kSadTabCrash)
          ? IDS_FEEDBACK_REPORT_PAGE_TITLE_SAD_TAB_FLOW
          : IDS_FEEDBACK_REPORT_PAGE_TITLE);
}

GURL FeedbackDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUIFeedbackURL);
}

void FeedbackDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

void FeedbackDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(new FeedbackHandler(this));
}

// The feedbackInfo will be available to JS via
// chrome.getVariableValue('dialogArguments')
std::string FeedbackDialog::GetDialogArgs() const {
  std::string data;
  base::JSONWriter::Write(feedback_info_, &data);
  return data;
}

void FeedbackDialog::Show() const {
  // The widget_ is set to null when the FeedbackDialog is constructed.
  // After the following two function calls, it is finally initialized.
  // Therefore, it is safer to check whether the widget_ is null
  if (this->widget_)
    this->widget_->Show();
}

views::Widget* FeedbackDialog::GetWidget() const {
  return this->widget_;
}

void FeedbackDialog::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr /* extension */);
}

bool FeedbackDialog::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return true;
}

void FeedbackDialog::OnDialogClosed(const std::string& json_retval) {
  DCHECK(this == current_instance_ || !attached_to_current_instance_);
  delete this;
}

void FeedbackDialog::OnCloseContents(WebContents* source,
                                     bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool FeedbackDialog::ShouldShowDialogTitle() const {
  return true;
}
