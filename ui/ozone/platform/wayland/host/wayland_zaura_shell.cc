// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"

#include <cstring>

#include <components/exo/wayland/protocol/aura-shell-client-protocol.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

namespace {

constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 60;

}  // namespace

// static
constexpr char WaylandZAuraShell::kInterfaceName[];

// static
void WaylandZAuraShell::Instantiate(WaylandConnection* connection,
                                    wl_registry* registry,
                                    uint32_t name,
                                    const std::string& interface,
                                    uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";

  if (connection->zaura_shell_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }

  auto zaura_shell = wl::Bind<struct zaura_shell>(
      registry, name, std::min(version, kMaxVersion));
  if (!zaura_shell) {
    LOG(ERROR) << "Failed to bind zaura_shell";
    return;
  }
  connection->zaura_shell_ =
      std::make_unique<WaylandZAuraShell>(zaura_shell.release(), connection);
  ReportShellUMA(UMALinuxWaylandShell::kZauraShell);

  // Usually WaylandOutputManager is instantiated first, so any ZAuraOutputs it
  // created wouldn't have been initialized, since the zaura_shell didn't exist
  // yet. So initialize them now.
  if (connection->wayland_output_manager()) {
    connection->wayland_output_manager()->InitializeAllZAuraOutputs();
  }
}

WaylandZAuraShell::WaylandZAuraShell(zaura_shell* aura_shell,
                                     WaylandConnection* connection)
    : obj_(aura_shell), connection_(connection) {
  DCHECK(obj_);
  DCHECK(connection_);

  static constexpr zaura_shell_listener kZAuraShellListener = {
      .layout_mode = &OnLayoutMode,
      .bug_fix = &OnBugFix,
      .desks_changed = &OnDesksChanged,
      .desk_activation_changed = &OnDeskActivationChanged,
      .activated = &OnActivated,
      .set_overview_mode = &OnSetOverviewMode,
      .unset_overview_mode = &OnUnsetOverviewMode,
      .compositor_version = &OnCompositorVersion,
      .all_bug_fixes_sent = &OnAllBugFixesSent};
  zaura_shell_add_listener(obj_.get(), &kZAuraShellListener, this);

  if (IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled() &&
      zaura_shell_get_version(wl_object()) >=
          ZAURA_TOPLEVEL_SURFACE_SUBMISSION_IN_PIXEL_COORDINATES_SINCE_VERSION) {
    connection->set_surface_submission_in_pixel_coordinates(true);
  }
}

WaylandZAuraShell::~WaylandZAuraShell() = default;

bool WaylandZAuraShell::HasBugFix(uint32_t id) {
  return std::find(bug_fix_ids_.begin(), bug_fix_ids_.end(), id) !=
         bug_fix_ids_.end();
}

std::string WaylandZAuraShell::GetDeskName(int index) const {
  if (static_cast<size_t>(index) >= desks_.size())
    return std::string();
  return desks_[index];
}

int WaylandZAuraShell::GetNumberOfDesks() {
  return desks_.size();
}

int WaylandZAuraShell::GetActiveDeskIndex() const {
  return active_desk_index_;
}

bool WaylandZAuraShell::SupportsAllBugFixesSent() const {
  return zaura_shell_get_version(wl_object()) >=
         ZAURA_SHELL_ALL_BUG_FIXES_SENT_SINCE_VERSION;
}

void WaylandZAuraShell::ResetBugFixesStatusForTesting() {
  all_bug_fixes_sent_ = false;
  bug_fix_ids_.clear();
}

absl::optional<std::vector<uint32_t>> WaylandZAuraShell::MaybeGetBugFixIds()
    const {
  if (!all_bug_fixes_sent_) {
    return absl::nullopt;
  }

  return absl::make_optional(bug_fix_ids_);
}

// static
void WaylandZAuraShell::OnLayoutMode(void* data,
                                     struct zaura_shell* zaura_shell,
                                     uint32_t layout_mode) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* self = static_cast<WaylandZAuraShell*>(data);
  auto* connection = self->connection_.get();
  auto* screen = connection->wayland_output_manager()->wayland_screen();

  switch (layout_mode) {
    case ZAURA_SHELL_LAYOUT_MODE_WINDOWED:
      connection->set_tablet_layout_state(
          display::TabletState::kInClamshellMode);
      // `screen` is null in some unit test suites or if it's called earlier
      // than screen initialization.
      if (screen)
        screen->OnTabletStateChanged(display::TabletState::kInClamshellMode);
      return;
    case ZAURA_SHELL_LAYOUT_MODE_TABLET:
      connection->set_tablet_layout_state(display::TabletState::kInTabletMode);
      // `screen` is null in some unit test suites or if it's called earlier
      // than screen initialization.
      if (screen)
        screen->OnTabletStateChanged(display::TabletState::kInTabletMode);
      return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

// static
void WaylandZAuraShell::OnBugFix(void* data,
                                 struct zaura_shell* zaura_shell,
                                 uint32_t id) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  CHECK(!self->all_bug_fixes_sent_);
  self->bug_fix_ids_.push_back(id);
}

// static
void WaylandZAuraShell::OnDesksChanged(void* data,
                                       struct zaura_shell* zaura_shell,
                                       struct wl_array* states) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  char* desk_name = reinterpret_cast<char*>(states->data);
  self->desks_.clear();
  while (desk_name < reinterpret_cast<char*>(states->data) + states->size) {
    std::string str(desk_name, strlen(desk_name));
    self->desks_.push_back(str);
    desk_name += strlen(desk_name) + 1;
  }
}

// static
void WaylandZAuraShell::OnDeskActivationChanged(void* data,
                                                struct zaura_shell* zaura_shell,
                                                int active_desk_index) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  self->active_desk_index_ = active_desk_index;
}

// static
void WaylandZAuraShell::OnActivated(void* data,
                                    struct zaura_shell* zaura_shell,
                                    wl_surface* gained_active,
                                    wl_surface* lost_active) {}

// static
void WaylandZAuraShell::OnSetOverviewMode(void* data,
                                          struct zaura_shell* zaura_shell) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* self = static_cast<WaylandZAuraShell*>(data);
  for (auto* window : self->connection_->window_manager()->GetAllWindows()) {
    if (auto* toplevel_window = window->AsWaylandToplevelWindow()) {
      toplevel_window->OnOverviewModeChanged(true);
    }
  }
#endif
}

// static
void WaylandZAuraShell::OnUnsetOverviewMode(void* data,
                                            struct zaura_shell* zaura_shell) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* self = static_cast<WaylandZAuraShell*>(data);
  for (auto* window : self->connection_->window_manager()->GetAllWindows()) {
    if (auto* toplevel_window = window->AsWaylandToplevelWindow()) {
      toplevel_window->OnOverviewModeChanged(false);
    }
  }
#endif
}

// static
void WaylandZAuraShell::OnCompositorVersion(void* data,
                                            struct zaura_shell* zaura_shell,
                                            const char* version_label) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  base::Version compositor_version(version_label);
  if (!compositor_version.IsValid()) {
    LOG(WARNING) << "Invalid compositor version string received.";
    self->compositor_version_ = {};
    return;
  }

  DCHECK_EQ(compositor_version.components().size(), 4u);
  DVLOG(1) << "Wayland compositor version: " << compositor_version;
  self->compositor_version_ = compositor_version;
}

// static
void WaylandZAuraShell::OnAllBugFixesSent(void* data,
                                          struct zaura_shell* zaura_shell) {
  auto* self = static_cast<WaylandZAuraShell*>(data);
  CHECK(!self->all_bug_fixes_sent_);
  self->all_bug_fixes_sent_ = true;

  if (!self->connection_->buffer_manager_host()) {
    // This message may be called before WaylandConnection initialization. Bug
    // fix ids will be sent on requested in such case.
    return;
  }

  self->connection_->buffer_manager_host()->OnAllBugFixesSent(
      self->bug_fix_ids_);
}

}  // namespace ui
