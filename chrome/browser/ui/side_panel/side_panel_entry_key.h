// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_KEY_H_
#define CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_KEY_H_

#include <string>

#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Container for entry identification related information.
class SidePanelEntryKey {
 public:
  explicit SidePanelEntryKey(SidePanelEntryId id);
  SidePanelEntryKey(SidePanelEntryId id, extensions::ExtensionId extension_id);
  SidePanelEntryKey(const SidePanelEntryKey& other);
  ~SidePanelEntryKey();

  SidePanelEntryKey& operator=(const SidePanelEntryKey& other);
  bool operator==(const SidePanelEntryKey& other) const;
  bool operator<(const SidePanelEntryKey& other) const;

  SidePanelEntryId id() const { return id_; }
  absl::optional<extensions::ExtensionId> extension_id() const {
    return extension_id_;
  }
  std::string ToString() const;

 private:
  SidePanelEntryId id_;
  absl::optional<extensions::ExtensionId> extension_id_ = absl::nullopt;
};

#endif  // CHROME_BROWSER_UI_SIDE_PANEL_SIDE_PANEL_ENTRY_KEY_H_
