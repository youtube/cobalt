// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "extensions/common/extension_id.h"

namespace extensions {

void OpenExtensionSidePanel(Browser& browser, const ExtensionId& id) {
  SidePanelUI::GetSidePanelUIForBrowser(&browser)->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, id));
}

content::WebContents* GetExtensionSidePanelWebContents(Browser& browser,
                                                       const ExtensionId& id) {
  return ExtensionSidePanelManager::GetOrCreateForBrowser(&browser)
      ->GetExtensionCoordinatorForTesting(id)
      ->GetHostWebContentsForTesting();
}

}  // namespace extensions
