// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/display_config_helper.h"

#include <vector>

namespace display::win {

namespace {

std::vector<DISPLAYCONFIG_PATH_INFO> GetDisplayConfigPathInfos() {
  for (LONG result = ERROR_INSUFFICIENT_BUFFER;
       result == ERROR_INSUFFICIENT_BUFFER;) {
    uint32_t path_elements, mode_elements;
    if (::GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_elements,
                                      &mode_elements) != ERROR_SUCCESS) {
      return {};
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> path_infos(path_elements);
    std::vector<DISPLAYCONFIG_MODE_INFO> mode_infos(mode_elements);
    result = ::QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_elements,
                                  path_infos.data(), &mode_elements,
                                  mode_infos.data(), nullptr);
    if (result == ERROR_SUCCESS) {
      path_infos.resize(path_elements);
      return path_infos;
    }
  }
  return {};
}

}  // namespace

absl::optional<DISPLAYCONFIG_PATH_INFO> GetDisplayConfigPathInfo(
    HMONITOR monitor) {
  // Get the monitor name.
  MONITORINFOEX monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  if (!::GetMonitorInfo(monitor, &monitor_info)) {
    return absl::nullopt;
  }

  // Look for a path info with a matching name.
  for (const auto& info : GetDisplayConfigPathInfos()) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME device_name = {};
    device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    device_name.header.size = sizeof(device_name);
    device_name.header.adapterId = info.sourceInfo.adapterId;
    device_name.header.id = info.sourceInfo.id;
    if ((::DisplayConfigGetDeviceInfo(&device_name.header) == ERROR_SUCCESS) &&
        (wcscmp(monitor_info.szDevice, device_name.viewGdiDeviceName) == 0)) {
      return info;
    }
  }
  return absl::nullopt;
}

}  // namespace display::win
