// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_layers.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/check_op.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "components/variations/entropy_provider.h"

namespace variations {

namespace {

void LogInvalidLayerReason(InvalidLayerReason reason) {
  base::UmaHistogramEnumeration("Variations.InvalidLayerReason", reason);
}

// Iterates through the members of the given layer proto definition, and
// returns the member which contains that slot (if any).
const Layer::LayerMember* FindActiveMemberBySlot(uint32_t chosen_slot,
                                                 const Layer& layer_proto) {
  for (const Layer::LayerMember& member : layer_proto.members()) {
    if (!member.id())
      continue;

    for (const Layer::LayerMember::SlotRange& slot : member.slots()) {
      if (slot.start() <= chosen_slot && chosen_slot <= slot.end())
        return &member;
    }
  }
  return nullptr;
}

// The result of SelectSlot.
struct SlotSelection {
  // The slot selected.
  ValueInRange slot;
  // The remainder after dividing pseudorandom range to slots.
  ValueInRange pseudorandom_remainder;
};

SlotSelection SelectSlot(ValueInRange pseudorandom, uint32_t num_slots) {
  DCHECK_GT(pseudorandom.range, 0u);
  DCHECK_GT(num_slots, 0u);
  DCHECK_EQ(pseudorandom.range % num_slots, 0u);
  // Since range and num_slots are both non-zero, and num_slots is a divisor of
  // range, slot_size is also guaranteed to be non-zero.
  uint32_t slot_size = pseudorandom.range / num_slots;
  return {
      .slot =
          {
              .value = pseudorandom.value / slot_size,
              .range = num_slots,
          },
      .pseudorandom_remainder =
          {
              .value = pseudorandom.value % slot_size,
              .range = slot_size,
          },
  };
}

ValueInRange CombineRanges(ValueInRange major, ValueInRange minor) {
  return {
      .value = major.value * minor.range + minor.value,
      .range = major.range * minor.range,
  };
}

ValueInRange SlotOfMember(const Layer::LayerMember& chosen_member,
                          uint32_t chosen_slot) {
  uint32_t slots_in_member = 0;
  uint32_t slots_in_member_less_than_chosen_slot = 0;
  for (const Layer::LayerMember::SlotRange& range : chosen_member.slots()) {
    const uint32_t range_size = range.end() - range.start() + 1;
    slots_in_member += range_size;
    if (chosen_slot > range.end()) {
      slots_in_member_less_than_chosen_slot += range_size;
    } else if (chosen_slot > range.start()) {
      slots_in_member_less_than_chosen_slot += chosen_slot - range.start();
    }
  }
  return {
      .value = slots_in_member_less_than_chosen_slot,
      .range = slots_in_member,
  };
}

// Computes a new entropy provider that can be used for uniform low-entropy
// randomization of studies in the layer member.
//
// The concept here is that the layer "divides" the pseudorandom range into
// different members, where "which member" is the "quotient", and now we are
// extracting the "remainder" of that division (as well as the range of the
// remainder, which will be the domain of the new provider).
//
// We define the remainder more specifically as the number of values in the
// pseudorandom function's range which give the same quotient (member) which are
// less than the given pseudorandom value. This makes the range of the
// remainder be the number of values in the range that map to the member.
//
// For example if |range| is [0,10) and we have a layer with 5 slots, and
// member M that contains slots 0 and 3, then there are 4 values in |range|
// that will activate that member [0,1,6,7], so the |remainder.range| will be 4.
// If |pseudorandom.value| is 7, then [0,1,6] are less than 7, so the
// |remainder.value| will be 3.
//
// The remainder is undefined for values not actually selected by the member,
// and this function should not be called with a chosen slot that is not in
// the member.
NormalizedMurmurHashEntropyProvider ComputeRemainderEntropy(
    const Layer::LayerMember& chosen_member,
    SlotSelection selection) {
  ValueInRange slot_of_member =
      SlotOfMember(chosen_member, selection.slot.value);
  ValueInRange remainder =
      CombineRanges(slot_of_member, selection.pseudorandom_remainder);
  return NormalizedMurmurHashEntropyProvider(remainder);
}

bool ValidSlotBounds(const Layer& layer_proto) {
  // Since num_slots divides LES range, we know it is small enough that
  // num_slots + 1 does not overflow.
  DCHECK_LE(layer_proto.num_slots(), layer_proto.num_slots() + 1);
  for (const auto& member : layer_proto.members()) {
    uint32_t next_slot_after_processed_ranges = 0;
    for (const auto& range : member.slots()) {
      // Ranges should be non-overlapping. We also require them to be in
      // increasing order so that we can easily validate that they are not
      // overlapping.
      if (range.start() < next_slot_after_processed_ranges) {
        return false;
      }
      // start and end are both unsigned (uint32_t) so no need to check that
      // they are non-negative.
      if (range.end() >= layer_proto.num_slots())
        return false;
      if (range.start() > range.end()) {
        return false;
      }
      next_slot_after_processed_ranges = range.end() + 1;
    }
  }
  return true;
}

}  // namespace

VariationsLayers::VariationsLayers(const VariationsSeed& seed,
                                   const EntropyProviders& entropy_providers)
    : nil_entropy({0, 1}) {
  // Don't activate any layer-constrained studies in benchmarking mode to
  // maintain deterministic behavior.
  if (entropy_providers.benchmarking_enabled())
    return;
  // TODO(crbug.com/1154033): Support a way to expire old/unused layers so they
  // no longer get processed by the clients.
  for (const Layer& layer_proto : seed.layers())
    ConstructLayer(entropy_providers, layer_proto);
}

VariationsLayers::VariationsLayers() : nil_entropy({0, 1}) {}

VariationsLayers::~VariationsLayers() = default;

void VariationsLayers::ConstructLayer(const EntropyProviders& entropy_providers,
                                      const Layer& layer_proto) {
  if (!layer_proto.unknown_fields().empty()) {
    LogInvalidLayerReason(InvalidLayerReason::kUnknownFields);
    return;
  }
  if (layer_proto.id() == 0) {
    LogInvalidLayerReason(InvalidLayerReason::kInvalidId);
    return;
  }
  if (layer_proto.num_slots() == 0) {
    LogInvalidLayerReason(InvalidLayerReason::kNoSlots);
    return;
  }
  if (layer_proto.members_size() == 0) {
    LogInvalidLayerReason(InvalidLayerReason::kNoMembers);
    return;
  }

  if (layer_proto.entropy_mode() != Layer::LOW &&
      layer_proto.entropy_mode() != Layer::DEFAULT) {
    LogInvalidLayerReason(InvalidLayerReason::kInvalidEntropyMode);
    return;
  }

  // Using the size of the domain as the output range maximizes the number of
  // possible pseudorandom outputs when using the low entropy source.
  size_t range = entropy_providers.low_entropy_domain();
  if (range % layer_proto.num_slots() != 0) {
    // We can't support uniform selection on layers with a slot count that
    // doesn't divide the low entropy range, so don't support them at all.
    LogInvalidLayerReason(
        InvalidLayerReason::kSlotsDoNotDivideLowEntropyDomain);
    return;
  }

  if (!ValidSlotBounds(layer_proto)) {
    LogInvalidLayerReason(InvalidLayerReason::kInvalidSlotBounds);
    return;
  }

  const auto& entropy_provider = (layer_proto.entropy_mode() != Layer::LOW)
                                     ? entropy_providers.default_entropy()
                                     : entropy_providers.low_entropy();
  uint32_t salt = layer_proto.salt() ? layer_proto.salt() : layer_proto.id();
  ValueInRange pseudorandom = {
      .value = entropy_provider.GetPseudorandomValue(salt, range),
      .range = static_cast<uint32_t>(range),
  };
  SlotSelection selection = SelectSlot(pseudorandom, layer_proto.num_slots());
  const auto* chosen_member =
      FindActiveMemberBySlot(selection.slot.value, layer_proto);
  if (!chosen_member) {
    // No member is active for the chosen slot.
    return;
  }

  // Store the active member info, along with the remainder entropy.
  active_member_for_layer_.emplace(
      layer_proto.id(), LayerInfo{
                            .active_member_id = chosen_member->id(),
                            .entropy_mode = layer_proto.entropy_mode(),
                            .remainder_entropy = ComputeRemainderEntropy(
                                *chosen_member, selection),
                        });
}

bool VariationsLayers::IsLayerMemberActive(uint32_t layer_id,
                                           uint32_t member_id) const {
  auto layer_iter = active_member_for_layer_.find(layer_id);
  if (layer_iter == active_member_for_layer_.end())
    return false;

  return layer_iter->second.active_member_id &&
         (member_id == layer_iter->second.active_member_id);
}

bool VariationsLayers::ActiveLayerMemberDependsOnHighEntropy(
    uint32_t layer_id) const {
  auto layer_iter = active_member_for_layer_.find(layer_id);
  if (layer_iter == active_member_for_layer_.end())
    return false;

  return layer_iter->second.entropy_mode == Layer::DEFAULT;
}

const base::FieldTrial::EntropyProvider& VariationsLayers::GetRemainderEntropy(
    uint32_t layer_id) const {
  auto layer_iter = active_member_for_layer_.find(layer_id);
  if (layer_iter == active_member_for_layer_.end()) {
    // TODO(holte): Remove CreateTrialsForStudy fuzzer, then uncomment this.
    // NOTREACHED();
    return nil_entropy;
  }
  return layer_iter->second.remainder_entropy;
}

}  // namespace variations
