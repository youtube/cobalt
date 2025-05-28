// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;
@class MagicStackContextMenuInteractionHandler;
@class MagicStackModule;
@protocol MagicStackModuleContainerDelegate;

// Container View for a module in the Magic Stack.
@interface MagicStackModuleContainer : UIView

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Configures this container with `config`.
- (void)configureWithConfig:(MagicStackModule*)config;

// Reset the main configurations of the view.
- (void)resetView;

// Handler for magic stack context menu. Only available after
// `configureWithConfig:` has been invoked.
- (MagicStackContextMenuInteractionHandler*)contextMenuInteractionHandler;

// Delegate for this container.
@property(nonatomic, weak) id<MagicStackModuleContainerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_MODULE_CONTAINER_H_
