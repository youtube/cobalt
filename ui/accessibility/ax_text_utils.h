// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TEXT_UTILS_H_
#define UI_ACCESSIBILITY_AX_TEXT_UTILS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

// Convenience method needed to implement platform-specific text
// accessibility APIs like IAccessible2. Search forwards or backwards
// (depending on |direction|) from the given |start_offset| until the
// given boundary is found, and return the offset of that boundary,
// using the vector of line break character offsets in |line_breaks|.
AX_EXPORT size_t FindAccessibleTextBoundary(const std::u16string& text,
                                            const std::vector<int>& line_breaks,
                                            ax::mojom::TextBoundary boundary,
                                            size_t start_offset,
                                            ax::mojom::MoveDirection direction,
                                            ax::mojom::TextAffinity affinity);

// Returns a string ID that corresponds to the name of the given action.
AX_EXPORT std::u16string ActionVerbToLocalizedString(
    const ax::mojom::DefaultActionVerb action_verb);

// Returns the non-localized string representation of a supported action.
// Some APIs on Linux and Windows need to return non-localized action names.
AX_EXPORT std::u16string ActionVerbToUnlocalizedString(
    const ax::mojom::DefaultActionVerb action_verb);

// Returns indices of all word starts in |text|.
AX_EXPORT std::vector<int> GetWordStartOffsets(const std::u16string& text);
// Returns indices of all word ends in |text|.
AX_EXPORT std::vector<int> GetWordEndOffsets(const std::u16string& text);

// Returns indices of all sentence starts in |text|.
AX_EXPORT std::vector<int> GetSentenceStartOffsets(const std::u16string& text);
// Returns indices of all sentence ends in |text|.
AX_EXPORT std::vector<int> GetSentenceEndOffsets(const std::u16string& text);
}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TEXT_UTILS_H_
