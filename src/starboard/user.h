// Copyright 2016 Google Inc. All Rights Reserved.
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

// User management API. Platforms that do not have users must still implement
// this API, always reporting a single user current and signed in.

// These APIs are NOT expected to be thread-safe, so either call them from a
// single thread, or perform proper synchronization around all calls.

#ifndef STARBOARD_USER_H_
#define STARBOARD_USER_H_

#include "starboard/export.h"
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

// Gets a list of up to |users_size| signed-in users, placing the results in
// |out_users|, and returning the number of actual users signed in, whether
// greater or less than |users_size|. It is expected that there will be a unique
// SbUser per signed in user, and that the referenced objects will persist for
// the lifetime of the app.
SB_EXPORT int SbUserGetSignedIn(SbUser* out_users, int users_size);

// Gets the current primary user, if any. This is the user that is determined,
// in a platform specific way, to be the primary user controlling the
// application. This may be because that user launched the app, or using
// whatever criteria is appropriate for the platform. It is expected that there
// will be a unique SbUser per signed in user, and that the referenced objects
// will persist for the lifetime of the app.
SB_EXPORT SbUser SbUserGetCurrent();

// Returns whether |user| is age-restricted according to the platform's age
// policies.
SB_EXPORT bool SbUserIsAgeRestricted(SbUser user);

// Gets the size of the value of |property_id| for |user|, INCLUDING the
// terminating null character. Returns 0 if if |user| is invalid, |property_id|
// isn't recognized, supported, or set for |user|.
SB_EXPORT int SbUserGetPropertySize(SbUser user, SbUserPropertyId property_id);

// Gets the value of |property_id| for |user|, and places it in |out_value|,
// returning false if |user| is invalid, |property_id| isn't recognized,
// supported, or set for |user|, or if |value_size| is too small.
SB_EXPORT bool SbUserGetProperty(SbUser user,
                                 SbUserPropertyId property_id,
                                 char* out_value,
                                 int value_size);

// Begins the user sign-in flow for the given platform, which may result in a
// user changed event being dispatched.
SB_EXPORT void SbUserStartSignIn();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_USER_H_
