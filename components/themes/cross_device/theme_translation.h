// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_CROSS_DEVICE_THEME_TRANSLATION_H_
#define COMPONENTS_THEMES_CROSS_DEVICE_THEME_TRANSLATION_H_

#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/themes/cross_device/cross_device_theme_tracker.h"

namespace themes {

DeviceThemeInfo<sync_pb::ThemeSpecifics> TranslateAndroid(
    const sync_pb::ThemeAndroidSpecifics& android_specifics);

DeviceThemeInfo<sync_pb::ThemeSpecifics> TranslateIos(
    const sync_pb::ThemeIosSpecifics& ios_specifics);

}  // namespace themes

#endif  // COMPONENTS_THEMES_CROSS_DEVICE_THEME_TRANSLATION_H_
