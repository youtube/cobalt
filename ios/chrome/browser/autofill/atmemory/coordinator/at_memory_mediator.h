// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol AtMemoryConsumer;

// Mediator for AtMemory.
@interface AtMemoryMediator : NSObject

// The consumer for this mediator.
@property(nonatomic, weak) id<AtMemoryConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_AT_MEMORY_MEDIATOR_H_
