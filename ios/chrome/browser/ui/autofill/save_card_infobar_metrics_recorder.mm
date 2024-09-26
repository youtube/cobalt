// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/save_card_infobar_metrics_recorder.h"

#import "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SaveCardInfobarMetricsRecorder

+ (void)recordModalEvent:(MobileMessagesSaveCardModalEvent)event {
  UMA_HISTOGRAM_ENUMERATION("Mobile.Messages.Save.Card.Modal.Event", event);
}

@end
