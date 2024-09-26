// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#import "ui/base/cocoa/base_view.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// Overrides events and actions for NSPopUpButtonCell.
@interface FakeNSPopUpButtonCell : NSObject
@end

@implementation FakeNSPopUpButtonCell

- (void)performClickWithFrame:(NSRect)frame inView:(NSView*)view {
}

- (void)attachPopUpWithFrame:(NSRect)frame inView:(NSView*)view {
}

@end

namespace headless {

namespace {

// Swizzles all event and acctions for NSPopUpButtonCell to avoid showing in
// headless mode.
class HeadlessPopUpMethods {
 public:
  HeadlessPopUpMethods(const HeadlessPopUpMethods&) = delete;
  HeadlessPopUpMethods& operator=(const HeadlessPopUpMethods&) = delete;

  static void Init() {
    [[maybe_unused]] static base::NoDestructor<HeadlessPopUpMethods> swizzler;
  }

 private:
  friend class base::NoDestructor<HeadlessPopUpMethods>;
  HeadlessPopUpMethods()
      : popup_perform_click_swizzler_([NSPopUpButtonCell class],
                                      [FakeNSPopUpButtonCell class],
                                      @selector(performClickWithFrame:inView:)),
        popup_attach_swizzler_([NSPopUpButtonCell class],
                               [FakeNSPopUpButtonCell class],
                               @selector(attachPopUpWithFrame:inView:)) {}

  base::mac::ScopedObjCClassSwizzler popup_perform_click_swizzler_;
  base::mac::ScopedObjCClassSwizzler popup_attach_swizzler_;
};

NSString* const kActivityReason = @"Batch headless process";
const NSActivityOptions kActivityOptions =
    (NSActivityUserInitiatedAllowingIdleSystemSleep |
     NSActivityLatencyCritical) &
    ~(NSActivitySuddenTerminationDisabled |
      NSActivityAutomaticTerminationDisabled);

}  // namespace

void HeadlessBrowserImpl::PlatformInitialize() {
  screen_ = std::make_unique<display::ScopedNativeScreen>();
  HeadlessPopUpMethods::Init();
}

void HeadlessBrowserImpl::PlatformStart() {
  // Disallow headless to be throttled as a background process.
  [[NSProcessInfo processInfo] beginActivityWithOptions:kActivityOptions
                                                 reason:kActivityReason];
}

void HeadlessBrowserImpl::PlatformInitializeWebContents(
    HeadlessWebContentsImpl* web_contents) {
  NSView* web_view =
      web_contents->web_contents()->GetNativeView().GetNativeNSView();
  [web_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  // TODO(eseckler): Support enabling BeginFrameControl on Mac. This is tricky
  // because it's a ui::Compositor startup setting and ui::Compositors are
  // recycled on Mac, see browser_compositor_view_mac.mm.
}

void HeadlessBrowserImpl::PlatformSetWebContentsBounds(
    HeadlessWebContentsImpl* web_contents,
    const gfx::Rect& bounds) {
  content::WebContents* content_web_contents = web_contents->web_contents();

  NSView* web_view = content_web_contents->GetNativeView().GetNativeNSView();
  NSRect frame = gfx::ScreenRectToNSRect(bounds);
  [web_view setFrame:frame];

  // Render widget host view is not ready at this point, so post a task to set
  // bounds at later time.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<content::WebContents> content_web_contents,
             const gfx::Rect& bounds) {
            if (content_web_contents) {
              content::RenderWidgetHostView* host_view =
                  content_web_contents->GetRenderWidgetHostView();
              if (host_view) {
                host_view->SetWindowFrameInScreen(bounds);
              }
            }
          },
          content_web_contents->GetWeakPtr(), bounds));
}

ui::Compositor* HeadlessBrowserImpl::PlatformGetCompositor(
    HeadlessWebContentsImpl* web_contents) {
  // TODO(eseckler): Support BeginFrameControl on Mac.
  return nullptr;
}

}  // namespace headless
