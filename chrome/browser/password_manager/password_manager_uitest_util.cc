// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_uitest_util.h"

#include "testing/gtest/include/gtest/gtest.h"

void TestGenerationPopupObserver::OnPopupShown(
    PasswordGenerationPopupController::GenerationUIState state) {
  popup_showing_ = GenerationPopup::kShown;
  state_ = state;
  MaybeQuitRunLoop();
}

void TestGenerationPopupObserver::OnPopupHidden() {
  popup_showing_ = GenerationPopup::kHidden;
  MaybeQuitRunLoop();
}

bool TestGenerationPopupObserver::popup_showing() const {
  return popup_showing_ == GenerationPopup::kShown;
}

GenerationUIState TestGenerationPopupObserver::state() const {
  return state_;
}

// Waits until the popup is in specified status.
void TestGenerationPopupObserver::WaitForStatus(GenerationPopup status) {
  if (status == popup_showing_)
    return;
  SCOPED_TRACE(::testing::Message()
               << "WaitForStatus " << static_cast<int>(status));
  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop_->Run();
  EXPECT_EQ(popup_showing_, status);
}

// Waits until the popup is either shown or hidden.
void TestGenerationPopupObserver::WaitForStatusChange() {
  SCOPED_TRACE(::testing::Message() << "WaitForStatusChange");
  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop_->Run();
}
void TestGenerationPopupObserver::MaybeQuitRunLoop() {
  if (run_loop_) {
    run_loop_->Quit();
    run_loop_ = nullptr;
  }
}

void ObservingAutofillClient::WaitForAutofillPopup() {
  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop.Run();
  DCHECK(!run_loop_);
}

void ObservingAutofillClient::ShowAutofillPopup(
    const autofill::AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<autofill::AutofillPopupDelegate> delegate) {
  if (run_loop_)
    run_loop_->Quit();
  run_loop_ = nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ObservingAutofillClient);
