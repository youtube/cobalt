// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_generator.h"

#import <algorithm>

// TODO(crbug.com/636188): required to implement ViewHierarchyContainsWebView
// for -drawViewHierarchyInRect:afterScreenUpdates:, remove once the workaround
// is no longer needed.
#import <WebKit/WebKit.h>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "build/blink_buildflags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/gfx/geometry/rect_f.h"
#import "ui/gfx/image/image.h"

namespace {

// Contains information needed for snapshotting.
struct SnapshotInfo {
  UIView* baseView;
  CGRect snapshotFrameInBaseView;
  CGRect snapshotFrameInWindow;
  NSArray<UIView*>* overlays;
};

// Returns YES if `view` or any view it contains is a WKWebView.
BOOL ViewHierarchyContainsWebView(UIView* view) {
  if ([view isKindOfClass:[WKWebView class]]) {
    return YES;
  }
#if BUILDFLAG(USE_BLINK)
  // TODO(crbug.com/1419001): Remove NSClassFromString and use the class
  // directly when possible.
  if ([view isKindOfClass:[NSClassFromString(@"RenderWidgetUIView") class]]) {
    return YES;
  }
#endif  // USE_BLINK
  for (UIView* subview in view.subviews) {
    if (ViewHierarchyContainsWebView(subview)) {
      return YES;
    }
  }
  return NO;
}

}  // namespace

@interface SnapshotGenerator () <CRWWebStateObserver>
@end

@implementation SnapshotGenerator {
  // The associated WebState.
  web::WebState* _webState;

  // Bridge object allowing to observe the WebState.
  std::unique_ptr<web::WebStateObserver> _webStateObserver;

  // The unique ID for WebState's snapshot.
  SnapshotID _snapshotID;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                      snapshotID:(SnapshotID)snapshotID {
  if ((self = [super init])) {
    DCHECK(webState);
    DCHECK(snapshotID.valid());
    _webState = webState;
    _snapshotID = snapshotID;

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);
  if (_snapshotStorage) {
    [_snapshotStorage retrieveImageForSnapshotID:_snapshotID callback:callback];
  } else {
    callback(nil);
  }
}

- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);

  __weak SnapshotGenerator* weakSelf = self;
  void (^wrappedCallback)(UIImage*) = ^(UIImage* image) {
    if (!image) {
      image = [weakSelf updateSnapshot];
      if (image) {
        image = GreyImage(image);
      }
    }
    callback(image);
  };

  SnapshotStorage* snapshotStorage = _snapshotStorage;
  if (snapshotStorage) {
    [snapshotStorage retrieveGreyImageForSnapshotID:_snapshotID
                                           callback:wrappedCallback];
  } else {
    wrappedCallback(nil);
  }
}

- (UIImage*)updateSnapshot {
  UIImage* snapshot = [self generateSnapshotWithOverlays:YES];
  [self updateSnapshotStorageWithImage:snapshot];
  return snapshot;
}

- (void)updateWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion {
  DCHECK(
      !web::GetWebClient()->IsAppSpecificURL(_webState->GetLastCommittedURL()));

  if (![self canTakeSnapshot]) {
    if (completion) {
      web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                                 completion(nil);
                                               }));
    }
    return;
  }
  SnapshotInfo snapshotInfo = [self snapshotInfo];
  CGRect snapshotFrameInWebView =
      [_webState->GetView() convertRect:snapshotInfo.snapshotFrameInBaseView
                               fromView:snapshotInfo.baseView];
  [_delegate snapshotGenerator:self willUpdateSnapshotForWebState:_webState];
  __weak SnapshotGenerator* weakSelf = self;
  _webState->TakeSnapshot(
      gfx::RectF(snapshotFrameInWebView),
      base::BindRepeating(^(const gfx::Image& image) {
        UIImage* snapshot = nil;
        if (!image.IsEmpty()) {
          snapshot = [weakSelf
              snapshotWithOverlays:snapshotInfo.overlays
                         baseImage:image.ToUIImage()
                     frameInWindow:snapshotInfo.snapshotFrameInWindow];
        }
        [weakSelf updateSnapshotStorageWithImage:snapshot];
        if (completion) {
          completion(snapshot);
        }
      }));
}

- (UIImage*)generateSnapshotWithOverlays:(BOOL)shouldAddOverlay {
  if (![self canTakeSnapshot]) {
    return nil;
  }
  SnapshotInfo snapshotInfo = [self snapshotInfo];
  [_delegate snapshotGenerator:self willUpdateSnapshotForWebState:_webState];
  UIImage* baseImage =
      [self snapshotBaseView:snapshotInfo.baseView
             frameInBaseView:snapshotInfo.snapshotFrameInBaseView];
  return [self
      snapshotWithOverlays:(shouldAddOverlay ? snapshotInfo.overlays : nil)
                 baseImage:baseImage
             frameInWindow:snapshotInfo.snapshotFrameInWindow];
}

- (void)willBeSavedGreyWhenBackgrounding {
  [_snapshotStorage willBeSavedGreyWhenBackgrounding:_snapshotID];
}

- (void)saveGreyInBackground {
  [_snapshotStorage saveGreyInBackgroundForSnapshotID:_snapshotID];
}

- (void)removeSnapshot {
  [_snapshotStorage removeImageWithSnapshotID:_snapshotID];
}

#pragma mark - Private methods

// Returns NO if WebState or the view is not ready for snapshot.
- (BOOL)canTakeSnapshot {
  // This allows for easier unit testing of classes that use SnapshotGenerator.
  if (!_delegate) {
    return NO;
  }

  // Do not generate a snapshot if web usage is disabled (as the WebState's
  // view is blank in that case).
  if (!_webState->IsWebUsageEnabled()) {
    return NO;
  }

  return [_delegate snapshotGenerator:self
           canTakeSnapshotForWebState:_webState];
}

// Returns a snapshot of `baseView` with `frameInBaseView`.
- (UIImage*)snapshotBaseView:(UIView*)baseView
             frameInBaseView:(CGRect)frameInBaseView {
  DCHECK(baseView);
  DCHECK(!CGRectIsEmpty(frameInBaseView));
  // Ideally, generate an UIImage by one step with `UIGraphicsImageRenderer`,
  // however, it generates a black image when the size of `baseView` is larger
  // than `frameInBaseView`. So this is a workaround to generate an UIImage by
  // dividing the step into 2 steps; 1) convert an UIView to an UIImage 2) crop
  // an UIImage with `frameInBaseView`.
  UIImage* baseImage = [self convertFromBaseView:baseView];
  return [self cropImage:baseImage frameInBaseView:frameInBaseView];
}

// Converts an UIView to an UIImage. The size of generated UIImage is the same
// as `baseView`.
- (UIImage*)convertFromBaseView:(UIView*)baseView {
  DCHECK(baseView);

  // Disable the automatic view dimming UIKit performs if a view is presented
  // modally over `baseView`.
  baseView.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;

  // Note: When not using device scale, the output image size may slightly
  // differ from the input size due to rounding.
  const CGFloat kScale =
      std::max<CGFloat>(1.0, [_snapshotStorage snapshotScaleForDevice]);
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = kScale;
  format.opaque = YES;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithBounds:baseView.bounds
                                               format:format];

  __block BOOL snapshotSuccess = YES;
  UIImage* image =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* UIContext) {
        if (baseView.window && ViewHierarchyContainsWebView(baseView)) {
          // `-renderInContext:` is the preferred way to render a snapshot, but
          // it's buggy for WKWebView, which is used for some WebUI pages such
          // as "No internet" or "Site can't be reached". If a
          // WKWebView-containing hierarchy must be snapshotted, the UIView
          // `-drawViewHierarchyInRect:` method is used instead.
          // `drawViewHierarchyInRect:` has undefined behavior when the view is
          // not in the visible view hierarchy. In practice, when this method is
          // called on a view that is part of view controller containment and
          // not in the view hierarchy, an
          // UIViewControllerHierarchyInconsistency exception will be thrown.
          // TODO(crbug.com/636188):
          // `-drawViewHierarchyInRect:afterScreenUpdates:` is buggy causing GPU
          // glitches, screen redraws during animations, broken pinch to dismiss
          // on tablet, etc.
          snapshotSuccess = [baseView drawViewHierarchyInRect:baseView.bounds
                                           afterScreenUpdates:YES];
        } else {
          // Render the view's layer via `-renderInContext:`.
          // To mitigate against crashes like crbug.com/1429512, ensure that
          // the layer's position is valid. If not, mark the snapshotting as
          // failed.
          CALayer* layer = baseView.layer;
          CGPoint pos = layer.position;
          if (isnan(pos.x) || isnan(pos.y)) {
            snapshotSuccess = NO;
          } else {
            [layer renderInContext:UIContext.CGContext];
          }
        }
      }];

  if (!snapshotSuccess) {
    image = nil;
  }

  // Set the mode to UIViewTintAdjustmentModeAutomatic.
  baseView.tintAdjustmentMode = UIViewTintAdjustmentModeAutomatic;

  return image;
}

// Crops an UIImage to `frameInBaseView`.
- (UIImage*)cropImage:(UIImage*)baseImage
      frameInBaseView:(CGRect)frameInBaseView {
  if (!baseImage) {
    return nil;
  }
  DCHECK(!CGRectIsEmpty(frameInBaseView));

  // Scale `frameInBaseView` to handle an image with 2x scale.
  CGFloat scale = baseImage.scale;
  frameInBaseView.origin.x *= scale;
  frameInBaseView.origin.y *= scale;
  frameInBaseView.size.width *= scale;
  frameInBaseView.size.height *= scale;

  // Perform cropping.
  CGImageRef imageRef =
      CGImageCreateWithImageInRect(baseImage.CGImage, frameInBaseView);

  // Convert back to an UIImage.
  UIImage* image = [UIImage imageWithCGImage:imageRef
                                       scale:scale
                                 orientation:baseImage.imageOrientation];

  // Clean up a reference pointer.
  CGImageRelease(imageRef);

  return image;
}

// Returns an image of the `baseImage` overlaid with `overlays` with the given
// `frameInWindow`.
- (UIImage*)snapshotWithOverlays:(NSArray<UIView*>*)overlays
                       baseImage:(UIImage*)baseImage
                   frameInWindow:(CGRect)frameInWindow {
  DCHECK(!CGRectIsEmpty(frameInWindow));
  if (!baseImage) {
    return nil;
  }
  // Note: If the baseImage scale differs from device scale, the baseImage size
  // may slightly differ from frameInWindow size due to rounding. Do not attempt
  // to compare the baseImage size and frameInWindow size.
  if (overlays.count == 0) {
    return baseImage;
  }
  const CGFloat kScale =
      std::max<CGFloat>(1.0, [_snapshotStorage snapshotScaleForDevice]);

  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = kScale;
  format.opaque = YES;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:frameInWindow.size
                                             format:format];

  return
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* UIContext) {
        CGContextRef context = UIContext.CGContext;

        // The base image is already a cropped snapshot so it is drawn at the
        // origin of the new image.
        [baseImage drawInRect:(CGRect){.origin = CGPointZero,
                                       .size = frameInWindow.size}];

        // This shifts the origin of the context so that future drawings can be
        // in window coordinates. For example, suppose that the desired snapshot
        // area is at (0, 99) in the window coordinate space. Drawing at (0, 99)
        // will appear as (0, 0) in the resulting image.
        CGContextTranslateCTM(context, -frameInWindow.origin.x,
                              -frameInWindow.origin.y);
        [self drawOverlays:overlays context:context];
      }];
}

// Updates the snapshot storage with `snapshot`.
- (void)updateSnapshotStorageWithImage:(UIImage*)snapshot {
  if (snapshot) {
    [_snapshotStorage setImage:snapshot withSnapshotID:_snapshotID];
  } else {
    // Remove any stale snapshot since the snapshot failed.
    [_snapshotStorage removeImageWithSnapshotID:_snapshotID];
  }
}

// Draws `overlays` onto `context` at offsets relative to the window.
- (void)drawOverlays:(NSArray<UIView*>*)overlays context:(CGContext*)context {
  for (UIView* overlay in overlays) {
    CGContextSaveGState(context);
    CGRect frameInWindow = [overlay.superview convertRect:overlay.frame
                                                   toView:nil];
    // This shifts the context so that drawing starts at the overlay's offset.
    CGContextTranslateCTM(context, frameInWindow.origin.x,
                          frameInWindow.origin.y);
    [[overlay layer] renderInContext:context];
    CGContextRestoreGState(context);
  }
}

// Retrieves information needed for snapshotting.
- (SnapshotInfo)snapshotInfo {
  SnapshotInfo snapshotInfo;
  snapshotInfo.baseView = [_delegate snapshotGenerator:self
                                   baseViewForWebState:_webState];
  DCHECK(snapshotInfo.baseView);
  UIEdgeInsets baseViewInsets = [_delegate snapshotGenerator:self
                               snapshotEdgeInsetsForWebState:_webState];
  snapshotInfo.snapshotFrameInBaseView =
      UIEdgeInsetsInsetRect(snapshotInfo.baseView.bounds, baseViewInsets);
  DCHECK(!CGRectIsEmpty(snapshotInfo.snapshotFrameInBaseView));
  snapshotInfo.snapshotFrameInWindow =
      [snapshotInfo.baseView convertRect:snapshotInfo.snapshotFrameInBaseView
                                  toView:nil];
  DCHECK(!CGRectIsEmpty(snapshotInfo.snapshotFrameInWindow));
  snapshotInfo.overlays = [_delegate snapshotGenerator:self
                           snapshotOverlaysForWebState:_webState];
  return snapshotInfo;
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webState = nullptr;
}

@end
