// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/supports_user_data.h"

#include "base/feature_list.h"
#include "base/features.h"
#include "base/sequence_checker.h"

namespace base {

std::unique_ptr<SupportsUserData::Data> SupportsUserData::Data::Clone() {
  return nullptr;
}

SupportsUserData::SupportsUserData()
    : user_data_(FeatureList::IsEnabled(features::kSupportsUserDataFlatHashMap)
                     ? MapVariants(FlatDataMap())
                     : MapVariants(DataMap())) {
  // Harmless to construct on a different execution sequence to subsequent
  // usage.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SupportsUserData::SupportsUserData(SupportsUserData&&) = default;
SupportsUserData& SupportsUserData::operator=(SupportsUserData&&) = default;

SupportsUserData::Data* SupportsUserData::GetUserData(const void* key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Avoid null keys; they are too vulnerable to collision.
  DCHECK(key);
  return absl::visit(
      [key](const auto& map) -> Data* {
        auto found = map.find(key);
        if (found != map.end()) {
          return found->second.get();
        }
        return nullptr;
      },
      user_data_);
}

std::unique_ptr<SupportsUserData::Data> SupportsUserData::TakeUserData(
    const void* key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Null keys are too vulnerable to collision.
  CHECK(key);
  return absl::visit(
      [key](auto& map) -> std::unique_ptr<SupportsUserData::Data> {
        auto found = map.find(key);
        if (found != map.end()) {
          std::unique_ptr<SupportsUserData::Data> deowned;
          deowned.swap(found->second);
          map.erase(key);
          return deowned;
        }
        return nullptr;
      },
      user_data_);
}

void SupportsUserData::SetUserData(const void* key,
                                   std::unique_ptr<Data> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Avoid null keys; they are too vulnerable to collision.
  DCHECK(key);
  if (data.get()) {
    absl::visit([key, &data](auto& map) { map[key] = std::move(data); },
                user_data_);
  } else {
    RemoveUserData(key);
  }
}

void SupportsUserData::RemoveUserData(const void* key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::visit(
      [key](auto& map) {
        auto it = map.find(key);
        if (it != map.end()) {
          // Remove the entry from the map before deleting `owned_data` to avoid
          // reentrancy issues when `owned_data` owns `this`. Otherwise:
          //
          // 1. `RemoveUserData()` calls `erase()`.
          // 2. `erase()` deletes `owned_data`.
          // 3. `owned_data` deletes `this`.
          //
          // At this point, `erase()` is still on the stack even though the
          // backing map (owned by `this`) has already been destroyed, and it
          // may simply crash, cause a use-after-free, or any other number of
          // interesting things.
          auto owned_data = std::move(it->second);
          map.erase(it);
        }
      },
      user_data_);
}

void SupportsUserData::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void SupportsUserData::CloneDataFrom(const SupportsUserData& other) {
  absl::visit(
      [this](const auto& other_map) {
        for (const auto& data_pair : other_map) {
          auto cloned_data = data_pair.second->Clone();
          if (cloned_data) {
            SetUserData(data_pair.first, std::move(cloned_data));
          }
        }
      },
      other.user_data_);
}

SupportsUserData::~SupportsUserData() {
  if (!absl::visit([](const auto& map) { return map.empty(); }, user_data_)) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  MapVariants local_user_data;
  user_data_.swap(local_user_data);
  // Now this->user_data_ is empty, and any destructors called transitively from
  // the destruction of |local_user_data| will see it that way instead of
  // examining a being-destroyed object.
}

void SupportsUserData::ClearAllUserData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  absl::visit([](auto& map) { map.clear(); }, user_data_);
}

}  // namespace base
