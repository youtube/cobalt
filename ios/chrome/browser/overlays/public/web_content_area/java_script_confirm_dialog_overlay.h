// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_CONFIRM_DIALOG_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_CONFIRM_DIALOG_OVERLAY_H_

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/overlays/public/overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_response_info.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

// Configuration object for OverlayRequests for JavaScript confirm dialogs.
class JavaScriptConfirmDialogRequest
    : public OverlayRequestConfig<JavaScriptConfirmDialogRequest> {
 public:
  ~JavaScriptConfirmDialogRequest() override;

  web::WebState* web_state() const { return weak_web_state_.get(); }
  const GURL& url() const { return url_; }
  bool is_main_frame() const { return is_main_frame_; }
  NSString* message() const { return message_; }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptConfirmDialogRequest);
  JavaScriptConfirmDialogRequest(web::WebState* web_state,
                                 const GURL& url,
                                 bool is_main_frame,
                                 NSString* message);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  base::WeakPtr<web::WebState> weak_web_state_;
  const GURL url_;
  bool is_main_frame_;
  NSString* message_ = nil;
};

// Response type used for JavaScript confirm dialogs.
class JavaScriptConfirmDialogResponse
    : public OverlayResponseInfo<JavaScriptConfirmDialogResponse> {
 public:
  ~JavaScriptConfirmDialogResponse() override;

  // The action selected by the user.
  enum class Action : short {
    kConfirm,      // Used when the user taps the OK button on a dialog.
    kCancel,       // Used when the user taps the Cancel button on a dialog.
    kBlockDialogs  // Used when the user taps the blocking option on a dialog,
    // indicating that subsequent dialogs from the page should be
    // blocked.
  };
  Action action() const { return action_; }

 private:
  OVERLAY_USER_DATA_SETUP(JavaScriptConfirmDialogResponse);
  JavaScriptConfirmDialogResponse(Action action);

  Action action_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_WEB_CONTENT_AREA_JAVA_SCRIPT_CONFIRM_DIALOG_OVERLAY_H_
