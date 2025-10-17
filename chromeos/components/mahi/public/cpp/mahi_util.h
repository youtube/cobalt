// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_UTIL_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_UTIL_H_

#include "base/component_export.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "ui/base/class_property.h"

namespace chromeos::mahi {

using ActionType = crosapi::mojom::MahiContextMenuActionType;
using GetContentCallback =
    base::OnceCallback<void(crosapi::mojom::MahiPageContentPtr)>;

// Metrics:
COMPONENT_EXPORT(MAHI_PUBLIC_CPP)
extern const char kMahiContentExtractionTriggeringLatency[];
COMPONENT_EXPORT(MAHI_PUBLIC_CPP)
extern const char kMahiContextMenuActivated[];
COMPONENT_EXPORT(MAHI_PUBLIC_CPP)
extern const char kMahiContextMenuActivatedFailed[];

// Contains the types of button existed in Mahi Menu.
enum class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) ButtonType {
  kSummary = 0,
  kOutline = 1,
  kSettings = 2,
  kQA = 3,
  kElucidation = 4,
  kSummaryOfSelection = 5,
  kMaxValue = kSummaryOfSelection,
};

// Contains the eligibility status of text for a certain feature, e.g.
// elucidation.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) SelectedTextState {
  kUnknown = 0,
  kEligible = 1,
  kEmpty = 2,
  kTooShort = 3,
  kTooLong = 4,
  kMaxValue = kTooLong,
};

COMPONENT_EXPORT(MAHI_PUBLIC_CPP)
ActionType MatchButtonTypeToActionType(const ButtonType button_type);

// Used by ash window manager to place the mahi menu bubble in the correct
// container.
extern const COMPONENT_EXPORT(MAHI_PUBLIC_CPP) ui::ClassProperty<bool>* const
    kIsMahiMenuKey;

}  // namespace chromeos::mahi

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_MAHI_UTIL_H_
