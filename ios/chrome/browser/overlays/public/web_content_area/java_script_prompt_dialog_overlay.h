// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_PROMPT_DIALOG_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_PROMPT_DIALOG_OVERLAY_H_

#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_response_info.h"
#include "ios/web/public/web_state.h"
#include "url/gurl.h"

// Configuration object for OverlayRequests for JavaScript prompt dialogs.
class JavaScriptPromptDialogRequest
    : public OverlayRequestConfig<JavaScriptPromptDialogRequest> {
 public:
  ~JavaScriptPromptDialogRequest() override;

  web::WebState* web_state() const { return weak_web_state_.get(); }
  const GURL& url() const { return url_; }
  bool is_main_frame() const { return is_main_frame_; }
  NSString* message() const { return message_; }

  // The default text shown in the text field.
  NSString* default_text_field_value() const {
    return default_text_field_value_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptPromptDialogRequest);
  JavaScriptPromptDialogRequest(web::WebState* web_state,
                                const GURL& url,
                                bool is_main_frame,
                                NSString* message,
                                NSString* default_text_field_value);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  base::WeakPtr<web::WebState> weak_web_state_;
  const GURL url_;
  bool is_main_frame_;
  NSString* message_ = nil;
  NSString* default_text_field_value_ = nil;
};

// Response type used for JavaScript prompt dialogs.
class JavaScriptPromptDialogResponse
    : public OverlayResponseInfo<JavaScriptPromptDialogResponse> {
 public:
  ~JavaScriptPromptDialogResponse() override;

  // The action selected by the user.
  enum class Action : short {
    kConfirm,      // Used when the user taps the OK button on a dialog.
    kCancel,       // Used when the user taps the Cancel button on a dialog.
    kBlockDialogs  // Used when the user taps the blocking option on a dialog,
    // indicating that subsequent dialogs from the page should be
    // blocked.
  };
  Action action() const { return action_; }
  // The user input.
  NSString* user_input() const { return user_input_; }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptPromptDialogResponse);
  JavaScriptPromptDialogResponse(Action action, NSString* user_input);

  Action action_;
  NSString* user_input_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_PROMPT_DIALOG_OVERLAY_H_
