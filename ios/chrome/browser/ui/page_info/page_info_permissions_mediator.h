// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PERMISSIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PERMISSIONS_MEDIATOR_H_

#import "ios/chrome/browser/permissions/ui_bundled/permissions_delegate.h"

@protocol PermissionsConsumer;

namespace web {
class WebState;
}

// Mediator for the page info permissions.
@interface PageInfoPermissionsMediator : NSObject <PermissionsDelegate>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer that reads information from `webState` to establish
// the property.
- (instancetype)initWithWebState:(web::WebState*)webState;

// Consumer that is configured by this mediator.
@property(nonatomic, weak) id<PermissionsConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PERMISSIONS_MEDIATOR_H_
