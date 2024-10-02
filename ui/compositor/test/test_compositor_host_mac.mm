// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_compositor_host.h"

#include "base/memory/raw_ptr.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"

#import <AppKit/NSApplication.h>
#import <AppKit/NSOpenGL.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <Foundation/NSAutoreleasePool.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"

// AcceleratedTestView provides an NSView class that delegates drawing to a
// ui::Compositor delegate, setting up the NSOpenGLContext as required.
@interface AcceleratedTestView : NSView {
  raw_ptr<ui::Compositor> _compositor;
}
// Designated initializer.
- (instancetype)init;
- (void)setCompositor:(ui::Compositor*)compositor;
@end

@implementation AcceleratedTestView
- (instancetype)init {
  // The frame will be resized when reparented into the window's view hierarchy.
  if ((self = [super initWithFrame:NSZeroRect])) {
    [self setWantsLayer:YES];
  }
  return self;
}

- (void)setCompositor:(ui::Compositor*)compositor {
  _compositor = compositor;
}

- (void)drawRect:(NSRect)rect {
  DCHECK(_compositor) << "Drawing with no compositor set.";
  _compositor->ScheduleFullRedraw();
}
@end

namespace ui {

// Tests that use Objective-C memory semantics need to have a top-level
// NSAutoreleasePool set up and initialized prior to execution and drained upon
// exit.  The tests will leak otherwise.
class FoundationHost {
 public:
  FoundationHost(const FoundationHost&) = delete;
  FoundationHost& operator=(const FoundationHost&) = delete;

 protected:
  FoundationHost() { pool_ = [[NSAutoreleasePool alloc] init]; }
  virtual ~FoundationHost() { [pool_ drain]; }

 private:
  NSAutoreleasePool* pool_;
};

// Tests that use the AppKit framework need to have the NSApplication
// initialized prior to doing anything with display objects such as windows,
// views, or controls.
class AppKitHost : public FoundationHost {
 public:
  AppKitHost(const AppKitHost&) = delete;
  AppKitHost& operator=(const AppKitHost&) = delete;

 protected:
  AppKitHost() { [NSApplication sharedApplication]; }
  ~AppKitHost() override {}
};

class TestAcceleratedWidgetMacNSView : public AcceleratedWidgetMacNSView {
 public:
  TestAcceleratedWidgetMacNSView(NSView* view) : view_([view retain]) {}

  TestAcceleratedWidgetMacNSView(const TestAcceleratedWidgetMacNSView&) =
      delete;
  TestAcceleratedWidgetMacNSView& operator=(
      const TestAcceleratedWidgetMacNSView&) = delete;

  virtual ~TestAcceleratedWidgetMacNSView() { [view_ release]; }

  // AcceleratedWidgetMacNSView
  void AcceleratedWidgetCALayerParamsUpdated() override {}

 private:
  NSView* view_ = nullptr;
};

// TestCompositorHostMac provides a window surface and a coordinated compositor
// for use in the compositor unit tests.
class TestCompositorHostMac : public TestCompositorHost, public AppKitHost {
 public:
  TestCompositorHostMac(const gfx::Rect& bounds,
                        ui::ContextFactory* context_factory);

  TestCompositorHostMac(const TestCompositorHostMac&) = delete;
  TestCompositorHostMac& operator=(const TestCompositorHostMac&) = delete;

  ~TestCompositorHostMac() override;

 private:
  // TestCompositorHost:
  void Show() override;
  ui::Compositor* GetCompositor() override;

  gfx::Rect bounds_;

  ui::Compositor compositor_;
  ui::AcceleratedWidgetMac accelerated_widget_;
  std::unique_ptr<TestAcceleratedWidgetMacNSView>
      test_accelerated_widget_nsview_;

  // Owned.  Released when window is closed.
  NSWindow* window_;
  viz::ParentLocalSurfaceIdAllocator allocator_;
};

TestCompositorHostMac::TestCompositorHostMac(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory)
    : bounds_(bounds),
      compositor_(context_factory->AllocateFrameSinkId(),
                  context_factory,
                  base::SingleThreadTaskRunner::GetCurrentDefault(),
                  false /* enable_pixel_canvas */),
      window_(nil) {}

TestCompositorHostMac::~TestCompositorHostMac() {
  accelerated_widget_.ResetNSView();
  // Release reference to |compositor_|.  Important because the |compositor_|
  // holds |this| as its delegate, so that reference must be removed here.
  [[window_ contentView] setCompositor:NULL];
  {
    base::scoped_nsobject<NSView> new_view(
        [[NSView alloc] initWithFrame:NSZeroRect]);
    [window_ setContentView:new_view.get()];
  }

  [window_ orderOut:nil];
  [window_ close];
}

void TestCompositorHostMac::Show() {
  DCHECK(!window_);
  window_ = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(bounds_.x(), bounds_.y(), bounds_.width(),
                                     bounds_.height())
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  base::scoped_nsobject<AcceleratedTestView> view(
      [[AcceleratedTestView alloc] init]);
  test_accelerated_widget_nsview_ =
      std::make_unique<TestAcceleratedWidgetMacNSView>(view);
  allocator_.GenerateId();
  accelerated_widget_.SetNSView(test_accelerated_widget_nsview_.get());
  compositor_.SetAcceleratedWidget(accelerated_widget_.accelerated_widget());
  compositor_.SetScaleAndSize(1.0f, bounds_.size(),
                              allocator_.GetCurrentLocalSurfaceId());
  [view setCompositor:&compositor_];
  [window_ setContentView:view];
  [window_ orderFront:nil];
}

ui::Compositor* TestCompositorHostMac::GetCompositor() {
  return &compositor_;
}

// static
TestCompositorHost* TestCompositorHost::Create(
    const gfx::Rect& bounds,
    ui::ContextFactory* context_factory) {
  return new TestCompositorHostMac(bounds, context_factory);
}

}  // namespace ui
