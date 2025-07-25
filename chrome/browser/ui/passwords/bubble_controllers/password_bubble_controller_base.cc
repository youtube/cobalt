// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "content/public/browser/web_contents.h"

PasswordBubbleControllerBase::PasswordBubbleControllerBase(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    password_manager::metrics_util::UIDisplayDisposition display_disposition)
    : metrics_recorder_(delegate->GetPasswordFormMetricsRecorder()),
      delegate_(std::move(delegate)) {
  if (metrics_recorder_) {
    metrics_recorder_->RecordPasswordBubbleShown(
        delegate_->GetCredentialSource(), display_disposition);
  }
  password_manager::metrics_util::LogUIDisplayDisposition(display_disposition);

  delegate_->OnBubbleShown();
}

PasswordBubbleControllerBase::~PasswordBubbleControllerBase() {
  // To make sure that subclasses reported intractions when being destructed.
  DCHECK(interaction_reported_);
}

void PasswordBubbleControllerBase::OnBubbleClosing() {
  // This method can be reentered from OnBubbleHidden() below. Reset the things
  // before calling it.
  if (!std::exchange(interaction_reported_, true))
    ReportInteractions();
  if (delegate_)
    std::exchange(delegate_, nullptr)->OnBubbleHidden();
}

Profile* PasswordBubbleControllerBase::GetProfile() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;
  return Profile::FromBrowserContext(web_contents->GetBrowserContext());
}

content::WebContents* PasswordBubbleControllerBase::GetWebContents() const {
  return delegate_ ? delegate_->GetWebContents() : nullptr;
}
