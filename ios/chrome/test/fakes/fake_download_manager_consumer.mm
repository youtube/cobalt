// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_download_manager_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeDownloadManagerConsumer
@synthesize fileName = _fileName;
@synthesize countOfBytesReceived = _countOfBytesReceived;
@synthesize countOfBytesExpectedToReceive = _countOfBytesExpectedToReceive;
@synthesize progress = _progress;
@synthesize state = _state;
@synthesize installDriveButtonVisible = _installDriveButtonVisible;

- (void)setInstallDriveButtonVisible:(BOOL)visible animated:(BOOL)animated {
  _installDriveButtonVisible = visible;
}

@end
