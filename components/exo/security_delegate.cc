// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/security_delegate.h"

#include <memory>
#include <string>

#include "ash/public/cpp/app_types_util.h"
#include "components/exo/shell_surface_util.h"

namespace exo {

namespace {

class DefaultSecurityDelegate : public SecurityDelegate {
 public:
  ~DefaultSecurityDelegate() override = default;

  // SecurityDelegate:
  bool CanLockPointer(aura::Window* toplevel) const override {
    // TODO(b/200896773): Move this out from exo's default security delegate
    // define in client's security delegates.
    return ash::IsArcWindow(toplevel) || ash::IsLacrosWindow(toplevel);
  }

  SetBoundsPolicy CanSetBounds(aura::Window* toplevel) const override {
    // TODO(b/200896773): Move into LacrosSecurityDelegate when it exists.
    if (ash::IsLacrosWindow(toplevel)) {
      return SetBoundsPolicy::DCHECK_IF_DECORATED;
    } else if (ash::IsArcWindow(toplevel)) {
      // TODO(b/285252684): Move into ArcSecurityDelegate when it exists.
      return SetBoundsPolicy::ADJUST_IF_DECORATED;
    } else {
      return SetBoundsPolicy::IGNORE;
    }
  }
};

}  // namespace

SecurityDelegate::~SecurityDelegate() = default;

// static
std::unique_ptr<SecurityDelegate>
SecurityDelegate::GetDefaultSecurityDelegate() {
  return std::make_unique<DefaultSecurityDelegate>();
}

bool SecurityDelegate::CanSelfActivate(aura::Window* window) const {
  // TODO(b/233691818): The default should be "false", and clients should
  // override that if they need to self-activate.
  //
  // Unfortunately, several clients don't have their own SecurityDelegate yet,
  // so we will continue to use the old exo::Permissions stuff until they do.
  return HasPermissionToActivate(window);
}

bool SecurityDelegate::CanLockPointer(aura::Window* window) const {
  return false;
}

SecurityDelegate::SetBoundsPolicy SecurityDelegate::CanSetBounds(
    aura::Window* window) const {
  return SecurityDelegate::SetBoundsPolicy::IGNORE;
}

}  // namespace exo
