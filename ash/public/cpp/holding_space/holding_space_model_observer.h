// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

class HoldingSpaceItem;

class ASH_PUBLIC_EXPORT HoldingSpaceModelObserver
    : public base::CheckedObserver {
 public:
  // Called when `items` get added to the holding space model.
  virtual void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) {}

  // Called when `items` get removed from the holding space model.
  virtual void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) {}

  // Called when a partially initialized holding space `item` gets fully
  // initialized.
  virtual void OnHoldingSpaceItemInitialized(const HoldingSpaceItem* item) {}

  // Indicates which field of a holding space item was updated during a batch
  // update operation, as notified through `OnHoldingSpaceItemUpdated()`. Note
  // that these values are used in a bitfield.
  enum UpdatedField : uint32_t {
    kAccessibleName = 1u,
    kBackingFile = kAccessibleName << 1u,
    kInProgressCommands = kBackingFile << 1u,
    kProgress = kInProgressCommands << 1u,
    kSecondaryText = kProgress << 1u,
    kSecondaryTextColor = kSecondaryText << 1u,
    kText = kSecondaryTextColor << 1u,
  };

  // Called when an `item` gets updated within the holding space model. The
  // specific fields which were updated are provided in `updated_fields`.
  virtual void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item,
                                         uint32_t updated_fields) {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_
