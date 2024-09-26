// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_BASE_VIEW_H_
#define UI_BASE_COCOA_BASE_VIEW_H_

#import <Cocoa/Cocoa.h>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"
#import "ui/base/cocoa/tracking_area.h"
#include "ui/gfx/geometry/rect.h"

// A view that provides common functionality that many views will need:
// - Automatic registration for mouse-moved events.
// - Funneling of mouse and key events to two methods
// - Coordinate conversion utilities
COMPONENT_EXPORT(UI_BASE)
@interface BaseView : NSView {
 @public
  enum EventHandled {
    kEventNotHandled,
    kEventHandled
  };

 @private
  ui::ScopedCrTrackingArea _trackingArea;
  BOOL _dragging;
  base::scoped_nsobject<NSEvent> _pendingExitEvent;
  NSInteger _pressureEventStage;
}

// Process an NSEventTypeLeftMouseUp event on this view that wasn't dispatched
// already to BaseView (e.g. if captured via an event monitor). This may
// generate a synthetic NSEventTypeMouseExited if the mouse exited the view area
// during a drag and should be invoked after a call to -mouseEvent: for the
// mouse up.
- (void)handleLeftMouseUp:(NSEvent*)theEvent;

// Override these methods (mouseEvent, keyEvent, forceTouchEvent) in a
// subclass.
- (void)mouseEvent:(NSEvent *)theEvent;

// keyEvent should return kEventHandled if it handled the event, or
// kEventNotHandled if it should be forwarded to BaseView's super class.
- (EventHandled)keyEvent:(NSEvent *)theEvent;

- (void)forceTouchEvent:(NSEvent*)theEvent;

// Useful rect conversions (doing coordinate flipping)
- (gfx::Rect)flipNSRectToRect:(NSRect)rect;
- (NSRect)flipRectToNSRect:(gfx::Rect)rect;

@end

// A notification that a view may issue when it receives first responder status.
// The name is |kViewDidBecomeFirstResponder|, the object is the view, and the
// NSSelectionDirection is wrapped in an NSNumber under the key
// |kSelectionDirection|.
COMPONENT_EXPORT(UI_BASE) extern NSString* kViewDidBecomeFirstResponder;
COMPONENT_EXPORT(UI_BASE) extern NSString* kSelectionDirection;

#endif  // UI_BASE_COCOA_BASE_VIEW_H_
