// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PREVIEW_METADATA_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PREVIEW_METADATA_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/files/file.h"

namespace ash {

ASH_EXPORT std::u16string QuickInsertGetFilePreviewDescription(
    std::optional<base::File::Info> info);
}

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PREVIEW_METADATA_H_
