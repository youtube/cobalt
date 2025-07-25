// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/file_manager/resource_loader.h"

#include "base/containers/contains.h"
#include "base/strings/string_util.h"

namespace ash {
namespace file_manager {

void AddFilesAppResources(content::WebUIDataSource* source,
                          const webui::ResourcePath* entries,
                          size_t size) {
  for (size_t i = 0; i < size; ++i) {
    std::string path(entries[i].path);
    // Only load resources for Files app.
    if (base::StartsWith(path, "file_manager/") &&
        !base::Contains(path, "untrusted_resources/")) {
      // Files app UI has all paths relative to //ui/file_manager/file_manager/
      // so we remove the leading file_manager/ to match the existing paths.
      base::ReplaceFirstSubstringAfterOffset(&path, 0, "file_manager/", "");
      source->AddResourcePath(path, entries[i].id);
    }
  }
}

}  // namespace file_manager
}  // namespace ash
