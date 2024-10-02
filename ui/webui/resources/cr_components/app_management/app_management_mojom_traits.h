// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_RESOURCES_CR_COMPONENTS_APP_MANAGEMENT_APP_MANAGEMENT_MOJOM_TRAITS_H_
#define UI_WEBUI_RESOURCES_CR_COMPONENTS_APP_MANAGEMENT_APP_MANAGEMENT_MOJOM_TRAITS_H_

#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/run_on_os_login_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"

namespace mojo {

namespace {

using AppType = app_management::mojom::AppType;
using PermissionDataView = app_management::mojom::PermissionDataView;
using PermissionType = app_management::mojom::PermissionType;
using TriState = app_management::mojom::TriState;
using PermissionValueDataView = app_management::mojom::PermissionValueDataView;
using InstallReason = app_management::mojom::InstallReason;
using InstallSource = app_management::mojom::InstallSource;
using WindowMode = app_management::mojom::WindowMode;
using RunOnOsLoginMode = app_management::mojom::RunOnOsLoginMode;
using RunOnOsLoginDataView = app_management::mojom::RunOnOsLoginDataView;

}  // namespace

template <>
struct EnumTraits<AppType, apps::AppType> {
  static AppType ToMojom(apps::AppType input);
  static bool FromMojom(AppType input, apps::AppType* output);
};

template <>
struct StructTraits<PermissionDataView, apps::PermissionPtr> {
  static apps::PermissionType permission_type(const apps::PermissionPtr& r) {
    return r->permission_type;
  }

  static const apps::PermissionValuePtr& value(const apps::PermissionPtr& r) {
    return r->value;
  }

  static bool is_managed(const apps::PermissionPtr& r) { return r->is_managed; }

  static bool Read(PermissionDataView, apps::PermissionPtr* out);
};

template <>
struct EnumTraits<PermissionType, apps::PermissionType> {
  static PermissionType ToMojom(apps::PermissionType input);
  static bool FromMojom(PermissionType input, apps::PermissionType* output);
};

template <>
struct EnumTraits<TriState, apps::TriState> {
  static TriState ToMojom(apps::TriState input);
  static bool FromMojom(TriState input, apps::TriState* output);
};

template <>
struct UnionTraits<PermissionValueDataView, apps::PermissionValuePtr> {
  static PermissionValueDataView::Tag GetTag(const apps::PermissionValuePtr& r);

  static bool IsNull(const apps::PermissionValuePtr& r) {
    return !absl::holds_alternative<bool>(r->value) &&
           !absl::holds_alternative<apps::TriState>(r->value);
  }

  static void SetToNull(apps::PermissionValuePtr* out) { out->reset(); }

  static bool bool_value(const apps::PermissionValuePtr& r) {
    if (absl::holds_alternative<bool>(r->value)) {
      return absl::get<bool>(r->value);
    }
    return false;
  }

  static apps::TriState tristate_value(const apps::PermissionValuePtr& r) {
    if (absl::holds_alternative<apps::TriState>(r->value)) {
      return absl::get<apps::TriState>(r->value);
    }
    return apps::TriState::kBlock;
  }

  static bool Read(PermissionValueDataView data, apps::PermissionValuePtr* out);
};

template <>
struct EnumTraits<InstallReason, apps::InstallReason> {
  static InstallReason ToMojom(apps::InstallReason input);
  static bool FromMojom(InstallReason input, apps::InstallReason* output);
};

template <>
struct EnumTraits<InstallSource, apps::InstallSource> {
  static InstallSource ToMojom(apps::InstallSource input);
  static bool FromMojom(InstallSource input, apps::InstallSource* output);
};

template <>
struct EnumTraits<WindowMode, apps::WindowMode> {
  static WindowMode ToMojom(apps::WindowMode input);
  static bool FromMojom(WindowMode input, apps::WindowMode* output);
};

template <>
struct EnumTraits<RunOnOsLoginMode, apps::RunOnOsLoginMode> {
  static RunOnOsLoginMode ToMojom(apps::RunOnOsLoginMode input);
  static bool FromMojom(RunOnOsLoginMode input, apps::RunOnOsLoginMode* output);
};

template <>
struct StructTraits<RunOnOsLoginDataView, apps::RunOnOsLoginPtr> {
  static apps::RunOnOsLoginMode login_mode(const apps::RunOnOsLoginPtr& r) {
    return r->login_mode;
  }

  static bool is_managed(const apps::RunOnOsLoginPtr& r) {
    return r->is_managed;
  }

  static bool Read(RunOnOsLoginDataView, apps::RunOnOsLoginPtr* out);
};

}  // namespace mojo

#endif  // UI_WEBUI_RESOURCES_CR_COMPONENTS_APP_MANAGEMENT_APP_MANAGEMENT_MOJOM_TRAITS_H_
