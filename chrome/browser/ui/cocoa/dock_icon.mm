// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dock_icon.h"

#include <stdint.h>

#include "base/check_op.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_nsobject.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

using content::BrowserThread;

namespace {

// The fraction of the size of the dock icon that the badge is.
constexpr CGFloat kBadgeFraction = 0.375f;
constexpr CGFloat kBadgeMargin = 4;
constexpr CGFloat kBadgeStrokeWidth = 6;

constexpr struct {
  CGFloat offset, radius, opacity;
} kBadgeShadows[] = {
    {0, 2, 0.14},
    {2, 2, 0.12},
    {1, 3, 0.2},
};

// The maximum update rate for the dock icon. 200ms = 5fps.
constexpr int64_t kUpdateFrequencyMs = 200;

}  // namespace

// A view that draws our dock tile.
@interface DockTileView : NSView {
 @private
  int _downloads;
  BOOL _indeterminate;
  float _progress;
}

// Indicates how many downloads are in progress.
@property (nonatomic) int downloads;

// Indicates whether the progress indicator should be in an indeterminate state
// or not.
@property (nonatomic) BOOL indeterminate;

// Indicates the amount of progress made of the download. Ranges from [0..1].
@property (nonatomic) float progress;

@end

@implementation DockTileView

@synthesize downloads = _downloads;
@synthesize indeterminate = _indeterminate;
@synthesize progress = _progress;

- (void)drawRect:(NSRect)dirtyRect {
  // Not -[NSApplication applicationIconImage]; that fails to return a pasted
  // custom icon.
  NSString* appPath = [base::mac::MainBundle() bundlePath];
  NSImage* appIcon = [[NSWorkspace sharedWorkspace] iconForFile:appPath];
  [appIcon drawInRect:[self bounds]
             fromRect:NSZeroRect
            operation:NSCompositingOperationSourceOver
             fraction:1.0];

  if (_downloads == 0)
    return;

  const CGFloat badgeSize = NSWidth(self.bounds) * kBadgeFraction;
  const NSRect badgeRect =
      NSMakeRect(NSMaxX(self.bounds) - badgeSize - kBadgeMargin, kBadgeMargin,
                 badgeSize, badgeSize);
  const CGFloat badgeRadius = badgeSize / 2;
  const NSPoint badgeCenter = NSMakePoint(NSMidX(badgeRect), NSMidY(badgeRect));

  NSBezierPath* backgroundPath =
      [NSBezierPath bezierPathWithOvalInRect:badgeRect];
  [[NSColor clearColor] setFill];

  base::scoped_nsobject<NSShadow> shadow([[NSShadow alloc] init]);
  shadow.get().shadowColor = [NSColor blackColor];
  for (const auto shadowProps : kBadgeShadows) {
    gfx::ScopedNSGraphicsContextSaveGState scopedGState;
    shadow.get().shadowOffset = NSMakeSize(0, -shadowProps.offset);
    shadow.get().shadowBlurRadius = shadowProps.radius;
    [[NSColor colorWithCalibratedWhite:0 alpha:shadowProps.opacity] setFill];
    [shadow set];
    [backgroundPath fill];
  }

  [[NSColor colorWithCalibratedRed:0xec / 255.0
                             green:0xf3 / 255.0
                              blue:0xfe / 255.0
                             alpha:1] setFill];
  [backgroundPath fill];

  // Stroke
  if (!_indeterminate) {
    NSBezierPath* strokePath;
    if (_progress >= 1.0) {
      strokePath = [NSBezierPath bezierPathWithOvalInRect:badgeRect];
    } else {
      CGFloat endAngle = 90.0 - 360.0 * _progress;
      if (endAngle < 0.0)
        endAngle += 360.0;
      strokePath = [NSBezierPath bezierPath];
      [strokePath
          appendBezierPathWithArcWithCenter:badgeCenter
                                     radius:badgeRadius - kBadgeStrokeWidth / 2
                                 startAngle:90.0
                                   endAngle:endAngle
                                  clockwise:YES];
    }
    [strokePath setLineWidth:kBadgeStrokeWidth];
    [[NSColor colorWithCalibratedRed:0x42 / 255.0
                               green:0x85 / 255.0
                                blue:0xf4 / 255.0
                               alpha:1] setStroke];
    [strokePath stroke];
  }

  // Download count
  base::scoped_nsobject<NSNumberFormatter> formatter(
      [[NSNumberFormatter alloc] init]);
  NSString* countString = [formatter stringFromNumber:@(_downloads)];

  CGFloat countFontSize = 24;
  NSSize countSize = NSZeroSize;
  base::scoped_nsobject<NSAttributedString> countAttrString;
  while (1) {
    NSFont* countFont = [NSFont systemFontOfSize:countFontSize
                                          weight:NSFontWeightMedium];

    // This will generally be plain Helvetica.
    if (!countFont)
      countFont = [NSFont userFontOfSize:countFontSize];

    // Continued failure would generate an NSException.
    if (!countFont)
      break;

    countAttrString.reset([[NSAttributedString alloc]
        initWithString:countString
            attributes:@{
              NSForegroundColorAttributeName :
                  [NSColor colorWithCalibratedWhite:0 alpha:0.65],
              NSFontAttributeName : countFont,
            }]);
    countSize = [countAttrString size];
    if (countSize.width > (badgeRadius - kBadgeStrokeWidth) * 1.5) {
      countFontSize -= 1.0;
    } else {
      break;
    }
  }

  NSPoint countOrigin = badgeCenter;
  countOrigin.x -= countSize.width / 2;
  countOrigin.y -= countSize.height / 2;

  [countAttrString.get() drawAtPoint:countOrigin];
}

@end


@implementation DockIcon

+ (DockIcon*)sharedDockIcon {
  static DockIcon* icon;
  if (!icon) {
    NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];

    base::scoped_nsobject<DockTileView> dockTileView(
        [[DockTileView alloc] init]);
    [dockTile setContentView:dockTileView];

    icon = [[DockIcon alloc] init];
  }

  return icon;
}

- (void)updateIcon {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::TimeDelta updateFrequency =
      base::Milliseconds(kUpdateFrequencyMs);

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta timeSinceLastUpdate = now - _lastUpdate;
  if (!_forceUpdate && timeSinceLastUpdate < updateFrequency)
    return;

  _lastUpdate = now;
  _forceUpdate = NO;

  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];

  [dockTile display];
}

- (void)setDownloads:(int)downloads {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];
  DockTileView* dockTileView = (DockTileView*)([dockTile contentView]);

  if (downloads != [dockTileView downloads]) {
    [dockTileView setDownloads:downloads];
    _forceUpdate = YES;
  }
}

- (void)setIndeterminate:(BOOL)indeterminate {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];
  DockTileView* dockTileView = (DockTileView*)([dockTile contentView]);

  if (indeterminate != [dockTileView indeterminate]) {
    [dockTileView setIndeterminate:indeterminate];
    _forceUpdate = YES;
  }
}

- (void)setProgress:(float)progress {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  NSDockTile* dockTile = [[NSApplication sharedApplication] dockTile];
  DockTileView* dockTileView = (DockTileView*)([dockTile contentView]);

  [dockTileView setProgress:progress];
}

@end
