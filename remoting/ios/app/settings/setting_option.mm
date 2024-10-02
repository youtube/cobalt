// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/settings/setting_option.h"

@implementation SettingOption

@synthesize title = _title;
@synthesize subtext = _subtext;
@synthesize action = _action;
@synthesize checked = _checked;
@synthesize style = _style;

@end
