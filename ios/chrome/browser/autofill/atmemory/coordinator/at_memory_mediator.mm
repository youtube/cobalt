// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"

@implementation AtMemoryMediator

- (void)setConsumer:(id<AtMemoryConsumer>)consumer {
  _consumer = consumer;
  [consumer setContentState:at_memory::AtMemoryContentState::kEmpty];
}

@end
