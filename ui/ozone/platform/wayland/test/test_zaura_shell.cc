// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_shell.h"

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_popup.h"
#include "ui/ozone/platform/wayland/test/test_zaura_surface.h"
#include "ui/ozone/platform/wayland/test/test_zaura_toplevel.h"

namespace wl {

namespace {

constexpr uint32_t kZAuraShellVersion = 44;
constexpr uint32_t kZAuraOutputVersion = 44;

void GetAuraSurface(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* surface_resource) {
  CreateResourceWithImpl<TestZAuraSurface>(client, &zaura_surface_interface,
                                           kZAuraShellVersion,
                                           &kTestZAuraSurfaceImpl, id);
}

void GetAuraOutput(wl_client* client,
                   wl_resource* resource,
                   uint32_t id,
                   wl_resource* output_resource) {
  wl_resource* zaura_output_resource = CreateResourceWithImpl<TestZAuraOutput>(
      client, &zaura_output_interface, kZAuraOutputVersion,
      &kTestZAuraOutputImpl, id);
  auto* output = GetUserDataAs<TestOutput>(output_resource);
  output->SetAuraOutput(GetUserDataAs<TestZAuraOutput>(zaura_output_resource));
}

void SurfaceSubmissionInPixelCoordinates(wl_client* client,
                                         wl_resource* resource) {
  // TODO(crbug.com/1346347): Implement zaura-shell protocol requests and test
  // their usage.
  NOTIMPLEMENTED_LOG_ONCE();
}

void GetAuraToplevelForXdgToplevel(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t id,
                                   wl_resource* toplevel) {
  CreateResourceWithImpl<TestZAuraToplevel>(client, &zaura_toplevel_interface,
                                            kZAuraShellVersion,
                                            &kTestZAuraToplevelImpl, id);
}

void GetAuraPopupForXdgPopup(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* popup) {
  CreateResourceWithImpl<TestZAuraPopup>(client, &zaura_popup_interface,
                                         kZAuraShellVersion,
                                         &kTestZAuraPopupImpl, id);
}

const struct zaura_shell_interface kTestZAuraShellImpl = {
    &GetAuraSurface,
    &GetAuraOutput,
    &SurfaceSubmissionInPixelCoordinates,
    &GetAuraToplevelForXdgToplevel,
    &GetAuraPopupForXdgPopup,
    &DestroyResource,
};

}  // namespace

TestZAuraShell::TestZAuraShell()
    : GlobalObject(&zaura_shell_interface,
                   &kTestZAuraShellImpl,
                   kZAuraShellVersion) {}

TestZAuraShell::~TestZAuraShell() = default;

void TestZAuraShell::SetBugFixes(std::vector<uint32_t> bug_fixes) {
  bug_fixes_ = std::move(bug_fixes);
  MaybeSendBugFixes();
}

void TestZAuraShell::OnBind() {
  MaybeSendBugFixes();
}

void TestZAuraShell::MaybeSendBugFixes() {
  if (resource() && wl_resource_get_version(resource()) >=
                        ZAURA_SHELL_BUG_FIX_SINCE_VERSION) {
    for (const uint32_t bug_fix : bug_fixes_)
      zaura_shell_send_bug_fix(resource(), bug_fix);
  }
}

}  // namespace wl
