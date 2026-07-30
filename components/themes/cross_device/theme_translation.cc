// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/theme_translation.h"

namespace themes {

namespace {

sync_pb::ThemeSpecifics TranslateAndroidTheme(
    const sync_pb::ThemeAndroidSpecifics& android_specifics) {
  sync_pb::ThemeSpecifics desktop_specifics;
  if (android_specifics.has_use_custom_theme()) {
    desktop_specifics.set_use_custom_theme(
        android_specifics.use_custom_theme());
  }
  if (android_specifics.has_ntp_background()) {
    *desktop_specifics.mutable_ntp_background() =
        android_specifics.ntp_background();
  }
  if (android_specifics.has_user_color_theme()) {
    *desktop_specifics.mutable_user_color_theme() =
        android_specifics.user_color_theme();
  }
  return desktop_specifics;
}

sync_pb::ThemeSpecifics TranslateIosTheme(
    const sync_pb::ThemeIosSpecifics& ios_specifics) {
  sync_pb::ThemeSpecifics desktop_specifics;
  if (ios_specifics.has_user_color_theme()) {
    *desktop_specifics.mutable_user_color_theme() =
        ios_specifics.user_color_theme();
  }
  if (ios_specifics.has_ntp_background()) {
    *desktop_specifics.mutable_ntp_background() =
        ios_specifics.ntp_background();
  }
  return desktop_specifics;
}

}  // namespace

DeviceThemeInfo<sync_pb::ThemeSpecifics> TranslateAndroid(
    const sync_pb::ThemeAndroidSpecifics& android_specifics) {
  DeviceThemeInfo<sync_pb::ThemeSpecifics> info;
  info.os_type = syncer::DeviceInfo::OsType::kAndroid;
  info.theme = TranslateAndroidTheme(android_specifics);
  return info;
}

DeviceThemeInfo<sync_pb::ThemeSpecifics> TranslateIos(
    const sync_pb::ThemeIosSpecifics& ios_specifics) {
  DeviceThemeInfo<sync_pb::ThemeSpecifics> info;
  info.os_type = syncer::DeviceInfo::OsType::kIOS;
  info.theme = TranslateIosTheme(ios_specifics);
  return info;
}

}  // namespace themes
