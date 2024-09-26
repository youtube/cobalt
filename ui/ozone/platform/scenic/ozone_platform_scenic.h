// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_OZONE_PLATFORM_SCENIC_H_
#define UI_OZONE_PLATFORM_SCENIC_OZONE_PLATFORM_SCENIC_H_

namespace ui {

class OzonePlatform;

// Constructor hook for use in ozone_platform_list.cc
OzonePlatform* CreateOzonePlatformScenic();

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_OZONE_PLATFORM_SCENIC_H_
