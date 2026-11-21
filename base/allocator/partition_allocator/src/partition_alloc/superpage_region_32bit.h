// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PARTITION_ALLOC_SUPERPAGE_REGION_32BIT_H_
#define PARTITION_ALLOC_SUPERPAGE_REGION_32BIT_H_

// Super-page placement for 32-bit builds.
//
// On 64-bit, PartitionAlloc takes super pages from pools reserved up front.
// On 32-bit it reserves every 2 MiB super page separately, wherever a 2 MiB-
// aligned hole is left, so the long-lived pages that hold small objects end up
// scattered through the address space that large transient mappings (media
// buffers, ArrayBuffers, ashmem) need. With regions configured, normal-bucket
// super pages come from a few large PROT_NONE reservations instead, placed by
// the kernel (no random hint) and grown in steps; direct maps are unchanged.
// When the regions are full and can't grow, super pages fall back to the
// normal per-page reservation. Alternatively, the bottom-up mode places super
// pages one at a time at a cursor that grows upward from a low address.
//
// Both modes are off until configured (Cobalt configures them from
// base::allocator::PartitionAllocSupport::ReconfigureAfterFeatureListInit()),
// and only normal-bucket super pages of the regular pool are affected.
// While neither is configured, the hooks return after one relaxed atomic
// load, without taking a lock. The module's lock isn't reset across fork(),
// so configure it only in processes that don't fork afterwards (Cobalt on
// Android is single-process).

#include <cstddef>
#include <cstdint>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc {

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

// Enables super-page regions. Reserves `initial_bytes` right away (may be 0),
// then grows by `step_bytes` (0: no growth; a step that doesn't fit is halved,
// down to 16 MiB) while the total stays within `max_bytes`. Sizes are rounded
// down to 2 MiB. POSIX only; elsewhere this is a no-op. Call once, early;
// super pages reserved before the call stay where they are. Returns the bytes
// reserved by this call.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
size_t ConfigureSuperPageRegionsFor32Bit(size_t initial_bytes,
                                         size_t step_bytes,
                                         size_t max_bytes);

// Bottom-up mode: instead of regions, place each normal-bucket
// super page at a cursor that moves upward from `base` (2 MiB-aligned) towards
// `limit`, stepping over anything already mapped, so long-lived small-object
// pages form one bottom-up stack while everything else stays top-down. Uses
// MAP_FIXED_NOREPLACE (Linux >= 4.17); on older kernels the flag is ignored
// and the address is only a hint, so a mismatch is unmapped and skipped.
// Super pages fall back to today's path once the cursor reaches `limit`.
// Mutually exclusive with ConfigureSuperPageRegionsFor32Bit() (CHECKed).
// Returns false, leaving it off, if `base` is 0, the aligned range is empty,
// or the platform has no MAP_FIXED_NOREPLACE.
// Not for sandboxed processes: Chromium's seccomp mmap policy doesn't allow
// MAP_FIXED_NOREPLACE (Cobalt on Android runs single-process).
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool ConfigureBottomUpSuperPagesFor32Bit(uintptr_t base, uintptr_t limit);

// b1: turns PartitionAlloc's random placement hints off (or back on), so
// that super pages, direct maps and V8/Oilpan pages are placed by the kernel.
// Forwards to SetRandomPageBaseDisabledFor32Bit() in
// address_space_randomization.h, for callers outside partition_alloc.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void SetRandomHintsDisabledFor32Bit(bool disabled);
PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool AreRandomHintsDisabledFor32Bit();

// True if this build supports super-page regions (32-bit POSIX); elsewhere
// ConfigureSuperPageRegionsFor32Bit() does nothing.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool AreSuperPageRegionsAvailableFor32Bit();

struct SuperPageRegionStats {
  size_t regions = 0;
  size_t reserved_bytes = 0;
  size_t used_bytes = 0;
  size_t fallback_super_pages = 0;  // Taken outside the regions since enabled.
  // Bottom-up mode: super pages placed by the cursor, and its position.
  size_t bottom_up_super_pages = 0;
  uintptr_t bottom_up_cursor = 0;
  // 2 MiB slots the regular pool already used (super pages and direct maps)
  // when regions or the bottom-up cursor were configured. These stay where
  // they are.
  size_t preexisting_super_pages = 0;
};
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
SuperPageRegionStats GetSuperPageRegionStats();

namespace internal {

// Returns the start of `length` bytes (a multiple of the super page size) of
// PROT_NONE address space inside a region, or 0 if regions are disabled or
// full and can't grow. The caller marks the range used, as for any
// reservation.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) uintptr_t SuperPageRegionReserve(size_t length);

// If [address, address + length) lies in a region: decommits it back to
// zeroed PROT_NONE (keeping the address range reserved), frees it in the
// region and returns true. Otherwise returns false.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool SuperPageRegionRelease(uintptr_t address, size_t length);

// True if a root's chain hint lies in the bottom-up band, where a
// fallback reservation would extend the band past its limit; the caller then
// reserves without a hint.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) bool SuperPageRegionShouldDropHint(uintptr_t hint);

// Counts super pages reserved outside the regions while they are enabled.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void SuperPageRegionNoteFallback(size_t super_pages);

// Testing: unmaps all regions that hold no live super pages (regions with
// live super pages stay mapped) and disables them.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void ResetSuperPageRegionsForTesting();
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void SetSuperPageRegionReserveFailForTesting(bool fail);
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
size_t GetSuperPageRegionReserveAttemptsForTesting();

}  // namespace internal

#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_SUPERPAGE_REGION_32BIT_H_
