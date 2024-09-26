// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#import <Cocoa/Cocoa.h>

#include <utility>

#include "base/functional/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/grit/generated_resources.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace chrome {

MessageBoxResult ShowMessageBoxCocoa(const std::u16string& message,
                                     MessageBoxType type,
                                     const std::u16string& checkbox_text) {
  startup_metric_utils::SetNonBrowserUIDisplayed();
  if (internal::g_should_skip_message_box_for_test)
    return MESSAGE_BOX_RESULT_YES;

  NSAlert* alert = [[[NSAlert alloc] init] autorelease];
  [alert setMessageText:base::SysUTF16ToNSString(message)];
  [alert setAlertStyle:NSAlertStyleWarning];
  if (type == MESSAGE_BOX_TYPE_QUESTION) {
    [alert addButtonWithTitle:l10n_util::GetNSString(
                                  IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)];
    [alert addButtonWithTitle:l10n_util::GetNSString(
                                  IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL)];
  } else {
    [alert addButtonWithTitle:l10n_util::GetNSString(IDS_OK)];
  }

  base::scoped_nsobject<NSButton> checkbox;
  if (!checkbox_text.empty()) {
    checkbox.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
    [checkbox setButtonType:NSButtonTypeSwitch];
    [checkbox setTitle:base::SysUTF16ToNSString(checkbox_text)];
    [checkbox sizeToFit];
    [alert setAccessoryView:checkbox];
  }

  NSInteger result = [alert runModal];
  if (result == NSAlertSecondButtonReturn)
    return MESSAGE_BOX_RESULT_NO;

  if (!checkbox || ([checkbox state] == NSControlStateValueOn))
    return MESSAGE_BOX_RESULT_YES;

  return MESSAGE_BOX_RESULT_NO;
}

}  // namespace chrome
