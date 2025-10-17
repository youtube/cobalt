// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GLIC_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GLIC_COMMANDS_H_

// Commands relating to the Glic flow.
@protocol GlicCommands

// Starts the Glic flow.
- (void)startGlicFlow;

// Dismiss the Glic flow.
- (void)dismissGlicFlow;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_GLIC_COMMANDS_H_
