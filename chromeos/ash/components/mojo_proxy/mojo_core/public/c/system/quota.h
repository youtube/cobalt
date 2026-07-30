// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_QUOTA_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_QUOTA_H_

#include <stdint.h>

#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/macros.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/system_export.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/types.h"

namespace mojo_legacy {
// Flags passed to |MojoSetQuota| via |MojoSetQuotaOptions|.
typedef uint32_t MojoSetQuotaFlags;

// No flags.
#define MOJO_LEGACY_SET_QUOTA_FLAG_NONE ((MojoSetQuotaFlags)0)

// Options passed to |MojoSetQuota()|.
struct MOJO_LEGACY_ALIGNAS(8) MojoSetQuotaOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoSetQuotaFlags| above.
  MojoSetQuotaFlags flags;
};
MOJO_LEGACY_STATIC_ASSERT(sizeof(struct MojoSetQuotaOptions) == 8,
                          "MojoSetQuotaOptions has wrong size.");

// Flags passed to |MojoQueryQuota| via |MojoQueryQuotaOptions|.
typedef uint32_t MojoQueryQuotaFlags;

// No flags.
#define MOJO_LEGACY_QUERY_QUOTA_FLAG_NONE ((MojoQueryQuotaFlags)0)

// Options passed to |MojoQueryQuota()|.
struct MOJO_LEGACY_ALIGNAS(8) MojoQueryQuotaOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoQueryQuotaFlags| above.
  MojoQueryQuotaFlags flags;
};
MOJO_LEGACY_STATIC_ASSERT(sizeof(struct MojoQueryQuotaOptions) == 8,
                          "MojoQueryQuotaOptions has wrong size.");

// The maximum value any quota can be set to. Effectively means "no quota".
#define MOJO_LEGACY_QUOTA_LIMIT_NONE ((uint64_t)0xffffffffffffffff)

// An enumeration of different types of quotas that can be set on a handle.
typedef uint32_t MojoQuotaType;

// Limits the number of unread messages which can be queued on a message pipe
// endpoint before raising a |MOJO_LEGACY_HANDLE_SIGNAL_QUOTA_EXCEEDED| signal
// on that endpoint. May only be set on message pipe handles.
#define MOJO_LEGACY_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH ((MojoQuotaType)0)

// Limits the total size (in bytes) of unread messages which can be queued on a
// message pipe endpoint before raising a
// |MOJO_LEGACY_HANDLE_SIGNAL_QUOTA_EXCEEDED| signal on that endpoint. May only
// be set on message pipe handles.
#define MOJO_LEGACY_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE ((MojoQuotaType)1)

// Limits the number of sent, unread messages which can be queued on a message
// pipe endpoint before raising a |MOJO_LEGACY_HANDLE_SIGNAL_QUOTA_EXCEEDED|
// signal on that  endpoint. May only be set on message pipe handles.
#define MOJO_LEGACY_QUOTA_TYPE_UNREAD_MESSAGE_COUNT ((MojoQuotaType)2)

// Sets a quota on a given handle which will cause that handle to raise the
// |MOJO_LEGACY_HANDLE_SIGNAL_QUOTA_EXCEEDED| signal if the quota is exceeded.
// Signals can be trapped using |MojoCreateTrap()| and related APIs (see
// trap.h).
//
// All quota limits on a handle default to |MOJO_LEGACY_QUOTA_LIMIT_NONE|,
// meaning that the resource is unlimited.
//
// NOTE: A handle's quota is only enforced as long as the handle remains within
// the process which set the quota.
//
// Parameters:
//   |handle|: The handle on which a quota should be set.
//   |type|: The type of quota to set. Certain types of quotas may only be set
//       on certain types of handles. See notes on individual quota type
//       definitions above for meaning and restrictions.
//   |limit|: The limiting value of the quota. The meaning of this is determined
//       by |type|. See notes on individual quota type definitions above.
//   |options|: Additional options; may be null.
//
//  Returns:
//    |MOJO_LEGACY_RESULT_OK| if the quota was successfully set.
//    |MOJO_LEGACY_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle
//    value,
//        |type| is not a known quota type, |options| is non-null but
//        |*options| is malformed, or the quota |type| cannot be set on |handle|
//        because the quota does not apply to that type of handle.
MOJO_LEGACY_SYSTEM_EXPORT MojoResult
MojoSetQuota(MojoHandle handle,
             MojoQuotaType type,
             uint64_t limit,
             const struct MojoSetQuotaOptions* options);

// Queries a handle for information about a specific quota.
//
// Parameters:
//   |handle|: The handle to query.
//   |type|: The type of quota to query.
//   |limit|: Receives the quota's currently set limit if non-null.
//   |usage|: Receives the quota's current usage if non-null.
//
// Returns:
//   |MOJO_LEGACY_RESULT_OK| if the quota was successfully queried on |handle|.
//   Upon
//       return, |*limit| contains the quota's current limit if |limit| is
//       non-null, and |*usage| contains the quota's current usage if |usage| is
//       non-null.
//   |MOJO_LEGACY_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle
//   value or
//       quota |type| does not apply to the type of object referenced by
//       |handle|.
MOJO_LEGACY_SYSTEM_EXPORT MojoResult
MojoQueryQuota(MojoHandle handle,
               MojoQuotaType type,
               const struct MojoQueryQuotaOptions* options,
               uint64_t* limit,
               uint64_t* usage);

}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_C_SYSTEM_QUOTA_H_
