// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ui/ozone/platform/starboard/ozone_platform_starboard.h"

#include <thread>

#include "base/logging.h"
#include "starboard/event.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/display/fake/fake_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/starboard/platform_screen_starboard.h"
#include "ui/ozone/platform/starboard/platform_window_starboard.h"
#include "ui/ozone/platform/starboard/surface_factory_starboard.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {
namespace {

// Ozone platform implementation for Starboard, an OS abstraction and porting
// layer for Cobalt (https://github.com/youtube/cobalt). Platform specific
// implementation details are abstracted out using Starboard.
//  - TODO(tholcombe): fill out implementation details as they're added.
class OzonePlatformStarboard : public OzonePlatform {
 public:
  OzonePlatformStarboard() {}

  OzonePlatformStarboard(const OzonePlatformStarboard&) = delete;
  OzonePlatformStarboard& operator=(const OzonePlatformStarboard&) = delete;

  ~OzonePlatformStarboard() override {}

  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    if (!surface_factory_) {
      LOG(WARNING) << "GetSurfaceFactoryOzone(): surface_factory_ is expected "
                      "to be initialized in InitializeGPU "
                      "or InitializeUI, but no graphics context was found.";
      surface_factory_ = std::make_unique<SurfaceFactoryStarboard>();
    }
    return surface_factory_.get();
  }

  ui::OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  ui::CursorFactory* GetCursorFactory() override {
    return cursor_factory_.get();
  }

  ui::InputController* GetInputController() override {
    return input_controller_.get();
  }

  ui::GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    initial_window_size_ = properties.bounds;
    return std::make_unique<PlatformWindowStarboard>(delegate,
                                                     properties.bounds);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    // TODO(b/371272304): Implement a native display delegate to allow display
    // configuration and hotplug (adding a new display while the process is
    // running).
    return std::make_unique<display::FakeDisplayDelegate>();
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    return std::make_unique<PlatformScreenStarboard>();
  }

  // This function must be called immediately after CreateScreen with the
  // `screen` that was returned from CreateScreen. They are separated to avoid
  // observer recursion into display::Screen from inside CreateScreen.
  void InitScreen(PlatformScreen* screen) override {
    auto platform_screen = static_cast<PlatformScreenStarboard*>(screen);
    // NOTE(b/410834483) we can initialize the screen with the window size
    // because for Cobalt we assume that window size is also screen size.
    platform_screen->InitScreen(initial_window_size_);
  }

  std::unique_ptr<InputMethod> CreateInputMethod(
      ImeKeyEventDispatcher* ime_key_event_dispatcher,
      gfx::AcceleratedWidget widget) override {
    return std::make_unique<InputMethodMinimal>(ime_key_event_dispatcher);
  }

  void PostCreateMainMessageLoop(base::OnceCallback<void()> shutdown_cb,
                                 scoped_refptr<base::SingleThreadTaskRunner>
                                     user_input_task_runner) override {}

  bool InitializeUI(const InitParams& params) override {
    if (!surface_factory_) {
      surface_factory_ = std::make_unique<SurfaceFactoryStarboard>();
    }

    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    // It doesn't matter which of the UI or GPU creates | overlay_manager_ | and
    // | surface_factory_ | in single-process mode.
    if (!overlay_manager_) {
      overlay_manager_ = std::make_unique<StubOverlayManager>();
    }
    // TODO(b/371272304): Investigate if we need our own implementation of
    // InputController for things like gamepads or other atypical input devices.
    input_controller_ = CreateStubInputController();
    cursor_factory_ = std::make_unique<BitmapCursorFactory>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    return true;
  }

  void InitializeGPU(const InitParams& params) override {
    // It doesn't matter which of the UI or GPU creates | overlay_manager_ | and
    // | surface_factory_ | in single-process mode.
    if (!overlay_manager_) {
      overlay_manager_ = std::make_unique<StubOverlayManager>();
    }
    if (!surface_factory_) {
      surface_factory_ = std::make_unique<SurfaceFactoryStarboard>();
    }
  }

 private:
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
  std::unique_ptr<SurfaceFactoryStarboard> surface_factory_;

  // Used to initialize the PlatformScreen within the InitScreen() method later.
  gfx::Rect initial_window_size_;
};

}  // namespace

OzonePlatform* CreateOzonePlatformStarboard() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kUseGL)) {
    CHECK_EQ(cmd->GetSwitchValueASCII(switches::kUseGL),
             gl::kGLImplementationANGLEName)
        << " Unsupported " << switches::kUseGL << " implementation";
  }

  return new OzonePlatformStarboard();
}

}  // namespace ui
