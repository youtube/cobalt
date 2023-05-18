// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/uwp/system_platform_error_internal.h"

using Windows::Foundation::AsyncOperationCompletedHandler;
using Windows::Foundation::AsyncStatus;
using Windows::Foundation::IAsyncOperation;
using Windows::UI::Popups::IUICommand;
using Windows::UI::Popups::MessageDialog;
using Windows::UI::Popups::UICommand;
using Windows::UI::Popups::UICommandInvokedHandler;

SbAtomic32 SbSystemPlatformErrorPrivate::s_error_count = 0;

SbSystemPlatformErrorPrivate::SbSystemPlatformErrorPrivate(
    ApplicationUwp* app,
    SbSystemPlatformErrorType type,
    SbSystemPlatformErrorCallback callback,
    void* user_data)
    : callback_(callback), user_data_(user_data) {
  SB_DCHECK(type == kSbSystemPlatformErrorTypeConnectionError);

  // Only one error dialog can be displayed at a time.
  if (SbAtomicAcquire_CompareAndSwap(&s_error_count, 0, 1) != 0) {
    return;
  }

  MessageDialog ^ dialog = ref new MessageDialog(app->GetString(
      "UNABLE_TO_CONTACT_YOUTUBE_1", "Sorry, could not connect to YouTube."));
  dialog->Commands->Append(
      MakeUICommand(app, "OFFLINE_MESSAGE_TRY_AGAIN", "Try again",
                    kSbSystemPlatformErrorResponsePositive));
  dialog->Commands->Append(MakeUICommand(app, "EXIT_BUTTON", "Exit",
                                         kSbSystemPlatformErrorResponseCancel));
  dialog->DefaultCommandIndex = 0;
  dialog->CancelCommandIndex = 1;

  try {
    dialog_operation_ = dialog->ShowAsync();
    dialog_operation_->Completed =
        ref new AsyncOperationCompletedHandler<IUICommand ^>(
            [this](IAsyncOperation<IUICommand ^> ^, AsyncStatus) {
              SB_DCHECK(SbAtomicAcquire_Load(&s_error_count) > 0);
              SbAtomicBarrier_Increment(&s_error_count, -1);
              delete this;
            });
  } catch (Platform::Exception ^) {
    SB_LOG(ERROR) << "Unable to raise SbSystemPlatformError";
    SbAtomicBarrier_Increment(&s_error_count, -1);
  }
}

bool SbSystemPlatformErrorPrivate::IsValid() const {
  return dialog_operation_.Get() != nullptr;
}

void SbSystemPlatformErrorPrivate::ClearAndDelete() {
  if (IsValid()) {
    dialog_operation_->Cancel();
  }
  delete this;
}

IUICommand ^ SbSystemPlatformErrorPrivate::MakeUICommand(
                 ApplicationUwp* app,
                 const char* id,
                 const char* fallback,
                 SbSystemPlatformErrorResponse response) {
  Platform::String ^ label = app->GetString(id, fallback);
  return ref new UICommand(label, ref new UICommandInvokedHandler(
                                      [this, response](IUICommand ^ command) {
                                        callback_(response, user_data_);
                                      }));
}
