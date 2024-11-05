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

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/common/bitmap_cursor_factory.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/starboard/platform_screen_starboard.h"
#include "ui/ozone/platform/starboard/surface_factory_starboard.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"
#include "ui/ozone/platform/starboard/platform_window_starboard.h"

#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

#include "starboard/log.h"

namespace ui {
namespace {

class OzonePlatformStarboard: public OzonePlatform {
 public:

  OzonePlatformStarboard() {}

  OzonePlatformStarboard(const OzonePlatformStarboard&) = delete;
  OzonePlatformStarboard& operator=(const OzonePlatformStarboard&) = delete;

  ~OzonePlatformStarboard() override {}

  SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return surface_factory_.get();
  }

  OverlayManagerOzone* GetOverlayManager() override {
    return nullptr;
  }

  CursorFactory* GetCursorFactory() override {
    return cursor_factory_.get();
  }

  InputController* GetInputController() override {
    return nullptr;
  }

  std::unique_ptr<PlatformScreen> CreateScreen() override {
    return std::make_unique<PlatformScreenStarboard>();
  }

  void InitScreen(PlatformScreen* screen) override {}

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    return std::make_unique<PlatformWindowStarboard>(delegate, properties.bounds);
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    return nullptr;
  }

  std::unique_ptr<InputMethod> CreateInputMethod(
      ImeKeyEventDispatcher* ime_key_event_dispatcher,
      gfx::AcceleratedWidget) override {
    return std::make_unique<InputMethodMinimal>(ime_key_event_dispatcher);
  }

  bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                     gfx::BufferUsage usage) const override {
    return true;
  }

  bool InitializeUI(const InitParams& params) override {
    if (!surface_factory_) {
      surface_factory_ = std::make_unique<SurfaceFactoryStarboard>();
    }
    platform_event_source_ = std::make_unique<starboard::StarboardPlatformEventSource>();
    keyboard_layout_engine_ = std::make_unique<StubKeyboardLayoutEngine>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        keyboard_layout_engine_.get());

    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    cursor_factory_ = std::make_unique<BitmapCursorFactory>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());

    return true;
  }

  void InitializeGPU(const InitParams& params) override {
    if (!surface_factory_) {
      surface_factory_ = std::make_unique<SurfaceFactoryStarboard>();
    }
  }

  void PostCreateMainMessageLoop(base::OnceCallback<void()> shutdown_cb,
                                 scoped_refptr<base::SingleThreadTaskRunner>
                                     user_input_task_runner) override {
  }

 private:
  std::unique_ptr<KeyboardLayoutEngine> keyboard_layout_engine_;
  std::unique_ptr<SurfaceFactoryStarboard> surface_factory_;
  std::unique_ptr<starboard::StarboardPlatformEventSource> platform_event_source_;
  std::unique_ptr<CursorFactory> cursor_factory_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;
};

}  // namespace

OzonePlatform* CreateOzonePlatformStarboard() {
  return new OzonePlatformStarboard();
}

}  // namespace ui
