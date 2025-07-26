// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_OMNIBOX_POPUP_PROVIDER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_OMNIBOX_POPUP_PROVIDER_H_

#import <UIKit/UIKit.h>

/// An interface for a provider of an omnibox popup. Allows to get information
/// about the state of and configure the popup.
class OmniboxPopupProvider {
 public:
  virtual bool IsPopupOpen() = 0;
  virtual void SetTextAlignment(NSTextAlignment alignment) = 0;
  virtual void SetSemanticContentAttribute(
      UISemanticContentAttribute semanticContentAttrbute) = 0;
  virtual void SetHasThumbnail(bool has_thumbnail) = 0;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_OMNIBOX_POPUP_PROVIDER_H_
