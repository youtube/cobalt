// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard User module
//
// Defines a user management API. This module defines functions only for
// managing signed-in users. Platforms that do not have users must still
// implement this API, always reporting a single user that is current and
// signed in.
//
// These APIs are NOT expected to be thread-safe, so either call them from a
// single thread, or perform proper synchronization around all calls.

#ifndef STARBOARD_USER_H_
#define STARBOARD_USER_H_

#include "starboard/export.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing a device user.
typedef struct SbUserPrivate SbUserPrivate;

// A handle to a user.
typedef SbUserPrivate* SbUser;

// A set of string properties that can be queried on a user.
typedef enum SbUserPropertyId {
  // The URL to the avatar for a user. Avatars are not provided on all
  // platforms.
  kSbUserPropertyAvatarUrl,

  // The path to a user's home directory, if supported on this platform.
  kSbUserPropertyHomeDirectory,

  // The username of a user, which may be the same as the User ID, or it may be
  // friendlier.
  kSbUserPropertyUserName,

  // A unique user ID of a user.
  kSbUserPropertyUserId,
} SbUserPropertyId;

// Well-defined value for an invalid user.
#define kSbUserInvalid (SbUser) NULL

// Returns whether the given user handle is valid.
static SB_C_INLINE bool SbUserIsValid(SbUser user) {
  return user != kSbUserInvalid;
}

// Gets a list of up to |users_size| signed-in users and places the results in
// |out_users|. The return value identifies the actual number of signed-in
// users, which may be greater or less than |users_size|.
//
// It is expected that there will be a unique |SbUser| per signed-in user and
// that the referenced objects will persist for the lifetime of the app.
//
// |out_users|: Handles for the retrieved users.
// |users_size|: The maximum number of signed-in users to retrieve.
SB_EXPORT int SbUserGetSignedIn(SbUser* out_users, int users_size);

// Gets the current primary user, if one exists. This is the user that is
// determined, in a platform-specific way, to be the primary user controlling
// the application. For example, the determination might be made because that
// user launched the app, though it should be made using whatever criteria are
// appropriate for the platform.
//
// It is expected that there will be a unique SbUser per signed-in user, and
// that the referenced objects will persist for the lifetime of the app.
SB_EXPORT SbUser SbUserGetCurrent();

// Returns the size of the value of |property_id| for |user|, INCLUDING the
// terminating null character. The function returns |0| if |user| is invalid
// or if |property_id| is not recognized, supported, or set for the user.
//
// |user|: The user for which property size data is being retrieved.
// |property_id|: The property for which the data is requested.
SB_EXPORT int SbUserGetPropertySize(SbUser user, SbUserPropertyId property_id);

// Retrieves the value of |property_id| for |user| and places it in |out_value|.
// The function returns:
// - |true| if the property value is retrieved successfully
// - |false| if |user| is invalid; if |property_id| isn't recognized, supported,
//   or set for |user|; or if |value_size| is too small.
//
// |user|: The user for which property size data is being retrieved.
// |property_id|: The property for which the data is requested.
// |out_value|: The retrieved property value.
// |value_size|: The size of the retrieved property value.
SB_EXPORT bool SbUserGetProperty(SbUser user,
                                 SbUserPropertyId property_id,
                                 char* out_value,
                                 int value_size);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_USER_H_
