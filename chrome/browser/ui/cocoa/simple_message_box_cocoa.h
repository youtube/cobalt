// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SIMPLE_MESSAGE_BOX_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_SIMPLE_MESSAGE_BOX_COCOA_H_

#include "chrome/browser/ui/simple_message_box.h"

namespace chrome {

MessageBoxResult ShowMessageBoxCocoa(const std::u16string& message,
                                     MessageBoxType type,
                                     const std::u16string& checkbox_text);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_SIMPLE_MESSAGE_BOX_COCOA_H_
