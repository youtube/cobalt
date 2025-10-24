// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_SHORTCUT_INPUT_KEY_STRINGS_H_
#define ASH_WEBUI_COMMON_SHORTCUT_INPUT_KEY_STRINGS_H_

namespace content {
class WebUIDataSource;
}

namespace ash {

namespace common {

void AddShortcutInputKeyStrings(content::WebUIDataSource* html_source);

}  // namespace common

}  // namespace ash

#endif  // ASH_WEBUI_COMMON_SHORTCUT_INPUT_KEY_STRINGS_H_
