// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_HTML_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_HTML_TEST_UTILS_H_

#include <string>

#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}

namespace ash {

// Helper methods for browser tests that need to get HTML element bounds
// and execute Javascript.

void ExecuteScriptAndExtractInt(content::WebContents* web_contents,
                                const std::string& script,
                                int* result);

void ExecuteScript(content::WebContents* web_contents,
                   const std::string& script);

gfx::Rect GetControlBoundsInRoot(content::WebContents* web_contents,
                                 const std::string& field_id);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_HTML_TEST_UTILS_H_
