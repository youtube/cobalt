// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_stat.h"

@implementation LevelUpStat

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
              imageLottieName:(NSString*)imageLottieName
                         type:(LevelUpTaskStatType)type {
  self = [super init];
  if (self) {
    _title = title;
    _subtitle = subtitle;
    _imageLottieName = imageLottieName;
    _type = type;
  }
  return self;
}

@end
