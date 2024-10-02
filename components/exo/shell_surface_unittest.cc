// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include <vector>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/constants/ash_constants.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/permission.h"
#include "components/exo/security_delegate.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/surface_test_util.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/mock_security_delegate.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/window_properties.h"
#include "components/exo/wm_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace exo {

using ShellSurfaceTest = test::ExoTestBase;

namespace {

bool HasBackdrop() {
  ash::WorkspaceController* wc = ash::ShellTestApi().workspace_controller();
  return !!ash::WorkspaceControllerTestApi(wc).GetBackdropWindow();
}

uint32_t ConfigureFullscreen(uint32_t serial,
                             const gfx::Rect& bounds,
                             chromeos::WindowStateType state_type,
                             bool resizing,
                             bool activated,
                             const gfx::Vector2d& origin_offset,
                             float raster_scale) {
  EXPECT_EQ(chromeos::WindowStateType::kFullscreen, state_type);
  return serial;
}

std::unique_ptr<ShellSurface> CreatePopupShellSurface(
    Surface* popup_surface,
    ShellSurface* parent,
    const gfx::Point& origin) {
  auto popup_shell_surface = std::make_unique<ShellSurface>(popup_surface);
  popup_shell_surface->DisableMovement();
  popup_shell_surface->SetPopup();
  popup_shell_surface->SetParent(parent);
  popup_shell_surface->SetOrigin(origin);
  return popup_shell_surface;
}

std::unique_ptr<ShellSurface> CreateX11TransientShellSurface(
    ShellSurface* parent,
    const gfx::Size& size,
    const gfx::Point& origin) {
  return test::ShellSurfaceBuilder(size)
      .SetParent(parent)
      .SetOrigin(origin)
      .BuildShellSurface();
}

struct ConfigureData {
  gfx::Rect suggested_bounds;
  chromeos::WindowStateType state_type = chromeos::WindowStateType::kDefault;
  bool is_resizing = false;
  bool is_active = false;
  float raster_scale = 1.0f;
};

uint32_t Configure(ConfigureData* config_data,
                   const gfx::Rect& bounds,
                   chromeos::WindowStateType state_type,
                   bool resizing,
                   bool activated,
                   const gfx::Vector2d& origin_offset,
                   float raster_scale) {
  config_data->suggested_bounds = bounds;
  config_data->state_type = state_type;
  config_data->is_resizing = resizing;
  config_data->is_active = activated;
  config_data->raster_scale = raster_scale;
  return 0;
}

bool IsCaptureWindow(ShellSurface* shell_surface) {
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  return WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow() ==
         window;
}

}  // namespace

TEST_F(ShellSurfaceTest, AcknowledgeConfigure) {
  gfx::Size buffer_size(32, 32);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Point origin(100, 100);
  shell_surface->GetWidget()->SetBounds(gfx::Rect(origin, buffer_size));
  EXPECT_EQ(origin.ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());

  const uint32_t kSerial = 1;
  shell_surface->set_configure_callback(
      base::BindRepeating(&ConfigureFullscreen, kSerial));
  shell_surface->SetFullscreen(true);

  // Surface origin should not change until configure request is acknowledged.
  EXPECT_EQ(origin.ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());

  // Compositor should be locked until configure request is acknowledged.
  ui::Compositor* compositor =
      shell_surface->GetWidget()->GetNativeWindow()->layer()->GetCompositor();
  EXPECT_TRUE(compositor->IsLocked());

  shell_surface->AcknowledgeConfigure(kSerial);
  std::unique_ptr<Buffer> fullscreen_buffer(new Buffer(
      exo_test_helper()->CreateGpuMemoryBuffer(GetContext()->bounds().size())));
  surface->Attach(fullscreen_buffer.get());
  surface->Commit();

  EXPECT_EQ(gfx::Point().ToString(),
            surface->window()->GetBoundsInRootWindow().origin().ToString());
  EXPECT_FALSE(compositor->IsLocked());
}

TEST_F(ShellSurfaceTest, SetParent) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> parent_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> parent_surface(new Surface);
  std::unique_ptr<ShellSurface> parent_shell_surface(
      new ShellSurface(parent_surface.get()));

  parent_surface->Attach(parent_buffer.get());
  parent_surface->Commit();

  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  shell_surface->SetParent(parent_shell_surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(
      parent_shell_surface->GetWidget()->GetNativeWindow(),
      wm::GetTransientParent(shell_surface->GetWidget()->GetNativeWindow()));

  // Use OnSetParent to move shell surface to 10, 10.
  gfx::Point parent_origin =
      parent_shell_surface->GetWidget()->GetWindowBoundsInScreen().origin();
  shell_surface->OnSetParent(
      parent_surface.get(),
      gfx::PointAtOffsetFromOrigin(gfx::Point(10, 10) - parent_origin));
  EXPECT_EQ(gfx::Rect(10, 10, 256, 256),
            shell_surface->GetWidget()->GetWindowBoundsInScreen());
  EXPECT_FALSE(shell_surface->CanActivate());
}

TEST_F(ShellSurfaceTest, DeleteShellSurfaceWithTransientChildren) {
  gfx::Size buffer_size(256, 256);
  auto parent_shell_surface =
      test::ShellSurfaceBuilder(buffer_size).BuildShellSurface();

  auto child1_shell_surface = test::ShellSurfaceBuilder(buffer_size)
                                  .SetParent(parent_shell_surface.get())
                                  .BuildShellSurface();
  auto child2_shell_surface = test::ShellSurfaceBuilder(buffer_size)
                                  .SetParent(parent_shell_surface.get())
                                  .BuildShellSurface();
  auto child3_shell_surface = test::ShellSurfaceBuilder(buffer_size)
                                  .SetParent(parent_shell_surface.get())
                                  .BuildShellSurface();

  EXPECT_EQ(parent_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child1_shell_surface->GetWidget()->GetNativeWindow()));
  EXPECT_EQ(parent_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child2_shell_surface->GetWidget()->GetNativeWindow()));
  EXPECT_EQ(parent_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                child3_shell_surface->GetWidget()->GetNativeWindow()));
  parent_shell_surface.reset();
}

TEST_F(ShellSurfaceTest, Maximize) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  shell_surface->Maximize();
  EXPECT_FALSE(HasBackdrop());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().width(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());

  // Toggle maximize.
  ash::WMEvent maximize_event(ash::WM_EVENT_TOGGLE_MAXIMIZE);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  ash::WindowState::Get(window)->OnWMEvent(&maximize_event);
  EXPECT_FALSE(shell_surface->GetWidget()->IsMaximized());
  EXPECT_FALSE(HasBackdrop());

  ash::WindowState::Get(window)->OnWMEvent(&maximize_event);
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
  EXPECT_FALSE(HasBackdrop());
}

TEST_F(ShellSurfaceTest, CanMaximizeResizableWindow) {
  gfx::Size buffer_size(400, 300);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();

  // Make sure we've created a resizable window.
  EXPECT_TRUE(shell_surface->CanResize());

  // Assert: Resizable windows can be maximized.
  EXPECT_TRUE(shell_surface->CanMaximize());
}

TEST_F(ShellSurfaceTest, CannotMaximizeNonResizableWindow) {
  gfx::Size buffer_size(400, 300);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  shell_surface->SetMinimumSize(buffer_size);
  shell_surface->SetMaximumSize(buffer_size);
  surface->Commit();

  // Make sure we've created a non-resizable window.
  EXPECT_FALSE(shell_surface->CanResize());

  // Assert: Non-resizable windows cannot be maximized.
  EXPECT_FALSE(shell_surface->CanMaximize());
}

TEST_F(ShellSurfaceTest, MaximizeFromFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();
  // Act: Maximize after fullscreen
  shell_surface->root_surface()->Commit();
  shell_surface->SetFullscreen(true);
  shell_surface->root_surface()->Commit();
  shell_surface->Maximize();
  shell_surface->root_surface()->Commit();

  // Assert: Window should stay fullscreen.
  EXPECT_TRUE(shell_surface->GetWidget()->IsFullscreen());
}

TEST_F(ShellSurfaceTest, MaximizeExitsFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();

  // Act: Set window property kRestoreOrMaximizeExitsFullscreen
  // then maximize after fullscreen
  shell_surface->root_surface()->Commit();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      kRestoreOrMaximizeExitsFullscreen, true);
  shell_surface->SetFullscreen(true);
  shell_surface->root_surface()->Commit();
  shell_surface->Maximize();
  shell_surface->root_surface()->Commit();

  // Assert: Window should exit fullscreen and be maximized.
  EXPECT_TRUE(shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
      kRestoreOrMaximizeExitsFullscreen));
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
}

TEST_F(ShellSurfaceTest, Minimize) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  EXPECT_TRUE(shell_surface->CanMinimize());

  // Minimizing can be performed before the surface is committed, but
  // widget creation will be deferred.
  shell_surface->Minimize();
  EXPECT_FALSE(shell_surface->GetWidget());

  // Attaching the buffer will create a widget with minimized state.
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());

  shell_surface->Restore();
  EXPECT_FALSE(shell_surface->GetWidget()->IsMinimized());

  std::unique_ptr<Surface> child_surface(new Surface);
  std::unique_ptr<ShellSurface> child_shell_surface(
      new ShellSurface(child_surface.get()));

  // Transient shell surfaces cannot be minimized.
  child_surface->SetParent(surface.get(), gfx::Point());
  child_surface->Attach(buffer.get());
  child_surface->Commit();
  EXPECT_FALSE(child_shell_surface->CanMinimize());
}

TEST_F(ShellSurfaceTest, Restore) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  // Note: Remove contents to avoid issues with maximize animations in tests.
  shell_surface->Maximize();
  EXPECT_FALSE(HasBackdrop());
  shell_surface->Restore();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(
      buffer_size.ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
}

TEST_F(ShellSurfaceTest, RestoreFromFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();

  // Act: Restore after fullscreen
  shell_surface->SetFullscreen(true);
  shell_surface->root_surface()->Commit();
  shell_surface->Restore();
  shell_surface->root_surface()->Commit();

  // Assert: Window should stay fullscreen.
  EXPECT_TRUE(shell_surface->GetWidget()->IsFullscreen());
}

TEST_F(ShellSurfaceTest, RestoreExitsFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  // Act: Set window property kRestoreOrMaximizeExitsFullscreen
  // then restore after fullscreen
  shell_surface->root_surface()->Commit();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      kRestoreOrMaximizeExitsFullscreen, true);
  shell_surface->SetFullscreen(true);
  shell_surface->Restore();
  shell_surface->root_surface()->Commit();

  // Assert: Window should exit fullscreen and be restored.
  EXPECT_TRUE(shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
      kRestoreOrMaximizeExitsFullscreen));
  EXPECT_EQ(gfx::Size(256, 256),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());
}

TEST_F(ShellSurfaceTest, HostWindowBoundsUpdatedAfterCommitWidget) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  shell_surface->SurfaceTreeHost::OnSurfaceCommit();
  shell_surface->root_surface()->SetSurfaceHierarchyContentBoundsForTest(
      gfx::Rect(0, 0, 50, 50));

  // Host Window Bounds size before committing.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), shell_surface->host_window()->bounds());
  EXPECT_TRUE(shell_surface->OnPreWidgetCommit());
  shell_surface->CommitWidget();
  // CommitWidget should update the Host Window Bounds.
  EXPECT_EQ(gfx::Rect(0, 0, 50, 50), shell_surface->host_window()->bounds());
}

TEST_F(ShellSurfaceTest, SetFullscreen) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetFullscreen(true);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
  shell_surface->SetFullscreen(false);
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_NE(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());
}

TEST_F(ShellSurfaceTest, PreWidgetUnfullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();
  shell_surface->Maximize();
  shell_surface->SetFullscreen(false);
  EXPECT_EQ(shell_surface->GetWidget(), nullptr);
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
}

TEST_F(ShellSurfaceTest, PreWidgetMaximizeFromFullscreen) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetNoCommit()
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();
  // Fullscreen -> Maximize for non Lacros surfaces should stay fullscreen
  shell_surface->SetFullscreen(true);
  shell_surface->Maximize();
  EXPECT_EQ(shell_surface->GetWidget(), nullptr);
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsFullscreen());
}

TEST_F(ShellSurfaceTest, SetTitle) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->SetTitle(std::u16string(u"test"));
  surface->Attach(buffer.get());
  surface->Commit();

  // NativeWindow's title is used within the overview mode, so it should
  // have the specified title.
  EXPECT_EQ(u"test", shell_surface->GetWidget()->GetNativeWindow()->GetTitle());
  // The titlebar shouldn't show the title.
  EXPECT_FALSE(
      shell_surface->GetWidget()->widget_delegate()->ShouldShowWindowTitle());
}

TEST_F(ShellSurfaceTest, SetApplicationId) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  EXPECT_FALSE(shell_surface->GetWidget());
  shell_surface->SetApplicationId("pre-widget-id");

  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ("pre-widget-id", *GetShellApplicationId(window));
  shell_surface->SetApplicationId("test");
  EXPECT_EQ("test", *GetShellApplicationId(window));
  EXPECT_FALSE(ash::WindowState::Get(window)->allow_set_bounds_direct());

  shell_surface->SetApplicationId(nullptr);
  EXPECT_EQ(nullptr, GetShellApplicationId(window));
}

TEST_F(ShellSurfaceTest, ActivationPermissionLegacy) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(window);

  // No permission granted so can't activate.
  EXPECT_FALSE(HasPermissionToActivate(window));

  // Can grant permission.
  GrantPermissionToActivate(window, base::Days(1));
  exo::Permission* permission = window->GetProperty(kPermissionKey);
  EXPECT_TRUE(permission->Check(Permission::Capability::kActivate));
  EXPECT_TRUE(HasPermissionToActivate(window));

  // Can revoke permission.
  RevokePermissionToActivate(window);
  EXPECT_FALSE(HasPermissionToActivate(window));

  // Can grant permission again.
  GrantPermissionToActivate(window, base::Days(2));
  exo::Permission* permission2 = window->GetProperty(kPermissionKey);
  EXPECT_TRUE(permission2->Check(Permission::Capability::kActivate));
  EXPECT_TRUE(HasPermissionToActivate(window));
}

TEST_F(ShellSurfaceTest, WidgetActivationLegacy) {
  std::unique_ptr<SecurityDelegate> default_security_delegate =
      SecurityDelegate::GetDefaultSecurityDelegate();
  gfx::Size buffer_size(64, 64);
  auto buffer1 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface1 = std::make_unique<Surface>();
  auto shell_surface1 = std::make_unique<ShellSurface>(surface1.get());
  surface1->Attach(buffer1.get());
  surface1->Commit();
  shell_surface1->SetSecurityDelegate(default_security_delegate.get());

  // The window is active.
  views::Widget* widget1 = shell_surface1->GetWidget();
  EXPECT_TRUE(widget1->IsActive());

  // Create a second window.
  auto buffer2 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface2 = std::make_unique<Surface>();
  auto shell_surface2 = std::make_unique<ShellSurface>(surface2.get());
  surface2->Attach(buffer2.get());
  surface2->Commit();
  shell_surface2->SetSecurityDelegate(default_security_delegate.get());

  // Now the second window is active.
  views::Widget* widget2 = shell_surface2->GetWidget();
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());

  // Grant permission to activate the first window.
  GrantPermissionToActivate(widget1->GetNativeWindow(), base::Days(1));

  // The first window can activate itself.
  surface1->RequestActivation();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget2->IsActive());

  // The second window cannot activate itself.
  surface2->RequestActivation();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget2->IsActive());
}

TEST_F(ShellSurfaceTest, WidgetActivation) {
  test::MockSecurityDelegate security_delegate;
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<ShellSurface> shell_surface1 =
      test::ShellSurfaceBuilder(buffer_size)
          .SetSecurityDelegate(&security_delegate)
          .BuildShellSurface();

  // The window is active.
  views::Widget* widget1 = shell_surface1->GetWidget();
  EXPECT_TRUE(widget1->IsActive());

  // Create a second window.
  std::unique_ptr<ShellSurface> shell_surface2 =
      test::ShellSurfaceBuilder(buffer_size)
          .SetSecurityDelegate(&security_delegate)
          .BuildShellSurface();

  // Now the second window is active.
  views::Widget* widget2 = shell_surface2->GetWidget();
  EXPECT_FALSE(widget1->IsActive());
  EXPECT_TRUE(widget2->IsActive());

  // The first window can activate itself.
  EXPECT_CALL(security_delegate, CanSelfActivate(widget1->GetNativeWindow()))
      .WillOnce(testing::Return(true));
  shell_surface1->surface_for_testing()->RequestActivation();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget2->IsActive());

  // The second window cannot activate itself.
  EXPECT_CALL(security_delegate, CanSelfActivate(widget2->GetNativeWindow()))
      .WillOnce(testing::Return(false));
  shell_surface2->surface_for_testing()->RequestActivation();
  EXPECT_TRUE(widget1->IsActive());
  EXPECT_FALSE(widget2->IsActive());
}

TEST_F(ShellSurfaceTest, EmulateOverrideRedirect) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  EXPECT_FALSE(shell_surface->GetWidget());
  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_FALSE(ash::WindowState::Get(window)->allow_set_bounds_direct());

  // Only surface with no app id with parent surface is considered
  // override redirect.
  std::unique_ptr<Surface> child_surface(new Surface);
  std::unique_ptr<ShellSurface> child_shell_surface(
      new ShellSurface(child_surface.get()));

  child_surface->SetParent(surface.get(), gfx::Point());
  child_surface->Attach(buffer.get());
  child_surface->Commit();
  aura::Window* child_window =
      child_shell_surface->GetWidget()->GetNativeWindow();

  // The window will not have a window state, thus will no be managed by window
  // manager.
  EXPECT_TRUE(ash::WindowState::Get(child_window)->allow_set_bounds_direct());
  EXPECT_EQ(ash::kShellWindowId_ShelfBubbleContainer,
            child_window->parent()->GetId());

  // NONE/SHADOW frame type should work on override redirect.
  child_surface->SetFrame(SurfaceFrameType::SHADOW);
  child_surface->Commit();
  EXPECT_EQ(wm::kShadowElevationMenuOrTooltip,
            wm::GetShadowElevationConvertDefault(child_window));

  child_surface->SetFrame(SurfaceFrameType::NONE);
  child_surface->Commit();
  EXPECT_EQ(wm::kShadowElevationNone,
            wm::GetShadowElevationConvertDefault(child_window));
}

TEST_F(ShellSurfaceTest, SetStartupId) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  EXPECT_FALSE(shell_surface->GetWidget());
  shell_surface->SetStartupId("pre-widget-id");

  surface->Attach(buffer.get());
  surface->Commit();
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ("pre-widget-id", *GetShellStartupId(window));
  shell_surface->SetStartupId("test");
  EXPECT_EQ("test", *GetShellStartupId(window));

  shell_surface->SetStartupId(nullptr);
  EXPECT_EQ(nullptr, GetShellStartupId(window));
}

TEST_F(ShellSurfaceTest, StartMove) {
  // TODO: Ractor out the shell surface creation.
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  // Map shell surface.
  surface->Attach(buffer.get());
  surface->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  // The interactive move should end when surface is destroyed.
  shell_surface->StartMove();

  // Test that destroying the shell surface before move ends is OK.
  shell_surface.reset();
}

TEST_F(ShellSurfaceTest, StartResize) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  // Map shell surface.
  surface->Attach(buffer.get());
  surface->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  // The interactive resize should end when surface is destroyed.
  shell_surface->StartResize(HTBOTTOMRIGHT);

  // Test that destroying the surface before resize ends is OK.
  surface.reset();
}

TEST_F(ShellSurfaceTest, StartResizeAndDestroyShell) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  uint32_t serial = 0;
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, const gfx::Rect& bounds,
         chromeos::WindowStateType state_type, bool resizing, bool activated,
         const gfx::Vector2d& origin_offset,
         float raster_scale) { return ++(*serial_ptr); },
      &serial);

  // Map shell surface.
  surface->Attach(buffer.get());
  shell_surface->set_configure_callback(configure_callback);

  surface->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  // The interactive resize should end when surface is destroyed.
  shell_surface->StartResize(HTBOTTOMRIGHT);

  // Go through configure/commit stage to update the resize component.
  shell_surface->AcknowledgeConfigure(serial);
  surface->Commit();

  shell_surface->set_configure_callback(base::BindRepeating(
      [](const gfx::Rect& bounds, chromeos::WindowStateType state_type,
         bool resizing, bool activated, const gfx::Vector2d& origin_offset,
         float raster_scale) {
        ADD_FAILURE() << "Configure Should not be called";
        return uint32_t{0};
      }));

  // Test that destroying the surface before resize ends is OK.
  shell_surface.reset();
}

TEST_F(ShellSurfaceTest, SetGeometry) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  gfx::Rect geometry(16, 16, 32, 32);
  shell_surface->SetGeometry(geometry);
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(
      geometry.size().ToString(),
      shell_surface->GetWidget()->GetWindowBoundsInScreen().size().ToString());
  EXPECT_EQ(gfx::Rect(gfx::Point() - geometry.OffsetFromOrigin(), buffer_size)
                .ToString(),
            shell_surface->host_window()->bounds().ToString());
}

TEST_F(ShellSurfaceTest, SetMinimumSize) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  surface->Attach(buffer.get());
  surface->Commit();

  constexpr gfx::Size kSizes[] = {{50, 50}, {100, 50}};
  for (const gfx::Size size : kSizes) {
    SCOPED_TRACE(
        base::StringPrintf("MinSize=%dx%d", size.width(), size.height()));
    ConfigureData config_data;
    shell_surface->set_configure_callback(
        base::BindRepeating(&Configure, base::Unretained(&config_data)));

    shell_surface->SetMinimumSize(size);
    surface->Commit();
    EXPECT_EQ(size, shell_surface->GetMinimumSize());
    EXPECT_EQ(size, shell_surface->GetWidget()->GetMinimumSize());
    EXPECT_EQ(size, shell_surface->GetWidget()
                        ->GetNativeWindow()
                        ->delegate()
                        ->GetMinimumSize());
    gfx::Size expected_size(buffer_size);
    expected_size.set_width(std::max(buffer_size.width(), size.width()));
    EXPECT_EQ(expected_size,
              shell_surface->GetWidget()->GetWindowBoundsInScreen().size());
    if (buffer_size.width() > size.width())
      EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
    else
      EXPECT_EQ(expected_size, config_data.suggested_bounds.size());
  }
  // Reset configure callback because config_data is out of scope.
  shell_surface->set_configure_callback(base::NullCallback());
  // With frame.
  surface->SetFrame(SurfaceFrameType::NORMAL);
  for (const gfx::Size size : kSizes) {
    SCOPED_TRACE(base::StringPrintf("MinSize=%dx%d with frame", size.width(),
                                    size.height()));
    ConfigureData config_data;
    shell_surface->set_configure_callback(
        base::BindRepeating(&Configure, base::Unretained(&config_data)));

    const gfx::Size size_with_frame(size.width(), size.height() + 32);
    shell_surface->SetMinimumSize(size);
    surface->Commit();
    EXPECT_EQ(size, shell_surface->GetMinimumSize());
    EXPECT_EQ(size_with_frame, shell_surface->GetWidget()->GetMinimumSize());
    EXPECT_EQ(size_with_frame, shell_surface->GetWidget()
                                   ->GetNativeWindow()
                                   ->delegate()
                                   ->GetMinimumSize());
    gfx::Size expected_size(buffer_size);
    expected_size.set_width(std::max(buffer_size.width(), size.width()));
    if (buffer_size.width() > size.width())
      EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
    else
      EXPECT_EQ(expected_size, config_data.suggested_bounds.size());
    shell_surface->set_configure_callback(base::NullCallback());
  }
}

TEST_F(ShellSurfaceTest, SetMaximumSize) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  surface->Attach(buffer.get());
  surface->Commit();

  constexpr gfx::Size kSizes[] = {{300, 300}, {200, 300}};
  for (const gfx::Size size : kSizes) {
    SCOPED_TRACE(
        base::StringPrintf("MaxSize=%dx%d", size.width(), size.height()));
    ConfigureData config_data;
    shell_surface->set_configure_callback(
        base::BindRepeating(&Configure, base::Unretained(&config_data)));

    shell_surface->SetMaximumSize(size);
    surface->Commit();
    EXPECT_EQ(size, shell_surface->GetMaximumSize());
    gfx::Size expected_size(buffer_size);
    expected_size.set_width(std::min(size.width(), buffer_size.width()));
    EXPECT_EQ(expected_size,
              shell_surface->GetWidget()->GetWindowBoundsInScreen().size());

    if (buffer_size.width() < size.width())
      EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
    else
      EXPECT_EQ(expected_size, config_data.suggested_bounds.size());
  }
}

void PreClose(int* pre_close_count, int* close_count) {
  EXPECT_EQ(*pre_close_count, *close_count);
  (*pre_close_count)++;
}

void Close(int* pre_close_count, int* close_count) {
  (*close_count)++;
  EXPECT_EQ(*pre_close_count, *close_count);
}

TEST_F(ShellSurfaceTest, CloseCallback) {
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  int pre_close_call_count = 0;
  int close_call_count = 0;
  shell_surface->set_pre_close_callback(
      base::BindRepeating(&PreClose, base::Unretained(&pre_close_call_count),
                          base::Unretained(&close_call_count)));
  shell_surface->set_close_callback(
      base::BindRepeating(&Close, base::Unretained(&pre_close_call_count),
                          base::Unretained(&close_call_count)));

  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_EQ(0, pre_close_call_count);
  EXPECT_EQ(0, close_call_count);
  shell_surface->GetWidget()->Close();
  EXPECT_EQ(1, pre_close_call_count);
  EXPECT_EQ(1, close_call_count);
}

void DestroyShellSurface(std::unique_ptr<ShellSurface>* shell_surface) {
  shell_surface->reset();
}

TEST_F(ShellSurfaceTest, SurfaceDestroyedCallback) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->set_surface_destroyed_callback(
      base::BindOnce(&DestroyShellSurface, base::Unretained(&shell_surface)));

  surface->Commit();

  EXPECT_TRUE(shell_surface.get());
  surface.reset();
  EXPECT_FALSE(shell_surface.get());
}

TEST_F(ShellSurfaceTest, ConfigureCallback) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;

  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  gfx::Rect geometry(16, 16, 32, 32);
  shell_surface->SetGeometry(geometry);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Commit();
  EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize());

  gfx::Rect maximized_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // State change should be sent even if the content is not attached.
  // See crbug.com/1138978.
  shell_surface->Maximize();
  shell_surface->AcknowledgeConfigure(0);

  EXPECT_FALSE(config_data.suggested_bounds.IsEmpty());
  EXPECT_EQ(maximized_bounds.size(), config_data.suggested_bounds.size());
  EXPECT_EQ(chromeos::WindowStateType::kMaximized, config_data.state_type);

  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_EQ(maximized_bounds.size(), config_data.suggested_bounds.size());
  EXPECT_EQ(chromeos::WindowStateType::kMaximized, config_data.state_type);
  shell_surface->Restore();
  shell_surface->AcknowledgeConfigure(0);
  // It should be restored to the original geometry size.
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize());

  shell_surface->SetFullscreen(true);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(GetContext()->bounds().size(), config_data.suggested_bounds.size());
  EXPECT_EQ(chromeos::WindowStateType::kFullscreen, config_data.state_type);
  shell_surface->SetFullscreen(false);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize());

  shell_surface->GetWidget()->Activate();
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_TRUE(config_data.is_active);
  shell_surface->GetWidget()->Deactivate();
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_FALSE(config_data.is_active);

  EXPECT_FALSE(config_data.is_resizing);
  shell_surface->StartResize(HTBOTTOMRIGHT);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_TRUE(config_data.is_resizing);
}

TEST_F(ShellSurfaceTest, CreateMinimizedWindow) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;

  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  gfx::Rect geometry(0, 0, 1, 1);
  shell_surface->SetGeometry(geometry);
  shell_surface->Minimize();
  shell_surface->AcknowledgeConfigure(0);
  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());
  EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize());
}

TEST_F(ShellSurfaceTest, CreateMinimizedWindow2) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;

  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  gfx::Rect geometry(0, 0, 1, 1);
  shell_surface->SetGeometry(geometry);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Commit();
  EXPECT_TRUE(config_data.suggested_bounds.IsEmpty());
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize());

  shell_surface->Minimize();
  shell_surface->AcknowledgeConfigure(0);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Commit();

  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMinimized());
  EXPECT_EQ(geometry.size(), shell_surface->CalculatePreferredSize());

  // Once the initial empty size is sent in configure,
  // new configure should send the size requested.
  EXPECT_EQ(geometry.size(), config_data.suggested_bounds.size());
}

TEST_F(ShellSurfaceTest,
       CreateMaximizedWindowWithRestoreBoundsWithoutInitialBuffer) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;
  gfx::Size buffer_size(256, 256);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder(buffer_size)
          .SetNoRootBuffer()
          .BuildShellSurface();

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  Surface* root_surface = shell_surface->surface_for_testing();
  root_surface->Commit();
  shell_surface->AcknowledgeConfigure(0);

  // Ash may maximize the window.
  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_FALSE(shell_surface->GetWidget()->IsMaximized());

  shell_surface->Maximize();
  shell_surface->AcknowledgeConfigure(0);

  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());

  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  root_surface->Attach(buffer.get());

  gfx::Rect geometry_full(buffer_size);
  shell_surface->SetGeometry(geometry_full);

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  root_surface->Commit();
  shell_surface->AcknowledgeConfigure(0);

  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());

  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  EXPECT_TRUE(window_state->HasRestoreBounds());

  auto bounds = window_state->GetRestoreBoundsInParent();
  EXPECT_EQ(geometry_full.size(), bounds.size());
}

TEST_F(ShellSurfaceTest, CreateMaximizedWindowWithRestoreBounds) {
  // Must be before shell_surface so it outlives it, for shell_surface's
  // destructor calls Configure() referencing these 4 variables.
  ConfigureData config_data;
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  gfx::Rect geometry(0, 0, 1, 1);
  shell_surface->SetGeometry(geometry);
  shell_surface->Maximize();

  // Commit without contents should result in a configure callback with empty
  // suggested size as a mechanisms to ask the client size itself.
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->AcknowledgeConfigure(0);

  gfx::Rect geometry_full(0, 0, 100, 100);
  shell_surface->SetGeometry(geometry_full);

  surface->Commit();
  shell_surface->AcknowledgeConfigure(1);

  EXPECT_TRUE(shell_surface->GetWidget());
  EXPECT_TRUE(shell_surface->GetWidget()->IsMaximized());
  EXPECT_EQ(geometry_full.size(), shell_surface->CalculatePreferredSize());

  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  EXPECT_TRUE(window_state->HasRestoreBounds());

  auto bounds = window_state->GetRestoreBoundsInParent();
  EXPECT_EQ(geometry.width(), bounds.width());
  EXPECT_EQ(geometry.height(), bounds.height());
}

TEST_F(ShellSurfaceTest, ToggleFullscreen) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(buffer_size,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());
  shell_surface->Maximize();
  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().width(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());

  ash::WMEvent event(ash::WM_EVENT_TOGGLE_FULLSCREEN);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // Enter fullscreen mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);

  EXPECT_FALSE(HasBackdrop());
  EXPECT_EQ(GetContext()->bounds().ToString(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().ToString());

  // Leave fullscreen mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);
  EXPECT_FALSE(HasBackdrop());

  // Check that shell surface is maximized.
  EXPECT_EQ(GetContext()->bounds().width(),
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
}

TEST_F(ShellSurfaceTest, FrameColors) {
  std::unique_ptr<Surface> surface(new Surface);
  gfx::Size buffer_size(64, 64);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  shell_surface->OnSetFrameColors(SK_ColorRED, SK_ColorTRANSPARENT);
  surface->Commit();

  const ash::NonClientFrameViewAsh* frame =
      static_cast<const ash::NonClientFrameViewAsh*>(
          shell_surface->GetWidget()->non_client_view()->frame_view());

  // Test if colors set before initial commit are set.
  EXPECT_EQ(SK_ColorRED, frame->GetActiveFrameColorForTest());
  // Frame should be fully opaque.
  EXPECT_EQ(SK_ColorBLACK, frame->GetInactiveFrameColorForTest());

  shell_surface->OnSetFrameColors(SK_ColorTRANSPARENT, SK_ColorBLUE);
  EXPECT_EQ(SK_ColorBLACK, frame->GetActiveFrameColorForTest());
  EXPECT_EQ(SK_ColorBLUE, frame->GetInactiveFrameColorForTest());
}

TEST_F(ShellSurfaceTest, CycleSnap) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_EQ(buffer_size,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().size());

  ash::WMEvent event(ash::WM_EVENT_CYCLE_SNAP_PRIMARY);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  // Enter snapped mode.
  ash::WindowState::Get(window)->OnWMEvent(&event);

  EXPECT_EQ(GetContext()->bounds().width() / 2,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());

  surface->Attach(buffer.get());
  surface->Commit();

  // Commit shouldn't change widget bounds when snapped.
  EXPECT_EQ(GetContext()->bounds().width() / 2,
            shell_surface->GetWidget()->GetWindowBoundsInScreen().width());
}

TEST_F(ShellSurfaceTest, ShellSurfaceMaximize) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .BuildShellSurface();
  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  // Expect: Can't resize when max_size is set.
  EXPECT_FALSE(window_state->CanMaximize());
  EXPECT_FALSE(window_state->CanSnap());

  shell_surface->SetMaximumSize(gfx::Size(0, 0));
  shell_surface->root_surface()->Commit();

  // Expect: Can resize without a max_size.
  EXPECT_TRUE(window_state->CanMaximize());
  EXPECT_TRUE(window_state->CanSnap());
}

TEST_F(ShellSurfaceTest, ShellSurfaceMaxSizeResizabilityOnlyMaximise) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetMaximumSize(gfx::Size(10, 10))
          .SetMinimumSize(gfx::Size(0, 0))
          .BuildShellSurface();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      kMaximumSizeForResizabilityOnly, true);
  shell_surface->root_surface()->Commit();

  auto* window_state =
      ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow());

  // Expect: Can resize with max_size > min_size.
  EXPECT_TRUE(window_state->CanMaximize());
  EXPECT_TRUE(window_state->CanResize());
  EXPECT_TRUE(window_state->CanSnap());

  shell_surface->SetMaximumSize(gfx::Size(0, 0));
  shell_surface->root_surface()->Commit();

  // Expect: Can resize with max_size unset.
  EXPECT_TRUE(window_state->CanMaximize());
  EXPECT_TRUE(window_state->CanResize());
  EXPECT_TRUE(window_state->CanSnap());

  shell_surface->SetMaximumSize(gfx::Size(10, 10));
  shell_surface->SetMinimumSize(gfx::Size(10, 10));
  shell_surface->root_surface()->Commit();

  // Expect: Can't resize where max_size is set and max_size == min_size.
  EXPECT_FALSE(window_state->CanMaximize());
  EXPECT_FALSE(window_state->CanResize());
  EXPECT_FALSE(window_state->CanSnap());
}

TEST_F(ShellSurfaceTest, Transient) {
  gfx::Size buffer_size(256, 256);

  std::unique_ptr<Buffer> parent_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> parent_surface(new Surface);
  parent_surface->Attach(parent_buffer.get());
  std::unique_ptr<ShellSurface> parent_shell_surface(
      new ShellSurface(parent_surface.get()));
  parent_surface->Commit();

  std::unique_ptr<Buffer> child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> child_surface(new Surface);
  child_surface->Attach(child_buffer.get());
  std::unique_ptr<ShellSurface> child_shell_surface(
      new ShellSurface(child_surface.get()));
  // Importantly, a transient window has an associated application.
  child_surface->SetApplicationId("fake_app_id");
  child_surface->SetParent(parent_surface.get(), gfx::Point(50, 50));
  child_surface->Commit();

  aura::Window* parent_window =
      parent_shell_surface->GetWidget()->GetNativeWindow();
  aura::Window* child_window =
      child_shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(parent_window && child_window);

  // The visibility of transient windows is controlled by the parent.
  parent_window->Hide();
  EXPECT_FALSE(child_window->IsVisible());
  parent_window->Show();
  EXPECT_TRUE(child_window->IsVisible());
}

TEST_F(ShellSurfaceTest, X11Transient) {
  auto parent = test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  gfx::Point origin(50, 50);

  auto transient =
      CreateX11TransientShellSurface(parent.get(), gfx::Size(100, 100), origin);
  EXPECT_TRUE(transient->GetWidget()->movement_disabled());
  EXPECT_EQ(transient->GetWidget()->GetWindowBoundsInScreen().origin(), origin);
}

TEST_F(ShellSurfaceTest, Popup) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  std::unique_ptr<Buffer> popup_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> popup_surface(new Surface);
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  ASSERT_EQ(gfx::Rect(50, 50, 256, 256),
            popup_shell_surface->GetWidget()->GetWindowBoundsInScreen());

  // Verify that created shell surface is popup and has capture.
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface->GetWidget()->GetNativeWindow()->GetType());
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  // Setting frame type on popup should have no effect.
  popup_surface->SetFrame(SurfaceFrameType::NORMAL);
  EXPECT_FALSE(popup_shell_surface->frame_enabled());

  // ShellSurface can capture the event even after it is created.
  std::unique_ptr<Buffer> sub_popup_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> sub_popup_surface(new Surface);
  sub_popup_surface->Attach(sub_popup_buffer.get());
  std::unique_ptr<ShellSurface> sub_popup_shell_surface(CreatePopupShellSurface(
      sub_popup_surface.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  sub_popup_shell_surface->Grab();
  sub_popup_surface->Commit();
  ASSERT_EQ(gfx::Rect(100, 50, 256, 256),
            sub_popup_shell_surface->GetWidget()->GetWindowBoundsInScreen());
  aura::Window* target =
      sub_popup_shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP, target->GetType());
  // The capture should be on `sub_popup_shell_surface`.
  EXPECT_TRUE(IsCaptureWindow(sub_popup_shell_surface.get()));

  {
    // Mouse is on the top most popup.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                         gfx::Point(100, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(sub_popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // Move the mouse to the parent popup.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-25, 0),
                         gfx::Point(75, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // Move the mouse to the main window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-25, -25),
                         gfx::Point(75, 25), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }

  // Removing top most popup moves the grab to parent popup.
  sub_popup_shell_surface.reset();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  {
    // Targetting should still work.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(
        popup_shell_surface->GetWidget()->GetNativeWindow());
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, GainCaptureFromSiblingSubPopup) {
  // Test that in the following setup:
  //
  //     popup_shell_surface1     popup_shell_surface2
  //  (has grab, loses capture) (has grab, gains capture)
  //                 \                /
  //                popup_shell_surface
  //
  // when popup_shell_surface2 is added, capture is correctly transferred to it
  // from popup_shell_surface1.

  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  auto popup_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  auto popup_buffer1 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface1 = std::make_unique<Surface>();
  popup_surface1->Attach(popup_buffer1.get());
  std::unique_ptr<ShellSurface> popup_shell_surface1(CreatePopupShellSurface(
      popup_surface1.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  popup_shell_surface1->Grab();
  popup_surface1->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface1.get()));

  auto popup_buffer2 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface2 = std::make_unique<Surface>();
  popup_surface2->Attach(popup_buffer2.get());
  std::unique_ptr<ShellSurface> popup_shell_surface2(CreatePopupShellSurface(
      popup_surface2.get(), popup_shell_surface.get(), gfx::Point(50, 100)));
  popup_shell_surface2->Grab();
  popup_surface2->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface2.get()));
}

TEST_F(ShellSurfaceTest, GainCaptureFromNieceSubPopup) {
  // Test that in the following setup:
  //
  //    popup_shell_surface3
  //           (no grab)
  //              |
  //    popup_shell_surface2
  //  (has grab; loses capture)
  //              |
  //    popup_shell_surface1    popup_shell_surface4
  //         (no grab)        (has grab; gains capture)
  //                 \                /
  //                popup_shell_surface
  //
  // when popup_shell_surface4 is added, capture is correctly transferred to
  // it from popup_shell_surface2.

  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  auto popup_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  auto popup_buffer1 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface1 = std::make_unique<Surface>();
  popup_surface1->Attach(popup_buffer1.get());
  std::unique_ptr<ShellSurface> popup_shell_surface1(CreatePopupShellSurface(
      popup_surface1.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  popup_surface1->Commit();
  // Doesn't change capture.
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  auto popup_buffer2 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface2 = std::make_unique<Surface>();
  popup_surface2->Attach(popup_buffer2.get());
  std::unique_ptr<ShellSurface> popup_shell_surface2(CreatePopupShellSurface(
      popup_surface2.get(), popup_shell_surface1.get(), gfx::Point(150, 50)));
  popup_shell_surface2->Grab();
  popup_surface2->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface2.get()));

  auto popup_buffer3 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface3 = std::make_unique<Surface>();
  popup_surface3->Attach(popup_buffer3.get());
  std::unique_ptr<ShellSurface> popup_shell_surface3(CreatePopupShellSurface(
      popup_surface3.get(), popup_shell_surface2.get(), gfx::Point(200, 50)));
  popup_surface3->Commit();
  // Doesn't change capture.
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface2.get()));

  auto popup_buffer4 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface4 = std::make_unique<Surface>();
  popup_surface4->Attach(popup_buffer4.get());
  std::unique_ptr<ShellSurface> popup_shell_surface4(CreatePopupShellSurface(
      popup_surface4.get(), popup_shell_surface.get(), gfx::Point(50, 100)));
  popup_shell_surface4->Grab();
  popup_surface4->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface4.get()));
}

TEST_F(ShellSurfaceTest, GainCaptureFromDescendantSubPopup) {
  // Test that in the following setup:
  //
  //    popup_shell_surface3
  //  (has grab; loses capture)
  //              |
  //    popup_shell_surface2
  //          (no grab)
  //              |
  //    popup_shell_surface1
  //  (has grab; gains capture)
  //              |
  //    popup_shell_surface
  //
  // when popup_shell_surface3 is closed, capture is correctly transferred to
  // popup_shell_surface1.

  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  auto popup_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface.get()));

  auto popup_buffer1 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface1 = std::make_unique<Surface>();
  popup_surface1->Attach(popup_buffer1.get());
  std::unique_ptr<ShellSurface> popup_shell_surface1(CreatePopupShellSurface(
      popup_surface1.get(), popup_shell_surface.get(), gfx::Point(100, 50)));
  popup_shell_surface1->Grab();
  popup_surface1->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface1.get()));

  auto popup_buffer2 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface2 = std::make_unique<Surface>();
  popup_surface2->Attach(popup_buffer2.get());
  std::unique_ptr<ShellSurface> popup_shell_surface2(CreatePopupShellSurface(
      popup_surface2.get(), popup_shell_surface1.get(), gfx::Point(150, 50)));
  popup_surface2->Commit();
  // Doesn't change capture.
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface1.get()));

  auto popup_buffer3 = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface3 = std::make_unique<Surface>();
  popup_surface3->Attach(popup_buffer3.get());
  std::unique_ptr<ShellSurface> popup_shell_surface3(CreatePopupShellSurface(
      popup_surface3.get(), popup_shell_surface2.get(), gfx::Point(200, 50)));
  popup_shell_surface3->Grab();
  popup_surface3->Commit();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface3.get()));

  popup_shell_surface3.reset();
  EXPECT_TRUE(IsCaptureWindow(popup_shell_surface1.get()));
}

TEST_F(ShellSurfaceTest, Menu) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> menu_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, MenuOnPopup) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> popup_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(popup_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface->GetWidget()->GetNativeWindow()->GetType());

  std::unique_ptr<ShellSurface> menu_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(popup_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, PopupOnMenu) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> menu_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface->GetWidget()->GetNativeWindow()->GetType());

  std::unique_ptr<ShellSurface> popup_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetParent(menu_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(popup_shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, PopupOnPopup) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> popup_shell_surface_1 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(popup_shell_surface_1->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface_1->GetWidget()->GetNativeWindow()->GetType());

  std::unique_ptr<ShellSurface> popup_shell_surface_2 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetParent(popup_shell_surface_1.get())
          .BuildShellSurface();
  EXPECT_TRUE(popup_shell_surface_2->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface_2->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, MenuOnMenu) {
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  EXPECT_TRUE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> menu_shell_surface_1 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface_1->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface_1->GetWidget()->GetNativeWindow()->GetType());

  std::unique_ptr<ShellSurface> menu_shell_surface_2 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsMenu()
          .SetParent(menu_shell_surface_1.get())
          .BuildShellSurface();
  EXPECT_TRUE(menu_shell_surface_2->GetWidget()->IsVisible());
  EXPECT_EQ(aura::client::WINDOW_TYPE_MENU,
            menu_shell_surface_2->GetWidget()->GetNativeWindow()->GetType());
}

TEST_F(ShellSurfaceTest, PopupWithInputRegion) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));

  surface->Attach(buffer.get());
  surface->SetInputRegion(cc::Region());

  std::unique_ptr<Buffer> child_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> child_surface(new Surface);
  child_surface->Attach(child_buffer.get());

  auto subsurface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  subsurface->SetPosition(gfx::PointF(10, 10));
  child_surface->SetInputRegion(cc::Region(gfx::Rect(0, 0, 256, 2560)));
  child_surface->Commit();
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  std::unique_ptr<Buffer> popup_buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> popup_surface(new Surface);
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  ASSERT_EQ(gfx::Rect(50, 50, 256, 256),
            popup_shell_surface->GetWidget()->GetWindowBoundsInScreen());

  // Verify that created shell surface is popup and has capture.
  EXPECT_EQ(aura::client::WINDOW_TYPE_POPUP,
            popup_shell_surface->GetWidget()->GetNativeWindow()->GetType());
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            popup_shell_surface->GetWidget()->GetNativeWindow());

  // Setting frame type on popup should have no effect.
  popup_surface->SetFrame(SurfaceFrameType::NORMAL);
  EXPECT_FALSE(popup_shell_surface->frame_enabled());

  aura::Window* target = popup_shell_surface->GetWidget()->GetNativeWindow();

  {
    // Mouse is on the popup.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // If it matches the parent's sub surface, use it.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-25, 0),
                         gfx::Point(25, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(child_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  {
    // If it didnt't match any surface in the parent, fallback to
    // the popup's surface.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-50, 0),
                         gfx::Point(0, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
  popup_surface.reset();
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            nullptr);

  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(0, 0, 256, 256));
  shell_surface->GetWidget()->GetNativeWindow()->AddChild(window.get());
  window->Show();

  {
    // If non surface window covers the window,
    /// GetTargetSurfaceForLocatedEvent should return nullptr.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(0, 0),
                         gfx::Point(50, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(window.get());
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, PopupWithInvisibleParent) {
  // Invisible main window.
  std::unique_ptr<ShellSurface> root_shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetNoRootBuffer()
          .BuildShellSurface();
  EXPECT_FALSE(root_shell_surface->GetWidget()->IsVisible());

  std::unique_ptr<ShellSurface> popup_shell_surface_1 =
      test::ShellSurfaceBuilder({256, 256})
          .SetNoRootBuffer()
          .SetAsPopup()
          .SetDisableMovement()
          .SetParent(root_shell_surface.get())
          .BuildShellSurface();

  EXPECT_EQ(root_shell_surface->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                popup_shell_surface_1->GetWidget()->GetNativeWindow()));

  EXPECT_FALSE(popup_shell_surface_1->GetWidget()->IsVisible());

  // Create visible popup.
  std::unique_ptr<ShellSurface> popup_shell_surface_2 =
      test::ShellSurfaceBuilder({256, 256})
          .SetAsPopup()
          .SetDisableMovement()
          .SetParent(popup_shell_surface_1.get())
          .BuildShellSurface();

  EXPECT_TRUE(popup_shell_surface_2->GetWidget()->IsVisible());

  EXPECT_EQ(popup_shell_surface_1->GetWidget()->GetNativeWindow(),
            wm::GetTransientParent(
                popup_shell_surface_2->GetWidget()->GetNativeWindow()));
}

TEST_F(ShellSurfaceTest, Caption) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  aura::Window* target = shell_surface->GetWidget()->GetNativeWindow();
  target->SetCapture();
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            shell_surface->GetWidget()->GetNativeWindow());
  {
    // Move the mouse at the caption of the captured window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(5, 5), gfx::Point(5, 5),
                         ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse at the center of the captured window.
    gfx::Rect bounds = shell_surface->GetWidget()->GetWindowBoundsInScreen();
    gfx::Point center = bounds.CenterPoint();
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, center - bounds.OffsetFromOrigin(),
                         center, ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, DragMaximizedWindow) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));

  // Maximize the window.
  shell_surface->Maximize();

  chromeos::WindowStateType configured_state =
      chromeos::WindowStateType::kDefault;
  shell_surface->set_configure_callback(base::BindLambdaForTesting(
      [&](const gfx::Rect& bounds, chromeos::WindowStateType state,
          bool resizing, bool activated, const gfx::Vector2d& origin_offset,
          float raster_scale) {
        configured_state = state;
        return uint32_t{0};
      }));

  // Initiate caption bar dragging for a window.
  gfx::Size size = shell_surface->GetWidget()->GetWindowBoundsInScreen().size();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  // Move mouse pointer to caption bar.
  event_generator->MoveMouseTo(size.width() / 2, 2);
  constexpr int kDragAmount = 10;
  // Start dragging a window.
  event_generator->DragMouseBy(kDragAmount, kDragAmount);

  EXPECT_FALSE(shell_surface->GetWidget()->IsMaximized());
  // `WindowStateType` after dragging should be `kNormal`.
  EXPECT_EQ(chromeos::WindowStateType::kNormal, configured_state);
}

TEST_F(ShellSurfaceTest, CaptionWithPopup) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);

  auto popup_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto popup_surface = std::make_unique<Surface>();
  popup_surface->Attach(popup_buffer.get());
  std::unique_ptr<ShellSurface> popup_shell_surface(CreatePopupShellSurface(
      popup_surface.get(), shell_surface.get(), gfx::Point(50, 50)));
  popup_shell_surface->Grab();
  popup_surface->Commit();
  aura::Window* target = popup_shell_surface->GetWidget()->GetNativeWindow();
  EXPECT_EQ(WMHelper::GetInstance()->GetCaptureClient()->GetCaptureWindow(),
            target);
  {
    // Move the mouse at the popup window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(5, 5),
                         gfx::Point(55, 55), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(popup_surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse at the caption of the main window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-45, -45),
                         gfx::Point(5, 5), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(nullptr, GetTargetSurfaceForLocatedEvent(&event));
  }

  {
    // Move the mouse in the main window.
    ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(-25, 0),
                         gfx::Point(25, 50), ui::EventTimeForNow(), 0, 0);
    ui::Event::DispatcherApi(&event).set_target(target);
    EXPECT_EQ(surface.get(), GetTargetSurfaceForLocatedEvent(&event));
  }
}

TEST_F(ShellSurfaceTest, SkipImeProcessingPropagateToSurface) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();
  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 256, 256));
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);

  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();
  ASSERT_FALSE(window->GetProperty(aura::client::kSkipImeProcessing));
  ASSERT_FALSE(shell_surface->root_surface()->window()->GetProperty(
      aura::client::kSkipImeProcessing));

  window->SetProperty(aura::client::kSkipImeProcessing, true);
  EXPECT_TRUE(window->GetProperty(aura::client::kSkipImeProcessing));
  EXPECT_TRUE(shell_surface->root_surface()->window()->GetProperty(
      aura::client::kSkipImeProcessing));
}

TEST_F(ShellSurfaceTest, NotifyLeaveEnter) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).SetNoCommit().BuildShellSurface();

  auto func = [](int64_t* old_display_id, int64_t* new_display_id,
                 int64_t old_id, int64_t new_id) {
    DCHECK_EQ(0, *old_display_id);
    DCHECK_EQ(0, *new_display_id);
    *old_display_id = old_id;
    *new_display_id = new_id;
    return true;
  };

  int64_t old_display_id = 0, new_display_id = 0;

  shell_surface->root_surface()->set_leave_enter_callback(
      base::BindRepeating(func, &old_display_id, &new_display_id));

  // Creating a new shell surface should notify on which display
  // it is created.
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(display::kInvalidDisplayId, old_display_id);
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            new_display_id);

  // Attaching a 2nd display should not change where the surface
  // is located.
  old_display_id = 0;
  new_display_id = 0;
  UpdateDisplay("800x600, 800x600");
  EXPECT_EQ(0, old_display_id);
  EXPECT_EQ(0, new_display_id);

  // Move the window to 2nd display.
  shell_surface->GetWidget()->SetBounds(gfx::Rect(1000, 0, 256, 256));

  int64_t secondary_id =
      display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
          .GetSecondaryDisplay()
          .id();

  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            old_display_id);
  EXPECT_EQ(secondary_id, new_display_id);

  // Disconnect the display the surface is currently on.
  old_display_id = 0;
  new_display_id = 0;
  UpdateDisplay("800x600");
  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
            new_display_id);
  EXPECT_EQ(secondary_id, old_display_id);
}

// Make sure that the server side triggers resize when the
// set_server_start_resize is called, and the resize shadow is created for the
// window.
TEST_F(ShellSurfaceTest, ServerStartResize) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  auto* widget = shell_surface->GetWidget();

  gfx::Size size = widget->GetWindowBoundsInScreen().size();
  widget->SetBounds(gfx::Rect(size));

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(size.width() + 2, size.height() / 2);

  // Ash creates resize shadow for resizable exo window when the
  // server_start_resize is set.
  ash::ResizeShadow* resize_shadow =
      ash::Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          widget->GetNativeWindow());
  ASSERT_TRUE(resize_shadow);

  constexpr int kDragAmount = 10;
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(kDragAmount, 0);
  event_generator->ReleaseLeftButton();

  EXPECT_EQ(widget->GetWindowBoundsInScreen().size().width(),
            size.width() + kDragAmount);
}

TEST_F(ShellSurfaceTest, LacrosToggleAxisMaximize) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetOrigin({10, 10})
          .BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  auto* widget = shell_surface->GetWidget();
  gfx::Size size = widget->GetWindowBoundsInScreen().size();

  gfx::Rect restored_bounds = shell_surface->GetBoundsInScreen();

  ui::test::EventGenerator* event_generator = GetEventGenerator();

  // Move mouse to top middle and double click to vertically maximize.
  event_generator->MoveMouseTo(10 + size.width() / 2, 10);
  event_generator->DoubleClickLeftButton();

  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect bounds_in_screen = shell_surface->GetBoundsInScreen();

  EXPECT_EQ(restored_bounds.x(), bounds_in_screen.x());
  EXPECT_EQ(restored_bounds.width(), bounds_in_screen.width());
  EXPECT_EQ(work_area.y(), bounds_in_screen.y());
  EXPECT_EQ(work_area.height(), bounds_in_screen.height());

  // Move mouse to top middle and double click to vertically Restore.
  event_generator->MoveMouseTo(10 + size.width() / 2, 0);
  event_generator->DoubleClickLeftButton();
  bounds_in_screen = shell_surface->GetBoundsInScreen();
  EXPECT_EQ(restored_bounds, bounds_in_screen);
}

TEST_F(ShellSurfaceTest, ServerStartResizeComponent) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).SetNoCommit().BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  auto* widget = shell_surface->GetWidget();

  gfx::Size size = widget->GetWindowBoundsInScreen().size();
  widget->SetBounds(gfx::Rect(size));

  uint32_t serial = 0;
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, const gfx::Rect& bounds,
         chromeos::WindowStateType state_type, bool resizing, bool activated,
         const gfx::Vector2d& origin_offset,
         float raster_scale) { return ++(*serial_ptr); },
      &serial);

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(size.width() + 2, size.height() / 2);

  EXPECT_EQ(shell_surface->resize_component_for_test(), HTCAPTION);

  constexpr int kDragAmount = 10;
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(kDragAmount, 0);
  EXPECT_EQ(shell_surface->resize_component_for_test(), HTCAPTION);
  shell_surface->AcknowledgeConfigure(serial);
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(shell_surface->resize_component_for_test(), HTRIGHT);
  event_generator->ReleaseLeftButton();
  shell_surface->AcknowledgeConfigure(serial);
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(shell_surface->resize_component_for_test(), HTCAPTION);
}

// Make sure that dragging to another display will update the origin to
// correct value.
TEST_F(ShellSurfaceTest, UpdateBoundsWhenDraggedToAnotherDisplay) {
  UpdateDisplay("800x600, 800x600");
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).BuildShellSurface();
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  shell_surface->SetWindowBounds({0, 0, 64, 64});

  gfx::Point last_origin;
  auto origin_change = [&](const gfx::Point& p) { last_origin = p; };

  shell_surface->set_origin_change_callback(
      base::BindLambdaForTesting(origin_change));
  event_generator->MoveMouseTo(1, 1);
  event_generator->PressLeftButton();
  shell_surface->StartMove();
  event_generator->MoveMouseTo(801, 1);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(last_origin, gfx::Point(800, 0));
}

// Make sure that commit during window drag should not move the
// window to another display.
TEST_F(ShellSurfaceTest, CommitShouldNotMoveDisplay) {
  UpdateDisplay("800x600, 800x600");
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetOrigin({750, 0})
          .BuildShellSurface();
  auto* screen = display::Screen::GetScreen();
  auto* root_surface = shell_surface->root_surface();

  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            screen
                ->GetDisplayNearestWindow(
                    shell_surface->GetWidget()->GetNativeWindow())
                .id());

  shell_surface->StartMove();

  gfx::Size buffer_size(256, 256);
  auto new_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  root_surface->Attach(new_buffer.get());
  root_surface->Commit();

  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            screen
                ->GetDisplayNearestWindow(
                    shell_surface->GetWidget()->GetNativeWindow())
                .id());

  shell_surface->EndDrag();

  // Ending drag will not move the window unless the mouse cursor enters
  // another display.
  EXPECT_EQ(screen->GetPrimaryDisplay().id(),
            screen
                ->GetDisplayNearestWindow(
                    shell_surface->GetWidget()->GetNativeWindow())
                .id());
}

// Make sure that resize shadow does not update until commit when the window
// property |aura::client::kUseWindowBoundsForShadow| is false.
TEST_F(ShellSurfaceTest, ResizeShadowIndependentBounds) {
  constexpr gfx::Point kOrigin(10, 10);
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64})
          .SetOrigin(kOrigin)
          .BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  auto* widget = shell_surface->GetWidget();

  gfx::Size size = widget->GetWindowBoundsInScreen().size();
  widget->SetBounds(gfx::Rect(size));

  // Starts mouse event to make sure resize shadow is created.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(size.width() + 2, size.height() / 2);

  // Creates resize shadow and normal shadow for resizable exo window.
  ash::ResizeShadow* resize_shadow =
      ash::Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          widget->GetNativeWindow());
  ASSERT_TRUE(resize_shadow);
  shell_surface->root_surface()->SetFrame(SurfaceFrameType::SHADOW);
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(widget->GetNativeWindow()->GetProperty(
      aura::client::kUseWindowBoundsForShadow));

  ui::Shadow* normal_shadow =
      wm::ShadowController::GetShadowForWindow(widget->GetNativeWindow());
  ASSERT_TRUE(normal_shadow);

  // ash::ResizeShadow::InitParams set the default |thickness| to 8.
  constexpr int kResizeShadowThickness = 8;

  EXPECT_EQ(gfx::Size(size.width() + kResizeShadowThickness, size.height()),
            resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(size, normal_shadow->content_bounds().size());

  constexpr gfx::Rect kNewBounds(kOrigin, {100, 100});
  uint32_t serial = 0;
  auto configure_callback = base::BindRepeating(
      [](uint32_t* const serial_ptr, const gfx::Rect& bounds,
         chromeos::WindowStateType state_type, bool resizing, bool activated,
         const gfx::Vector2d& origin_offset,
         float raster_scale) { return ++(*serial_ptr); },
      &serial);

  shell_surface->set_configure_callback(configure_callback);

  // Resize the widget and set geometry.
  shell_surface->StartResize(HTBOTTOMRIGHT);
  shell_surface->SetWidgetBounds(kNewBounds, /*adjusted by server=*/false);
  shell_surface->SetGeometry(gfx::Rect(kNewBounds.size()));

  // Client acknowledge configure for resizing. Shadow sizes should not be
  // updated yet until commit.
  shell_surface->AcknowledgeConfigure(serial);
  EXPECT_EQ(gfx::Size(size.width() + kResizeShadowThickness, size.height()),
            resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(size, normal_shadow->content_bounds().size());

  // Normal and resize shadow sizes are updated after commit.
  shell_surface->root_surface()->Commit();
  EXPECT_EQ(gfx::Size(kNewBounds.width() + kResizeShadowThickness,
                      kNewBounds.height()),
            resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(kNewBounds.size(), normal_shadow->content_bounds().size());

  // Explicitly ends the drag here.
  ash::Shell::Get()->toplevel_window_event_handler()->CompleteDragForTesting(
      ash::ToplevelWindowEventHandler::DragResult::SUCCESS);
  // Hide Shadow
  event_generator->MoveMouseTo({0, 0});

  EXPECT_FALSE(resize_shadow->visible());

  // Move
  UpdateDisplay("800x600,1200x1000");
  shell_surface->GetWidget()->SetBounds({{1000, 100}, kNewBounds.size()});

  shell_surface->AcknowledgeConfigure(serial);
  shell_surface->root_surface()->Commit();

  auto* screen = display::Screen::GetScreen();
  int64_t secondary_id =
      display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
          .GetSecondaryDisplay()
          .id();
  ASSERT_EQ(secondary_id, screen
                              ->GetDisplayNearestWindow(
                                  shell_surface->GetWidget()->GetNativeWindow())
                              .id());

  // Use outside to start drag because exo consumes events inside.
  event_generator->MoveMouseTo({999, 99});

  gfx::Rect bounds = shell_surface->GetWidget()->GetNativeWindow()->bounds();

  EXPECT_TRUE(resize_shadow->visible());
  gfx::Rect expected_shadow_on_secondary(
      bounds.x() - kResizeShadowThickness, bounds.y() - kResizeShadowThickness,
      bounds.width() + kResizeShadowThickness,
      bounds.height() + kResizeShadowThickness);
  EXPECT_EQ(expected_shadow_on_secondary,
            resize_shadow->GetLayerForTest()->bounds());

  constexpr gfx::Rect kResizedBoundsOn2nd{950, 50, 150, 150};
  shell_surface->StartResize(HTTOPLEFT);
  shell_surface->GetWidget()->SetBounds(kResizedBoundsOn2nd);
  shell_surface->SetGeometry(gfx::Rect(kResizedBoundsOn2nd.size()));
  shell_surface->AcknowledgeConfigure(serial);

  EXPECT_EQ(expected_shadow_on_secondary,
            resize_shadow->GetLayerForTest()->bounds());

  shell_surface->root_surface()->Commit();
  constexpr gfx::Rect kExpectedShadowBoundsOn2nd(
      150 - kResizeShadowThickness, 50 - kResizeShadowThickness,
      kResizedBoundsOn2nd.width() + kResizeShadowThickness,
      kResizedBoundsOn2nd.height() + kResizeShadowThickness);

  EXPECT_EQ(kExpectedShadowBoundsOn2nd,
            resize_shadow->GetLayerForTest()->bounds());
}

// Make sure that resize shadow updates as soon as widget bounds change when
// the window property |aura::client::kUseWindowBoundsForShadow| is false.
TEST_F(ShellSurfaceTest, ResizeShadowDependentBounds) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).BuildShellSurface();
  shell_surface->OnSetServerStartResize();
  shell_surface->root_surface()->Commit();
  ASSERT_TRUE(shell_surface->GetWidget());

  auto* widget = shell_surface->GetWidget();

  gfx::Size size = widget->GetWindowBoundsInScreen().size();
  widget->SetBounds(gfx::Rect(size));

  // Starts mouse event to make sure resize shadow is created.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(size.width() + 2, size.height() / 2);

  // Creates resize shadow and normal shadow for resizable exo window.
  ash::ResizeShadow* resize_shadow =
      ash::Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          widget->GetNativeWindow());
  ASSERT_TRUE(resize_shadow);
  shell_surface->root_surface()->SetFrame(SurfaceFrameType::SHADOW);
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(widget->GetNativeWindow()->GetProperty(
      aura::client::kUseWindowBoundsForShadow));
  // Override the property to update the shadow bounds immediately.
  widget->GetNativeWindow()->SetProperty(
      aura::client::kUseWindowBoundsForShadow, true);

  ui::Shadow* normal_shadow =
      wm::ShadowController::GetShadowForWindow(widget->GetNativeWindow());
  ASSERT_TRUE(normal_shadow);

  // ash::ResizeShadow::InitParams set the default |thickness| to 8.
  const int kResizeShadowThickness = 8;

  EXPECT_EQ(gfx::Size(size.width() + kResizeShadowThickness, size.height()),
            resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(size, normal_shadow->content_bounds().size());

  gfx::Size new_size(100, 100);
  gfx::Rect new_bounds(new_size);

  // Resize the widget and set geometry.
  shell_surface->StartResize(HTBOTTOMRIGHT);
  shell_surface->SetWidgetBounds(new_bounds, /*adjusted_by_server=*/false);
  shell_surface->SetGeometry(new_bounds);
  // Shadow bounds are updated as soon as the widget bounds change.
  EXPECT_EQ(
      gfx::Size(new_size.width() + kResizeShadowThickness, new_size.height()),
      resize_shadow->GetLayerForTest()->bounds().size());
  EXPECT_EQ(new_size, normal_shadow->content_bounds().size());
}

TEST_F(ShellSurfaceTest, PropertyResolverTest) {
  class TestPropertyResolver : public exo::WMHelper::AppPropertyResolver {
   public:
    TestPropertyResolver() = default;
    ~TestPropertyResolver() override = default;
    void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) override {
      if (expected_app_id == params.app_id) {
        out_properties_container.AcquireAllPropertiesFrom(
            std::move(params.for_creation ? properties_for_creation
                                          : properties_after_creation));
      }
    }
    std::string expected_app_id;
    ui::PropertyHandler properties_for_creation;
    ui::PropertyHandler properties_after_creation;
  };
  std::unique_ptr<TestPropertyResolver> resolver_holder =
      std::make_unique<TestPropertyResolver>();
  auto* resolver = resolver_holder.get();
  WMHelper::GetInstance()->RegisterAppPropertyResolver(
      std::move(resolver_holder));

  resolver->properties_for_creation.SetProperty(ash::kShelfItemTypeKey, 1);
  resolver->properties_after_creation.SetProperty(ash::kShelfItemTypeKey, 2);
  resolver->expected_app_id = "test";

  // Make sure that properties are properly populated for both
  // "before widget creation", and "after widget creation".
  {
    // TODO(oshima): create a test API to create a shell surface.
    gfx::Size buffer_size(256, 256);
    auto buffer = std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
    auto surface = std::make_unique<Surface>();
    auto shell_surface = std::make_unique<ShellSurface>(surface.get());

    surface->SetApplicationId("test");
    surface->Attach(buffer.get());
    surface->Commit();
    EXPECT_EQ(1, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));
    surface->SetApplicationId("test");
    EXPECT_EQ(2, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));
  }

  // Make sure that properties are will not be popluated when the app ids
  // mismatch "before" and "after" widget is created.
  resolver->properties_for_creation.SetProperty(ash::kShelfItemTypeKey, 1);
  resolver->properties_after_creation.SetProperty(ash::kShelfItemTypeKey, 2);
  {
    gfx::Size buffer_size(256, 256);
    auto buffer = std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
    auto surface = std::make_unique<Surface>();
    auto shell_surface = std::make_unique<ShellSurface>(surface.get());

    surface->SetApplicationId("testx");
    surface->Attach(buffer.get());
    surface->Commit();
    EXPECT_NE(1, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));

    surface->SetApplicationId("testy");
    surface->Attach(buffer.get());
    surface->Commit();
    EXPECT_NE(1, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));
    EXPECT_NE(2, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));

    // Updating to the matching |app_id| should set the window property.
    surface->SetApplicationId("test");
    EXPECT_EQ(2, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                     ash::kShelfItemTypeKey));
  }
}

TEST_F(ShellSurfaceTest, Overlay) {
  auto shell_surface =
      test::ShellSurfaceBuilder({100, 100}).BuildShellSurface();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kSkipImeProcessing, true);

  EXPECT_FALSE(shell_surface->HasOverlay());

  auto textfield = std::make_unique<views::Textfield>();
  auto* textfield_ptr = textfield.get();

  ShellSurfaceBase::OverlayParams params(std::move(textfield));
  shell_surface->AddOverlay(std::move(params));
  EXPECT_TRUE(shell_surface->HasOverlay());
  EXPECT_NE(shell_surface->GetWidget()->GetFocusManager()->GetFocusedView(),
            textfield_ptr);

  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());
  EXPECT_EQ(gfx::Size(100, 100), shell_surface->overlay_widget_for_testing()
                                     ->GetWindowBoundsInScreen()
                                     .size());

  PressAndReleaseKey(ui::VKEY_X);
  EXPECT_EQ(textfield_ptr->GetText(), u"");

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseToCenterOf(shell_surface->GetWidget()->GetNativeWindow());
  generator->ClickLeftButton();

  // Test normal key input, which should go through IME.
  EXPECT_EQ(shell_surface->GetWidget()->GetFocusManager()->GetFocusedView(),
            textfield_ptr);
  PressAndReleaseKey(ui::VKEY_X);
  EXPECT_EQ(textfield_ptr->GetText(), u"x");
  EXPECT_TRUE(textfield_ptr->GetSelectedText().empty());

  // Controls (Select all) should work.
  PressAndReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);

  EXPECT_EQ(textfield_ptr->GetText(), u"x");
  EXPECT_EQ(textfield_ptr->GetSelectedText(), u"x");

  auto* widget = ash::TestWidgetBuilder()
                     .SetBounds(gfx::Rect(200, 200))
                     .BuildOwnedByNativeWidget();
  ASSERT_TRUE(widget->IsActive());

  PressAndReleaseKey(ui::VKEY_Y);

  EXPECT_EQ(textfield_ptr->GetText(), u"x");
  EXPECT_EQ(textfield_ptr->GetSelectedText(), u"x");

  // Re-activate the surface and make sure that the overlay can still handle
  // keys.
  shell_surface->GetWidget()->Activate();
  // The current text will be replaced with new character because
  // the text is selected.
  PressAndReleaseKey(ui::VKEY_Y);
  EXPECT_EQ(textfield_ptr->GetText(), u"y");
  EXPECT_TRUE(textfield_ptr->GetSelectedText().empty());
}

TEST_F(ShellSurfaceTest, OverlayOverlapsFrame) {
  auto shell_surface =
      test::ShellSurfaceBuilder({100, 100}).BuildShellSurface();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kSkipImeProcessing, true);
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);

  EXPECT_FALSE(shell_surface->HasOverlay());

  ShellSurfaceBase::OverlayParams params(std::make_unique<views::View>());
  params.overlaps_frame = false;
  shell_surface->AddOverlay(std::move(params));
  EXPECT_TRUE(shell_surface->HasOverlay());

  {
    gfx::Size overlay_size =
        shell_surface->GetWidget()->GetWindowBoundsInScreen().size();
    overlay_size.set_height(overlay_size.height() - views::kCaptionButtonWidth);
    EXPECT_EQ(overlay_size, shell_surface->overlay_widget_for_testing()
                                ->GetWindowBoundsInScreen()
                                .size());
  }

  shell_surface->OnSetFrame(SurfaceFrameType::NONE);
  {
    gfx::Size overlay_size =
        shell_surface->GetWidget()->GetWindowBoundsInScreen().size();
    EXPECT_EQ(overlay_size, shell_surface->overlay_widget_for_testing()
                                ->GetWindowBoundsInScreen()
                                .size());
  }
}

TEST_F(ShellSurfaceTest, OverlayCanResize) {
  auto shell_surface =
      test::ShellSurfaceBuilder({100, 100}).BuildShellSurface();
  shell_surface->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kSkipImeProcessing, true);
  shell_surface->OnSetFrame(SurfaceFrameType::NORMAL);

  EXPECT_FALSE(shell_surface->HasOverlay());
  {
    ShellSurfaceBase::OverlayParams params(std::make_unique<views::View>());
    params.can_resize = false;
    shell_surface->AddOverlay(std::move(params));
  }
  EXPECT_FALSE(shell_surface->GetWidget()->widget_delegate()->CanResize());

  shell_surface->RemoveOverlay();
  {
    ShellSurfaceBase::OverlayParams params(std::make_unique<views::View>());
    params.can_resize = true;
    shell_surface->AddOverlay(std::move(params));
  }
  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());
}

class TestWindowObserver : public WMHelper::ExoWindowObserver {
 public:
  TestWindowObserver() {}

  TestWindowObserver(const TestWindowObserver&) = delete;
  TestWindowObserver& operator=(const TestWindowObserver&) = delete;

  // WMHelper::ExoWindowObserver overrides
  void OnExoWindowCreated(aura::Window* window) override {
    windows_.push_back(window);
  }

  const std::vector<aura::Window*>& observed_windows() { return windows_; }

 private:
  std::vector<aura::Window*> windows_;
};

TEST_F(ShellSurfaceTest, NotifyOnWindowCreation) {
  auto shell_surface =
      test::ShellSurfaceBuilder({100, 100}).SetNoCommit().BuildShellSurface();

  TestWindowObserver observer;
  WMHelper::GetInstance()->AddExoWindowObserver(&observer);

  // Committing a surface triggers window creation if it isn't already attached
  // to the root.
  shell_surface->surface_for_testing()->Commit();

  EXPECT_EQ(1u, observer.observed_windows().size());
}

TEST_F(ShellSurfaceTest, Reparent) {
  auto shell_surface1 = test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  views::Widget* widget1 = shell_surface1->GetWidget();

  // Create a second window.
  auto shell_surface2 = test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  views::Widget* widget2 = shell_surface2->GetWidget();

  auto child_shell_surface =
      test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  child_shell_surface->SetParent(shell_surface1.get());
  views::Widget* child_widget = child_shell_surface->GetWidget();
  // By default, a child widget is not activatable. Explicitly make it
  // activatable so that calling child_surface->RequestActivation() is
  // possible.
  child_widget->widget_delegate()->SetCanActivate(true);

  GrantPermissionToActivateIndefinitely(widget1->GetNativeWindow());
  GrantPermissionToActivateIndefinitely(widget2->GetNativeWindow());
  GrantPermissionToActivateIndefinitely(child_widget->GetNativeWindow());

  shell_surface2->Activate();
  EXPECT_FALSE(child_widget->ShouldPaintAsActive());
  EXPECT_FALSE(widget1->ShouldPaintAsActive());
  EXPECT_TRUE(widget2->ShouldPaintAsActive());

  child_shell_surface->Activate();
  // A widget should have the same paint-as-active state with its parent.
  EXPECT_TRUE(child_widget->ShouldPaintAsActive());
  EXPECT_TRUE(widget1->ShouldPaintAsActive());
  EXPECT_FALSE(widget2->ShouldPaintAsActive());

  // Reparent child to widget2.
  child_shell_surface->SetParent(shell_surface2.get());
  EXPECT_TRUE(child_widget->ShouldPaintAsActive());
  EXPECT_TRUE(widget2->ShouldPaintAsActive());
  EXPECT_FALSE(widget1->ShouldPaintAsActive());

  shell_surface1->Activate();
  EXPECT_FALSE(child_widget->ShouldPaintAsActive());
  EXPECT_FALSE(widget2->ShouldPaintAsActive());
  EXPECT_TRUE(widget1->ShouldPaintAsActive());

  // Delete widget1 (i.e. the non-parent widget) shouldn't crash.
  widget1->Close();
  shell_surface1.reset();

  child_shell_surface->Activate();
  EXPECT_TRUE(child_widget->ShouldPaintAsActive());
  EXPECT_TRUE(widget2->ShouldPaintAsActive());
}

TEST_F(ShellSurfaceTest, ThrottleFrameRate) {
  auto shell_surface = test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  SurfaceObserverForTest observer;
  shell_surface->root_surface()->AddSurfaceObserver(&observer);
  aura::Window* window = shell_surface->GetWidget()->GetNativeWindow();

  EXPECT_CALL(observer, ThrottleFrameRate(true));
  window->SetProperty(ash::kFrameRateThrottleKey, true);

  EXPECT_CALL(observer, ThrottleFrameRate(false));
  window->SetProperty(ash::kFrameRateThrottleKey, false);

  shell_surface->root_surface()->RemoveSurfaceObserver(&observer);
}

namespace {

struct ShellSurfaceCallbacks {
  struct ConfigureState {
    gfx::Rect bounds;
    chromeos::WindowStateType state_type;
    bool resizing;
    bool activated;
  };

  uint32_t OnConfigure(const gfx::Rect& bounds,
                       chromeos::WindowStateType state_type,
                       bool resizing,
                       bool activated,
                       const gfx::Vector2d& origin_offset,
                       float raster_scale) {
    configure_state.emplace();
    *configure_state = {bounds, state_type, resizing, activated};
    return ++serial;
  }
  void OnOriginChange(const gfx::Point& origin_) { origin = origin_; }
  void Reset() {
    configure_state.reset();
    origin.reset();
  }
  absl::optional<ConfigureState> configure_state;
  absl::optional<gfx::Point> origin;
  int32_t serial = 0;
};

}  // namespace

TEST_F(ShellSurfaceTest, DragWithHTCLIENT) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({64, 64}).BuildShellSurface();
  ShellSurfaceCallbacks callbacks;
  shell_surface->set_configure_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnConfigure, base::Unretained(&callbacks)));
  ash::WindowState::Get(shell_surface->GetWidget()->GetNativeWindow())
      ->CreateDragDetails(gfx::PointF(0, 0), HTCLIENT,
                          ::wm::WINDOW_MOVE_SOURCE_TOUCH);

  shell_surface->GetWidget()->SetBounds(gfx::Rect(0, 0, 80, 80));
  shell_surface->AcknowledgeConfigure(callbacks.serial);
  shell_surface->root_surface()->Commit();
}

TEST_F(ShellSurfaceTest, ScreenCoordinates) {
  auto shell_surface = test::ShellSurfaceBuilder({20, 20}).BuildShellSurface();
  ShellSurfaceCallbacks callbacks;

  shell_surface->set_configure_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnConfigure, base::Unretained(&callbacks)));
  shell_surface->set_origin_change_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnOriginChange, base::Unretained(&callbacks)));

  shell_surface->SetWindowBounds(gfx::Rect(10, 10, 300, 300));
  ASSERT_TRUE(!!callbacks.configure_state);
  ASSERT_TRUE(!callbacks.origin);
  EXPECT_EQ(callbacks.configure_state->bounds, gfx::Rect(10, 10, 300, 300));

  callbacks.Reset();
  shell_surface->SetWindowBounds(gfx::Rect(100, 100, 300, 300));
  ASSERT_TRUE(!callbacks.configure_state);
  ASSERT_TRUE(!!callbacks.origin);
  EXPECT_EQ(*callbacks.origin, gfx::Point(100, 100));

  callbacks.Reset();
  shell_surface->SetWindowBounds(gfx::Rect(0, 0, 300000, 300000));
  ASSERT_TRUE(!!callbacks.configure_state);
  EXPECT_EQ(callbacks.configure_state->bounds,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
}

TEST_F(ShellSurfaceTest, InitialBounds) {
  {
    gfx::Size size(20, 30);
    auto shell_surface =
        test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();

    EXPECT_FALSE(shell_surface->GetWidget());
    gfx::Rect bounds(gfx::Point(35, 135), size);
    shell_surface->SetWindowBounds(bounds);
    shell_surface->root_surface()->Commit();

    ASSERT_TRUE(shell_surface->GetWidget());
    EXPECT_EQ(bounds, shell_surface->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    // Requesting larger than display work area bounds should not be allowed.
    gfx::Size size(2000, 3000);
    auto shell_surface =
        test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();

    EXPECT_FALSE(shell_surface->GetWidget());
    gfx::Rect bounds(gfx::Point(35, 135), size);
    shell_surface->SetWindowBounds(bounds);
    shell_surface->root_surface()->Commit();

    ASSERT_TRUE(shell_surface->GetWidget());
    EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().work_area(),
              shell_surface->GetWidget()->GetWindowBoundsInScreen());
  }
}

// Make sure that the centering logic can use the correct size
// even if there is a pending configure.
TEST_F(ShellSurfaceTest, InitialCenteredBoundsWithConfigure) {
  auto shell_surface = test::ShellSurfaceBuilder(gfx::Size(0, 0))
                           .SetNoRootBuffer()
                           .SetNoCommit()
                           .BuildShellSurface();
  ShellSurfaceCallbacks callbacks;
  shell_surface->set_configure_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnConfigure, base::Unretained(&callbacks)));
  shell_surface->root_surface()->Commit();
  EXPECT_FALSE(shell_surface->GetWidget()->IsVisible());

  gfx::Size size(256, 256);
  auto new_buffer =
      std::make_unique<Buffer>(exo_test_helper()->CreateGpuMemoryBuffer(size));
  shell_surface->root_surface()->Attach(new_buffer.get());
  shell_surface->root_surface()->Commit();
  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());

  gfx::Rect expected =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  expected.ClampToCenteredSize(size);
  EXPECT_EQ(expected, shell_surface->GetWidget()->GetWindowBoundsInScreen());
}

// Test that restore info is set correctly.
TEST_F(ShellSurfaceTest, SetRestoreInfo) {
  int32_t restore_session_id = 200;
  int32_t restore_window_id = 100;

  gfx::Size size(20, 30);
  auto shell_surface =
      test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();

  shell_surface->SetRestoreInfo(restore_session_id, restore_window_id);
  shell_surface->Restore();
  shell_surface->root_surface()->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());
  EXPECT_EQ(restore_session_id,
            shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                app_restore::kWindowIdKey));
  EXPECT_EQ(restore_window_id,
            shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                app_restore::kRestoreWindowIdKey));
}

// Test that restore id is set correctly.
TEST_F(ShellSurfaceTest, SetRestoreInfoWithWindowIdSource) {
  int32_t restore_session_id = 200;
  const std::string app_id = "app_id";

  gfx::Size size(20, 30);
  auto shell_surface =
      test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();

  shell_surface->SetRestoreInfoWithWindowIdSource(restore_session_id, app_id);
  shell_surface->Restore();
  shell_surface->root_surface()->Commit();

  EXPECT_TRUE(shell_surface->GetWidget()->IsVisible());

  // FetchRestoreWindowId will return 0, because no app with id "app_id" is
  // installed.
  EXPECT_EQ(0, shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                   app_restore::kRestoreWindowIdKey));
  EXPECT_EQ(app_id, *shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                        app_restore::kAppIdKey));
}

// Surfaces without non-client view should not crash.
TEST_F(ShellSurfaceTest, NoNonClientViewWithConfigure) {
  // Popup windows don't have a non-client view.
  auto shell_surface =
      test::ShellSurfaceBuilder({20, 20}).SetAsPopup().BuildShellSurface();
  ShellSurfaceCallbacks callbacks;

  // Having a configure callback leads to a call to GetClientBoundsInScreen().
  shell_surface->set_configure_callback(base::BindRepeating(
      &ShellSurfaceCallbacks::OnConfigure, base::Unretained(&callbacks)));

  shell_surface->SetWindowBounds(gfx::Rect(10, 10, 300, 300));
  EXPECT_TRUE(callbacks.configure_state);
  EXPECT_EQ(callbacks.configure_state->bounds, gfx::Rect(10, 10, 300, 300));
}

TEST_F(ShellSurfaceTest, WindowIsResizableWithEmptySizeConstraints) {
  auto shell_surface = test::ShellSurfaceBuilder({20, 20})
                           .SetMinimumSize(gfx::Size(0, 0))
                           .SetMaximumSize(gfx::Size(0, 0))
                           .BuildShellSurface();
  EXPECT_TRUE(shell_surface->GetWidget()->widget_delegate()->CanResize());
}

TEST_F(ShellSurfaceTest, SetSystemModal) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetUseSystemModalContainer()
          .SetNoCommit()
          .BuildShellSurface();

  shell_surface->SetSystemModal(true);
  shell_surface->root_surface()->Commit();

  EXPECT_EQ(ui::MODAL_TYPE_SYSTEM, shell_surface->GetModalType());
  EXPECT_FALSE(shell_surface->frame_enabled());
}

TEST_F(ShellSurfaceTest, PipInitialPosition) {
  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256})
          .SetUseSystemModalContainer()
          .SetNoCommit()
          .BuildShellSurface();
  shell_surface->SetWindowBounds(gfx::Rect(20, 20, 256, 256));
  shell_surface->SetPip();
  shell_surface->root_surface()->Commit();
  // PIP positioner place the PIP window to the edge that is closer to the given
  // position
  EXPECT_EQ(gfx::Rect(8, 20, 256, 256),
            shell_surface->GetWidget()->GetWindowBoundsInScreen());
}

TEST_F(ShellSurfaceTest, PostWindowChangeCallback) {
  chromeos::WindowStateType state_type = chromeos::WindowStateType::kDefault;
  auto test_callback = base::BindRepeating(
      [](chromeos::WindowStateType* state_type, const gfx::Rect&,
         chromeos::WindowStateType new_type, bool, bool, const gfx::Vector2d&,
         float) -> uint32_t {
        *state_type = new_type;
        return 0;
      },
      &state_type);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({256, 256}).BuildShellSurface();

  shell_surface->set_configure_callback(test_callback);

  auto* state = ash::WindowState::Get(
      shell_surface->GetWidget()->GetNativeWindow()->GetToplevelWindow());

  // Make sure we are in a non-snapped state before testing state change.
  ASSERT_FALSE(state->IsSnapped());

  auto snap_event = std::make_unique<ash::WMEvent>(ash::WM_EVENT_SNAP_PRIMARY);

  // Trigger a snap event, this should cause a configure event.
  state->OnWMEvent(snap_event.get());

  EXPECT_EQ(state_type, chromeos::WindowStateType::kPrimarySnapped);
}

// A single configuration event should be sent when both the bounds and the
// window state change.
TEST_F(ShellSurfaceTest, ConfigureOnlySentOnceForBoundsAndWindowStateChange) {
  int times_configured = 0;
  auto test_callback = base::BindRepeating(
      [](int* times_configured, const gfx::Rect&,
         chromeos::WindowStateType new_type, bool, bool, const gfx::Vector2d&,
         float) -> uint32_t {
        ++(*times_configured);
        return 0;
      },
      &times_configured);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({1, 1}).BuildShellSurface();

  shell_surface->set_configure_callback(test_callback);

  auto* state = ash::WindowState::Get(
      shell_surface->GetWidget()->GetNativeWindow()->GetToplevelWindow());

  // Make sure we are in normal mode. Maximizing from this state should result
  // in BOTH the bounds and state changing.
  ASSERT_TRUE(state->IsNormalStateType());

  auto maximize_event = std::make_unique<ash::WMEvent>(ash::WM_EVENT_MAXIMIZE);

  // Trigger a snap event, this should cause a configure event.
  state->OnWMEvent(maximize_event.get());

  // The bounds change event should have been suppressed because the window
  // state is changing.
  EXPECT_EQ(times_configured, 1);
}

TEST_F(ShellSurfaceTest, SetImmersiveModeTriggersConfigure) {
  int times_configured = 0;
  auto test_callback = base::BindRepeating(
      [](int* times_configured, const gfx::Rect&,
         chromeos::WindowStateType new_type, bool, bool, const gfx::Vector2d&,
         float) -> uint32_t {
        ++(*times_configured);
        return 0;
      },
      &times_configured);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder({1, 1}).BuildShellSurface();

  shell_surface->set_configure_callback(test_callback);

  shell_surface->SetUseImmersiveForFullscreen(true);

  EXPECT_EQ(times_configured, 1);
}

TEST_F(ShellSurfaceTest,
       SetRasterScaleWindowPropertyConfiguresRasterScaleAndWaitsForAck) {
  ConfigureData config_data;
  constexpr gfx::Size buffer_size(256, 256);

  std::unique_ptr<ShellSurface> shell_surface =
      test::ShellSurfaceBuilder(buffer_size).BuildShellSurface();

  shell_surface->set_configure_callback(
      base::BindRepeating(&Configure, base::Unretained(&config_data)));

  auto* window = shell_surface->GetWidget()->GetNativeWindow();
  window->SetProperty(aura::client::kRasterScale, 0.1f);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(0.1f, config_data.raster_scale);

  window->SetProperty(aura::client::kRasterScale, 1.0f);
  shell_surface->AcknowledgeConfigure(0);
  EXPECT_EQ(1.0f, config_data.raster_scale);
}

TEST_F(ShellSurfaceTest, MoveParentWithoutWidget) {
  UpdateDisplay("800x600, 800x600");
  constexpr gfx::Size kSize{256, 256};
  std::unique_ptr<ShellSurface> parent_surface =
      test::ShellSurfaceBuilder(kSize).BuildShellSurface();

  std::unique_ptr<ShellSurface> child_surface =
      test::ShellSurfaceBuilder(kSize).SetNoCommit().BuildShellSurface();
  child_surface->SetParent(parent_surface.get());
  auto* parent_widget = parent_surface->GetWidget();
  auto* root_before = parent_widget->GetNativeWindow()->GetRootWindow();
  parent_widget->SetBounds({{1000, 0}, kSize});
  // Crash (crbug.com/1395433) happens when a transient parent moved
  // to another root window before widget is created. Make sure that
  // happened.
  EXPECT_NE(root_before, parent_widget->GetNativeWindow()->GetRootWindow());
}

}  // namespace exo
