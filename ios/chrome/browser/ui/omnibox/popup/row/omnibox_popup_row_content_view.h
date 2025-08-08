// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/row/omnibox_popup_row_content_configuration.h"

/// Content View of the omnibox popup row, contains the view and layout of the
/// UI.
@interface OmniboxPopupRowContentView : UIView <UIContentView>

/// Returns the current configuration of the view. Setting this property applies
/// the new configuration to the view.
@property(nonatomic, copy) OmniboxPopupRowContentConfiguration* configuration;

/// Initializes OmniboxPopupRowContentView with `configuration`.
- (instancetype)initWithConfiguration:
    (OmniboxPopupRowContentConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_ROW_OMNIBOX_POPUP_ROW_CONTENT_VIEW_H_
