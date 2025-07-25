// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PERMISSIONS_PERMISSIONS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_PERMISSIONS_PERMISSIONS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

API_AVAILABLE(ios(15.0))
@interface PermissionsAppInterface : NSObject

// Returns a dictionary of permissions and their states of the currently active
// web state; if no web state is active, returns an empty dictionary.
+ (NSDictionary<NSNumber*, NSNumber*>*)statesForAllPermissions;

@end

#endif  // IOS_CHROME_BROWSER_UI_PERMISSIONS_PERMISSIONS_APP_INTERFACE_H_
