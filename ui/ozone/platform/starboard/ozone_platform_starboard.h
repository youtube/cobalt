// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_OZONE_PLATFORM_STARBOARD_H_
#define UI_OZONE_PLATFORM_OZONE_PLATFORM_STARBOARD_H_

namespace ui {

class OzonePlatform;

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformStarboard();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_OZONE_PLATFORM_STARBOARD_H_
