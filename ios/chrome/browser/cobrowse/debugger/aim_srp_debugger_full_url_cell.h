// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_FULL_URL_CELL_H_
#define IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_FULL_URL_CELL_H_

#import <UIKit/UIKit.h>

// Cell to display and edit the full URL.
@interface AIMSRPDebuggerFullURLCell : UITableViewCell

@property(nonatomic, readonly) UITextView* textView;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_FULL_URL_CELL_H_
