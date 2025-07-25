// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"

#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "chrome/android/chrome_jni_headers/ClickToCallMessageHandler_jni.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/proto/click_to_call_message.pb.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "url/gurl.h"

ClickToCallMessageHandler::ClickToCallMessageHandler() = default;

ClickToCallMessageHandler::~ClickToCallMessageHandler() = default;

void ClickToCallMessageHandler::OnMessage(
    chrome_browser_sharing::SharingMessage message,
    SharingMessageHandler::DoneCallback done_callback) {
  DCHECK(message.has_click_to_call_message());
  TRACE_EVENT0("sharing", "ClickToCallMessageHandler::OnMessage");
  base::ScopedClosureRunner runner(
      base::BindOnce(std::move(done_callback), /*response=*/nullptr));

  std::string phone_number = message.click_to_call_message().phone_number();
  GURL phone_url(base::StrCat({"tel:", phone_number}));
  bool is_valid_phone_number = IsUrlSafeForClickToCall(phone_url) &&
                               phone_url.GetContent() == phone_number;

  base::UmaHistogramBoolean("Sharing.ClickToCallPhoneNumberValid",
                            is_valid_phone_number);

  // This can happen if a user on an older version of Chrome on their desktop
  // clicks on a tel link that contains url-escaped unsafe characters like #.
  // Another reason might be if the remote sender is using a custom or
  // compromised version of Chrome. In either case, ignoring the message is the
  // safest option as we don't want to pass along this number to the Android
  // Dialer.
  if (!is_valid_phone_number)
    return;

  HandlePhoneNumber(phone_number);
}

void ClickToCallMessageHandler::HandlePhoneNumber(
    const std::string& phone_number) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ClickToCallMessageHandler_handleMessage(
      env, base::android::ConvertUTF8ToJavaString(env, phone_number));
}
