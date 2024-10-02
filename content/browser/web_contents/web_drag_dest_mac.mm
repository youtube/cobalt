// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_drag_dest_mac.h"

#include <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

#include "base/mac/scoped_nsobject.h"
#include "base/ranges/algorithm.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/web_contents_ns_view_bridge.mojom.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "content/public/common/drop_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"

using blink::DragOperationsMask;
using content::DropData;
using content::OpenURLParams;
using content::Referrer;
using content::WebContentsImpl;
using remote_cocoa::mojom::DraggingInfo;

namespace content {

DropContext::DropContext(const DropData drop_data,
                         const gfx::PointF client_pt,
                         const gfx::PointF screen_pt,
                         int modifier_flags,
                         base::WeakPtr<RenderWidgetHostImpl> target_rwh)
    : drop_data(drop_data),
      client_pt(client_pt),
      screen_pt(screen_pt),
      modifier_flags(modifier_flags),
      target_rwh(target_rwh) {}

DropContext::DropContext(const DropContext& other) = default;
DropContext::DropContext(DropContext&& other) = default;

DropContext::~DropContext() = default;

}  // namespace content

namespace {

int GetModifierFlags() {
  int modifier_state = 0;
  UInt32 currentModifiers = GetCurrentKeyModifiers();
  if (currentModifiers & ::shiftKey)
    modifier_state |= blink::WebInputEvent::kShiftKey;
  if (currentModifiers & ::controlKey)
    modifier_state |= blink::WebInputEvent::kControlKey;
  if (currentModifiers & ::optionKey)
    modifier_state |= blink::WebInputEvent::kAltKey;
  if (currentModifiers & ::cmdKey)
    modifier_state |= blink::WebInputEvent::kMetaKey;

  // The return value of 1 << 0 corresponds to the left mouse button,
  // 1 << 1 corresponds to the right mouse button,
  // 1 << n, n >= 2 correspond to other mouse buttons.
  NSUInteger pressedButtons = [NSEvent pressedMouseButtons];

  if (pressedButtons & (1 << 0))
    modifier_state |= blink::WebInputEvent::kLeftButtonDown;
  if (pressedButtons & (1 << 1))
    modifier_state |= blink::WebInputEvent::kRightButtonDown;
  if (pressedButtons & (1 << 2))
    modifier_state |= blink::WebInputEvent::kMiddleButtonDown;

  return modifier_state;
}

content::GlobalRoutingID GetRenderViewHostID(content::RenderViewHost* rvh) {
  return content::GlobalRoutingID(rvh->GetProcess()->GetID(),
                                  rvh->GetRoutingID());
}

void DropCompletionCallback(WebDragDest* drag_dest,
                            const content::DropContext context,
                            absl::optional<content::DropData> drop_data) {
  // This is an async callback. Make sure RWH is still valid.
  if (!context.target_rwh)
    return;

  [drag_dest completeDropAsync:drop_data withContext:context];
}

}  // namespace

@implementation WebDragDest

// |contents| is the WebContentsImpl representing this tab, used to communicate
// drag&drop messages to WebCore and handle navigation on a successful drop
// (if necessary).
- (id)initWithWebContentsImpl:(WebContentsImpl*)contents {
  if ((self = [super init])) {
    _webContents = contents;
    _canceled = false;
    _dragStartProcessID = content::ChildProcessHost::kInvalidUniqueID;
    _dragStartViewID = content::GlobalRoutingID(
        content::ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
  }
  return self;
}

- (DropData*)currentDropData {
  return _dropDataFiltered.get();
}

- (void)setDragDelegate:(content::WebDragDestDelegate*)delegate {
  _delegate = delegate;
}

// Call to set whether or not we should allow the drop. Takes effect the
// next time |-draggingUpdated:| is called.
- (void)setCurrentOperation:(NSDragOperation)operation {
  _currentOperation = operation;
}

// Given a point in window coordinates and a view in that window, return a
// flipped point in the coordinate system of |view|.
- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint
                            view:(NSView*)view {
  DCHECK(view);
  NSPoint viewPoint =  [view convertPoint:windowPoint fromView:nil];
  NSRect viewFrame = [view frame];
  viewPoint.y = viewFrame.size.height - viewPoint.y;
  return viewPoint;
}

// Given a point in window coordinates and a view in that window, return a
// flipped point in screen coordinates.
- (NSPoint)flipWindowPointToScreen:(const NSPoint&)windowPoint
                              view:(NSView*)view {
  DCHECK(view);
  NSPoint screenPoint = [view.window convertPointToScreen:windowPoint];
  NSRect screenFrame = view.window.screen.frame;
  screenPoint.y = screenFrame.size.height - screenPoint.y;
  return screenPoint;
}

// Messages to send during the tracking of a drag, usually upon receiving
// calls from the view system. Communicates the drag messages to WebCore.

- (void)setDropData:(const DropData&)dropData {
  _dropDataUnfiltered = std::make_unique<DropData>(dropData);
}

- (NSDragOperation)draggingEntered:(const DraggingInfo*)info {
  if (_webContents->ShouldIgnoreInputEvents())
    return NSDragOperationNone;

  // Save off the RVH so we can tell if it changes during a drag. If it does,
  // we need to send a new enter message in draggingUpdated:.
  _currentRVH = _webContents->GetRenderViewHost();

  gfx::PointF transformedPt;
  if (!_webContents->GetRenderWidgetHostView()) {
    // TODO(ekaramad, paulmeyer): Find a better way than toggling |canceled_|.
    // This could happen when the renderer process for the top-level RWH crashes
    // (see https://crbug.com/670645).
    _canceled = true;
    return NSDragOperationNone;
  }

  content::RenderWidgetHostImpl* targetRWH =
      [self GetRenderWidgetHostAtPoint:info->location_in_view
                         transformedPt:&transformedPt];
  if (![self isValidDragTarget:targetRWH])
    return NSDragOperationNone;

  // Filter |dropDataUnfiltered_| by currentRWHForDrag_ to populate
  // |dropDataFiltered_|.
  DCHECK(_dropDataUnfiltered);
  std::unique_ptr<DropData> dropData =
      std::make_unique<DropData>(*_dropDataUnfiltered);
  _currentRWHForDrag = targetRWH->GetWeakPtr();
  _currentRWHForDrag->FilterDropData(dropData.get());

  NSDragOperation mask = info->operation_mask;

  // Give the delegate an opportunity to cancel the drag.
  if (auto* delegate = _webContents->GetDelegate()) {
    _canceled = !delegate->CanDragEnter(_webContents, *dropData,
                                        static_cast<DragOperationsMask>(mask));
  }

  if (_canceled)
    return NSDragOperationNone;

  if (_delegate) {
    _delegate->DragInitialize(_webContents);
    _delegate->OnDragEnter();
  }

  _dropDataFiltered.swap(dropData);

  _currentRWHForDrag->DragTargetDragEnter(
      *_dropDataFiltered, transformedPt, info->location_in_screen,
      static_cast<DragOperationsMask>(mask), GetModifierFlags(),
      base::DoNothing());

  // We won't know the true operation (whether the drag is allowed) until we
  // hear back from the renderer. For now, be optimistic:
  _currentOperation = NSDragOperationCopy;
  return _currentOperation;
}

- (void)draggingExited {
  if (_webContents->ShouldIgnoreInputEvents())
    return;

  if (!_dropDataFiltered || !_dropDataUnfiltered)
    return;

  DCHECK(_currentRVH);
  if (_currentRVH != _webContents->GetRenderViewHost())
    return;

  if (_canceled)
    return;

  if (_delegate)
    _delegate->OnDragLeave();

  if (_currentRWHForDrag) {
    _currentRWHForDrag->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
    _currentRWHForDrag.reset();
  }
  _dropDataUnfiltered.reset();
  _dropDataFiltered.reset();
}

- (NSDragOperation)draggingUpdated:(const DraggingInfo*)info {
  if (_webContents->ShouldIgnoreInputEvents())
    return NSDragOperationNone;

  if (!_dropDataFiltered || !_dropDataUnfiltered)
    return NSDragOperationNone;

  if (_canceled) {
    // TODO(ekaramad,paulmeyer): We probably shouldn't be checking for
    // |canceled_| twice in this method.
    return NSDragOperationNone;
  }

  gfx::PointF transformedPt;
  content::RenderWidgetHostImpl* targetRWH =
      [self GetRenderWidgetHostAtPoint:info->location_in_view
                         transformedPt:&transformedPt];

  if (![self isValidDragTarget:targetRWH])
    return NSDragOperationNone;

  // TODO(paulmeyer): The dragging delegates may now by invoked multiple times
  // per drag, even without the drag ever leaving the window.
  if (targetRWH != _currentRWHForDrag.get()) {
    if (_currentRWHForDrag) {
      gfx::PointF transformedLeavePoint = info->location_in_view;
      gfx::PointF transformedScreenPoint = info->location_in_screen;
      content::RenderWidgetHostViewBase* rootView =
          static_cast<content::RenderWidgetHostViewBase*>(
              _webContents->GetRenderWidgetHostView());
      content::RenderWidgetHostViewBase* currentDragView =
          static_cast<content::RenderWidgetHostViewBase*>(
              _currentRWHForDrag->GetView());
      rootView->TransformPointToCoordSpaceForView(
          transformedLeavePoint, currentDragView, &transformedLeavePoint);
      rootView->TransformPointToCoordSpaceForView(
          transformedScreenPoint, currentDragView, &transformedScreenPoint);
      _currentRWHForDrag->DragTargetDragLeave(transformedLeavePoint,
                                              transformedScreenPoint);
    }
    [self draggingEntered:info];
  }

  if (_canceled)
    return NSDragOperationNone;

  NSDragOperation mask = info->operation_mask;
  targetRWH->DragTargetDragOver(transformedPt, info->location_in_screen,
                                static_cast<DragOperationsMask>(mask),
                                GetModifierFlags(), base::DoNothing());

  if (_delegate)
    _delegate->OnDragOver();

  return _currentOperation;
}

- (BOOL)performDragOperation:(const DraggingInfo*)info
    withWebContentsViewDelegate:
        (content::WebContentsViewDelegate*)webContentsViewDelegate {
  if (_webContents->ShouldIgnoreInputEvents())
    return NO;

  gfx::PointF transformedPt;
  content::RenderWidgetHostImpl* targetRWH =
      [self GetRenderWidgetHostAtPoint:info->location_in_view
                         transformedPt:&transformedPt];

  if (![self isValidDragTarget:targetRWH])
    return NO;

  if (targetRWH != _currentRWHForDrag.get()) {
    if (_currentRWHForDrag)
      _currentRWHForDrag->DragTargetDragLeave(transformedPt,
                                              info->location_in_screen);
    [self draggingEntered:info];
  }

  _currentRVH = nullptr;
  _webContents->Focus();

  if (webContentsViewDelegate) {
    content::DropContext context(/*drop_data=*/*_dropDataFiltered,
                                 /*client_pt=*/transformedPt,
                                 /*screen_pt=*/info->location_in_screen,
                                 /*modifier_flags=*/GetModifierFlags(),
                                 /*target_rwh=*/targetRWH->GetWeakPtr());
    // Use a separate variable since `context` is about to move.
    content::DropData drop_data = context.drop_data;
    webContentsViewDelegate->OnPerformDrop(
        std::move(drop_data),
        base::BindOnce(&DropCompletionCallback, self, std::move(context)));
  } else {
    if (_delegate)
      _delegate->OnDrop();
    targetRWH->DragTargetDrop(*_dropDataFiltered, transformedPt,
                              info->location_in_screen, GetModifierFlags(),
                              base::DoNothing());
  }
  _dropDataUnfiltered.reset();
  _dropDataFiltered.reset();

  return YES;
}

- (void)completeDropAsync:(absl::optional<content::DropData>)dropData
              withContext:(const content::DropContext)context {
  if (dropData.has_value()) {
    if (_delegate)
      _delegate->OnDrop();
    context.target_rwh->DragTargetDrop(
        dropData.value(), context.client_pt, context.screen_pt,
        context.modifier_flags, base::DoNothing());
  } else {
    if (_delegate)
      _delegate->OnDragLeave();
    context.target_rwh->DragTargetDragLeave(gfx::PointF(), gfx::PointF());
  }
}

- (content::RenderWidgetHostImpl*)
    GetRenderWidgetHostAtPoint:(const gfx::PointF&)viewPoint
                 transformedPt:(gfx::PointF*)transformedPt {
  return _webContents->GetInputEventRouter()->GetRenderWidgetHostAtPoint(
      _webContents->GetRenderViewHost()->GetWidget()->GetView(), viewPoint,
      transformedPt);
}

- (void)setDragStartTrackersForProcess:(int)processID {
  _dragStartProcessID = processID;
  _dragStartViewID = GetRenderViewHostID(_webContents->GetRenderViewHost());
}

- (void)resetDragStartTrackers {
  _dragStartProcessID = content::ChildProcessHost::kInvalidUniqueID;
  _dragStartViewID = content::GlobalRoutingID(
      content::ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE);
}

- (bool)isValidDragTarget:(content::RenderWidgetHostImpl*)targetRWH {
  return targetRWH->GetProcess()->GetID() == _dragStartProcessID ||
         GetRenderViewHostID(_webContents->GetRenderViewHost()) !=
             _dragStartViewID;
}

@end

namespace content {

DropData PopulateDropDataFromPasteboard(NSPasteboard* pboard) {
  DCHECK(pboard);
  DropData drop_data;

  // https://crbug.com/1016740#c21
  base::scoped_nsobject<NSArray> types([[pboard types] retain]);

  drop_data.did_originate_from_renderer =
      [types containsObject:ui::kUTTypeChromiumRendererInitiatedDrag];
  drop_data.is_from_privileged =
      [types containsObject:ui::kUTTypeChromiumPrivilegedInitiatedDrag];

  // Get URL if possible. To avoid exposing file system paths to web content,
  // filenames in the drag are not converted to file URLs.
  NSArray<NSString*>* urls;
  NSArray<NSString*>* titles;
  if (ui::clipboard_util::URLsAndTitlesFromPasteboard(
          pboard, /*include_files=*/false, &urls, &titles)) {
    drop_data.url = GURL(base::SysNSStringToUTF8(urls.firstObject));
    drop_data.url_title = base::SysNSStringToUTF16(titles.firstObject);
  }

  // Get plain text.
  if ([types containsObject:NSPasteboardTypeString]) {
    drop_data.text =
        base::SysNSStringToUTF16([pboard stringForType:NSPasteboardTypeString]);
  }

  // Get HTML. If there's no HTML, try RTF.
  if ([types containsObject:NSPasteboardTypeHTML]) {
    NSString* html = [pboard stringForType:NSPasteboardTypeHTML];
    drop_data.html = base::SysNSStringToUTF16(html);
  } else if ([types containsObject:ui::kUTTypeChromiumImageAndHTML]) {
    NSString* html = [pboard stringForType:ui::kUTTypeChromiumImageAndHTML];
    drop_data.html = base::SysNSStringToUTF16(html);
  } else if ([types containsObject:NSPasteboardTypeRTF]) {
    NSString* html = ui::clipboard_util::GetHTMLFromRTFOnPasteboard(pboard);
    drop_data.html = base::SysNSStringToUTF16(html);
  }

  // Get files.
  drop_data.filenames = ui::clipboard_util::FilesFromPasteboard(pboard);

  // Get custom MIME data.
  if ([types containsObject:ui::kUTTypeChromiumWebCustomData]) {
    NSData* customData = [pboard dataForType:ui::kUTTypeChromiumWebCustomData];
    ui::ReadCustomDataIntoMap(customData.bytes, customData.length,
                              &drop_data.custom_data);
  }

  return drop_data;
}

}  // namespace content
