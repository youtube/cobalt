// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation
import SwiftUI
import WidgetKit

@main
struct ChromeWidgets: WidgetBundle {
  init() {
    CrashHelper.configure()
  }

  var body: some Widget {
    if #available(iOS 17.0, *) {
      return body17
    } else {
      return body16
    }
  }

  @available(iOS 17, *)
  @WidgetBundleBuilder
  var body17: some Widget {
    #if IOS_ENABLE_WIDGETS_FOR_MIM
      QuickActionsWidgetConfigurable()
      SearchWidgetConfigurable()
      ShortcutsWidgetConfigurable()
      SearchPasswordsWidgetConfigurable()
      DinoGameWidgetConfigurable()
    #else
      QuickActionsWidget()
      SearchWidget()
      ShortcutsWidget()
      SearchPasswordsWidget()
      DinoGameWidget()
    #endif
    #if IOS_ENABLE_LOCKSCREEN_WIDGET
      #if IOS_AVAILABLE_LOCKSCREEN_WIDGET
        LockscreenLauncherSearchWidget()
        LockscreenLauncherIncognitoWidget()
        LockscreenLauncherVoiceSearchWidget()
        LockscreenLauncherGameWidget()
      #endif
    #endif
  }

  @WidgetBundleBuilder
  var body16: some Widget {
    QuickActionsWidget()
    SearchWidget()
    ShortcutsWidget()
    SearchPasswordsWidget()
    DinoGameWidget()
    #if IOS_ENABLE_LOCKSCREEN_WIDGET
      #if IOS_AVAILABLE_LOCKSCREEN_WIDGET
        LockscreenLauncherSearchWidget()
        LockscreenLauncherIncognitoWidget()
        LockscreenLauncherVoiceSearchWidget()
        LockscreenLauncherGameWidget()
      #endif
    #endif
  }
}
