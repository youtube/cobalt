// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_

#import <Foundation/Foundation.h>

namespace at_memory {

// Represents the different content states of the AtMemory screen.
enum class AtMemoryContentState {
  // Shows the empty state view.
  kEmpty,
  // Shows previously filled items.
  kPreviouslyFilled,
  // Shows search cell (including loading state).
  kSearch,
  // Shows search results.
  kSearchResults,
  // Shows that the query is unsupported.
  kQueryUnsupported,
  // Shows a "no data" message.
  kNoData,
};

}  // namespace at_memory

// Consumer for AtMemory.
@protocol AtMemoryConsumer <NSObject>

// Sets the current content state of the AtMemory screen.
- (void)setContentState:(at_memory::AtMemoryContentState)contentState;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_
