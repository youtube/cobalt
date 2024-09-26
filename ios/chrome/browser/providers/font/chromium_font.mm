// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/font/font_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

UIFont* GetBrandedProductRegularFont(CGFloat size) {
  // Chromium does not have branded fonts, so return system default instead.
  return [UIFont systemFontOfSize:size];
}

}  // namespace provider
}  // namespace ios
