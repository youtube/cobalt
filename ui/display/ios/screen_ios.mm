// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/gfx/native_widget_types.h"

namespace display {
namespace {
class ScreenNotification {
 public:
  virtual void ScreenChanged() = 0;
};
}  // namespace
}  // namespace display

@interface ScreenObserver : NSObject {
  raw_ptr<display::ScreenNotification> _notifier;
}
- (void)mainScreenChanged;
@end

@implementation ScreenObserver

- (instancetype)initWithNotifier:(display::ScreenNotification*)notifier {
  if (self = [super init]) {
    _notifier = notifier;
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(mainScreenChanged)
                          name:UIDeviceOrientationDidChangeNotification
                        object:nil];
  }
  return self;
}

- (void)mainScreenChanged {
  if (!base::SingleThreadTaskRunner::HasCurrentDefault()) {
    return;
  }
  // This notification comes before UIScreen can change its bounds so post a
  // task so the update occurs after the UIScreen has been updated.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&display::ScreenNotification::ScreenChanged,
                                base::Unretained(_notifier)));
}

@end

namespace display {
namespace {

class ScreenIos : public ScreenBase, public ScreenNotification {
 public:
  ScreenIos() {
    observer_ = [[ScreenObserver alloc] initWithNotifier:this];
    ScreenChanged();
  }

  ScreenIos(const ScreenIos&) = delete;
  ScreenIos& operator=(const ScreenIos&) = delete;

  void ScreenChanged() override {
    UIScreen* mainScreen = [UIScreen mainScreen];
    CHECK(mainScreen);
    Display display(0, gfx::Rect(mainScreen.bounds));
    CGFloat scale = [mainScreen scale];
    if (Display::HasForceDeviceScaleFactor()) {
      scale = Display::GetForcedDeviceScaleFactor();
    }
    display.set_device_scale_factor(scale);
    ProcessDisplayChanged(display, true /* is_primary */);
  }

  gfx::Point GetCursorScreenPoint() override {
    NOTIMPLEMENTED();
    return gfx::Point(0, 0);
  }

  bool IsWindowUnderCursor(gfx::NativeWindow window) override {
    NOTIMPLEMENTED();
    return false;
  }

  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    NOTIMPLEMENTED();
    return gfx::NativeWindow();
  }

  int GetNumDisplays() const override {
#if TARGET_IPHONE_SIMULATOR
    // UIScreen does not reliably return correct results on the simulator.
    return 1;
#else
    return [[UIScreen screens] count];
#endif
  }

 private:
  ScreenObserver* __strong observer_;
};

}  // namespace

// static
gfx::NativeWindow Screen::GetWindowForView(gfx::NativeView view) {
  return gfx::NativeWindow(view.Get().window);
}

Screen* CreateNativeScreen() {
  return new ScreenIos;
}

}  // namespace display
