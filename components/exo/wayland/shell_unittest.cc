// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "components/exo/wayland/wayland_display_output.h"

#include <xdg-shell-client-protocol.h>
#include <cstdint>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/wayland/test/client_util.h"
#include "components/exo/wayland/test/server_util.h"
#include "components/exo/wayland/test/wayland_server_test.h"

namespace exo::wayland {

namespace {

// A custom shell object which can act as xdg toplevel or remote surface.
// TODO(oshima): Implement more key events complete and move to a separate file.
class ShellClientData : public test::TestClient::CustomData {
 public:
  explicit ShellClientData(test::TestClient* client)
      : client_(client),
        surface_(wl_compositor_create_surface(client->compositor())) {}
  ~ShellClientData() override { Close(); }

  // Xdg Shell related methods.
  static void OnXdgToplevelClose(void* data, struct xdg_toplevel* toplevel) {
    static_cast<ShellClientData*>(data)->Close();
  }

  void CreateXdgToplevel() {
    constexpr xdg_toplevel_listener xdg_toplevel_listener = {
        [](void*, xdg_toplevel*, int32_t, int32_t, wl_array*) {},
        &OnXdgToplevelClose,
        [](void*, xdg_toplevel*, int32_t, int32_t) {},
        [](void*, xdg_toplevel*, wl_array*) {},
    };

    xdg_surface_.reset(
        xdg_wm_base_get_xdg_surface(client_->xdg_wm_base(), surface_.get()));
    xdg_toplevel_.reset(xdg_surface_get_toplevel(xdg_surface_.get()));
    xdg_toplevel_add_listener(xdg_toplevel_.get(), &xdg_toplevel_listener,
                              this);

    aura_toplevel_.reset(zaura_shell_get_aura_toplevel_for_xdg_toplevel(
        client_->aura_shell(), xdg_toplevel_.get()));
  }

  // Remote Shell related methods.
  static void OnRemoteSurfaceClose(void* data, zcr_remote_surface_v2*) {
    static_cast<ShellClientData*>(data)->Close();
  }

  void CreateRemoteSurface() {
    static constexpr zcr_remote_surface_v2_listener remote_surface_v2_listener =
        {
            &OnRemoteSurfaceClose,
            [](void*, zcr_remote_surface_v2*, uint32_t) {},
            [](void*, zcr_remote_surface_v2*, int, int, int, int) {},
            [](void*, zcr_remote_surface_v2*, uint32_t, uint32_t, int32_t,
               int32_t, int32_t, int32_t, uint32_t) {},
            [](void*, zcr_remote_surface_v2*, uint32_t) {},
            [](void*, zcr_remote_surface_v2*, int32_t, int32_t, int32_t) {},
            [](void*, zcr_remote_surface_v2*, int32_t) {},
            [](void*, zcr_remote_surface_v2*, wl_output*, int32_t, int32_t,
               int32_t, int32_t, uint32_t) {},
        };

    remote_surface_.reset(zcr_remote_shell_v2_get_remote_surface(
        client_->cr_remote_shell_v2(), surface_.get(),
        ZCR_REMOTE_SHELL_V2_CONTAINER_DEFAULT));
    zcr_remote_surface_v2_add_listener(remote_surface_.get(),
                                       &remote_surface_v2_listener, this);
  }

  void Pin() {
    zcr_remote_surface_v2_pin(remote_surface_.get(), /*trusted=*/true);
  }

  void UnsetSnap() { zaura_toplevel_unset_snap(aura_toplevel_.get()); }

  // Common to both xdg toplevel and remote surface.
  void CreateAndAttachBuffer(const gfx::Size& size) {
    buffer_ = client_->shm_buffer_factory()->CreateBuffer(0, size.width(),
                                                          size.height());
    wl_surface_attach(surface_.get(), buffer_->resource(), 0, 0);
  }

  void Commit() { wl_surface_commit(surface_.get()); }

  void DestroySurface() { wl_surface_destroy(surface_.release()); }

  void RequestWindowBounds(const gfx::Rect& bounds, wl_output* target_output) {
    zaura_toplevel_set_window_bounds(aura_toplevel_.get(), bounds.x(),
                                     bounds.y(), bounds.width(),
                                     bounds.height(), target_output);
  }

  void Close() {
    close_called_ = true;
    if (surface_) {
      wl_surface_attach(surface_.get(), nullptr, 0, 0);
    }
    if (buffer_) {
      wl_buffer_destroy(buffer_->GetResourceAndRelease());
    }
    buffer_.reset();
    if (xdg_toplevel_) {
      xdg_toplevel_destroy(xdg_toplevel_.release());
      xdg_surface_destroy(xdg_surface_.release());
    }
    if (aura_toplevel_) {
      zaura_toplevel_destroy(aura_toplevel_.release());
    }
    if (remote_surface_) {
      zcr_remote_surface_v2_destroy(remote_surface_.release());
    }
    if (surface_) {
      wl_surface_destroy(surface_.release());
    }
  }

  test::ResourceKey GetSurfaceResourceKey() const {
    return test::client_util::GetResourceKey(surface_.get());
  }

  bool close_called() const { return close_called_; }

 private:
  bool close_called_ = false;
  const raw_ptr<test::TestClient, ExperimentalAsh> client_;
  std::unique_ptr<wl_surface> surface_;
  std::unique_ptr<xdg_surface> xdg_surface_;
  std::unique_ptr<xdg_toplevel> xdg_toplevel_;
  std::unique_ptr<zaura_toplevel> aura_toplevel_;
  std::unique_ptr<zcr_remote_surface_v2> remote_surface_;
  std::unique_ptr<test::TestBuffer> buffer_;
};

enum TestCases {
  // Xdg Client (Laros/Crostini)
  XdgByClient,
  XdgWidgetClose,
  XdgWidgetCloseNow,
  XdgWindowDelete,
  // RemoteSehell (ARC++)
  RemoteByClient,
  RemoteWidgetClose,
  RemoteWidgetCloseNow,
  RemoteWindowDelete,
};

class ShellDestructionTest : public test::WaylandServerTest,
                             public testing::WithParamInterface<TestCases> {
 public:
  ShellDestructionTest() = default;
  ShellDestructionTest(const ShellDestructionTest&) = delete;
  ShellDestructionTest& operator=(const ShellDestructionTest&) = delete;
  ~ShellDestructionTest() override = default;

  bool IsXdgShell() {
    return GetParam() == XdgWidgetCloseNow || GetParam() == XdgWindowDelete;
  }

  bool IsWidgetCloseNow() {
    return GetParam() == XdgWidgetCloseNow ||
           GetParam() == RemoteWidgetCloseNow;
  }
  bool IsWidgetClose() {
    return GetParam() == XdgWidgetClose || GetParam() == RemoteWidgetClose;
  }
  bool IsByClient() {
    return GetParam() == XdgByClient || GetParam() == RemoteByClient;
  }
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(Xdg,
                         ShellDestructionTest,
                         testing::Values(XdgByClient,
                                         XdgWidgetClose,
                                         XdgWidgetCloseNow,
                                         XdgWindowDelete));
INSTANTIATE_TEST_SUITE_P(Remote,
                         ShellDestructionTest,
                         testing::Values(RemoteByClient,
                                         RemoteWidgetClose,
                                         RemoteWidgetCloseNow,
                                         RemoteWindowDelete));

// Make sure that xdg topevel/remote surfaces can be
// destroyed via Widget::CloseNow and window deletion.
// (b/276351837)
TEST_P(ShellDestructionTest, ShellDestruction) {
  test::ResourceKey surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));

    auto data = std::make_unique<ShellClientData>(client);
    auto* data_ptr = data.get();
    client->set_data(std::move(data));
    if (IsXdgShell()) {
      data_ptr->CreateXdgToplevel();
    } else {
      data_ptr->CreateRemoteSurface();
    }
    data_ptr->CreateAndAttachBuffer({256, 256});
    data_ptr->Commit();
    surface_key = data_ptr->GetSurfaceResourceKey();
  });

  Surface* surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), surface_key);
  auto* shell_surface =
      GetShellSurfaceBaseForWindow(surface->window()->GetToplevelWindow());
  auto widget_weak_ptr = shell_surface->GetWidget()->GetWeakPtr();
  ASSERT_TRUE(shell_surface);
  ASSERT_TRUE(shell_surface->GetWidget()->IsVisible());

  if (IsWidgetClose()) {
    shell_surface->GetWidget()->Close();
    base::RunLoop().RunUntilIdle();
  } else if (IsWidgetCloseNow()) {
    shell_surface->GetWidget()->CloseNow();
  } else if (IsByClient()) {
    PostToClientAndWait([&](test::TestClient* client) {
      auto* data_ptr = client->GetDataAs<ShellClientData>();
      data_ptr->Close();
    });
  } else {
    delete shell_surface->GetWidget()->GetNativeWindow();
  }

  PostToClientAndWait([&](test::TestClient* client) {
    EXPECT_TRUE(client->GetDataAs<ShellClientData>()->close_called());
  });

  // Widget should be deleted.
  EXPECT_FALSE(widget_weak_ptr);
  // The surface resource should also be destroyed.
  EXPECT_FALSE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                  surface_key));
}

using ShellWithClientTest = test::WaylandServerTest;

// Calling SetPined w/o commit should not crash (crbug.com/979128).
TEST_F(ShellWithClientTest, DestroyRootSurfaceBeforeCommit) {
  test::ResourceKey surface_key;
  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(256 * 256 * 4));

    auto data = std::make_unique<ShellClientData>(client);
    auto* data_ptr = data.get();
    client->set_data(std::move(data));
    data_ptr->CreateRemoteSurface();
    data_ptr->CreateAndAttachBuffer({256, 256});
    surface_key = data_ptr->GetSurfaceResourceKey();
  });
  EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                 surface_key));
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data_ptr = client->GetDataAs<ShellClientData>();
    data_ptr->DestroySurface();
    data_ptr->Pin();
  });
  EXPECT_FALSE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                  surface_key));
}

// Tests UnsetSnap() w/o attaching buffer doesn't crash (b/278147793).
TEST_F(ShellWithClientTest, UnsetSnapBeforeCommit) {
  test::ResourceKey surface_key;

  PostToClientAndWait([&](test::TestClient* client) {
    auto data = std::make_unique<ShellClientData>(client);
    auto* data_ptr = data.get();
    client->set_data(std::move(data));
    data_ptr->CreateXdgToplevel();
    surface_key = data_ptr->GetSurfaceResourceKey();
  });
  EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                 surface_key));
  // Verify the widget is not created yet.
  Surface* surface = test::server_util::GetUserDataForResource<Surface>(
      server_.get(), surface_key);
  ShellSurfaceBase* shell_surface_base =
      static_cast<ShellSurfaceBase*>(surface->GetDelegateForTesting());
  ASSERT_TRUE(shell_surface_base);
  EXPECT_FALSE(shell_surface_base->GetWidget());
  PostToClientAndWait([&](test::TestClient* client) {
    auto* data_ptr = client->GetDataAs<ShellClientData>();
    data_ptr->UnsetSnap();
  });
  EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(server_.get(),
                                                                 surface_key));
}

TEST_F(ShellWithClientTest, CreateWithDisplayId) {
  UpdateDisplay("800x600, 800x600");

  auto primary_id = GetPrimaryDisplay().id();
  auto secondary_id = GetSecondaryDisplay().id();

  // Initialize client.
  PostToClientAndWait([&](test::TestClient* client) {
    ASSERT_TRUE(client->InitShmBufferFactory(800 * 100 * 4));
    ASSERT_EQ(client->globals().outputs.size(), 2u);
  });

  auto create_new_window = [this](const gfx::Rect& bounds, int output_index) {
    test::ResourceKey surface_key;
    PostToClientAndWait([&](test::TestClient* client) {
      auto data = std::make_unique<ShellClientData>(client);
      auto* data_ptr = data.get();
      client->set_data(std::move(data));
      data_ptr->CreateXdgToplevel();
      wl_output* target_output =
          output_index == -1 ? nullptr
                             : client->globals().outputs[output_index].get();
      data_ptr->RequestWindowBounds(bounds, target_output);
      data_ptr->Commit();
      surface_key = data_ptr->GetSurfaceResourceKey();
    });

    EXPECT_TRUE(test::server_util::GetUserDataForResource<Surface>(
        server_.get(), surface_key));
    // Verify the widget is not created yet.
    Surface* surface = test::server_util::GetUserDataForResource<Surface>(
        server_.get(), surface_key);
    ShellSurfaceBase* shell_surface_base =
        static_cast<ShellSurfaceBase*>(surface->GetDelegateForTesting());
    return shell_surface_base;
  };

  auto* screen = display::Screen::GetScreen();
  constexpr gfx::Rect kPrimarilyOnPrimary{100, 0, 800, 100};
  {
    auto* shell_surface_base = create_new_window(kPrimarilyOnPrimary, 1);
    EXPECT_EQ(secondary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    EXPECT_EQ(kPrimarilyOnPrimary,
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    auto* shell_surface_base = create_new_window(kPrimarilyOnPrimary, 0);
    EXPECT_EQ(primary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    EXPECT_EQ(kPrimarilyOnPrimary,
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    auto* shell_surface_base = create_new_window(kPrimarilyOnPrimary, -1);
    EXPECT_EQ(primary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    // If display is not specified, new window will be placed fully inside the
    // display.
    // TODO(crbug.com/1291592): This logic is not consistent with
    // ash. This has to be updated once the bug is fixed.
    EXPECT_EQ(gfx::Rect{kPrimarilyOnPrimary.size()},
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }

  constexpr gfx::Rect kAlmostOnPrimary{101, 0, 700, 100};
  {
    auto* shell_surface_base = create_new_window(kAlmostOnPrimary, 1);
    // The window should stay on the secondary display (output_index=1).
    EXPECT_EQ(secondary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    EXPECT_EQ(kAlmostOnPrimary,
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    auto* shell_surface_base = create_new_window(kAlmostOnPrimary, 0);
    EXPECT_EQ(primary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    EXPECT_EQ(kAlmostOnPrimary,
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
  {
    auto* shell_surface_base = create_new_window(kAlmostOnPrimary, -1);
    EXPECT_EQ(primary_id,
              screen
                  ->GetDisplayNearestWindow(
                      shell_surface_base->GetWidget()->GetNativeWindow())
                  .id());
    // TODO(crbug.com/1291592): This logic is not consistent with
    // ash. This has to be updated once the bug is fixed.
    EXPECT_EQ(gfx::Rect({100, 0}, kAlmostOnPrimary.size()),
              shell_surface_base->GetWidget()->GetWindowBoundsInScreen());
  }
}

}  // namespace exo::wayland
