---
layout: doc
title: "Starboard Module Reference: user.h"
---

Defines a user management API. This module defines functions only for managing
signed-in users. Platforms that do not have users must still implement this API,
always reporting a single user that is current and signed in.

These APIs are NOT expected to be thread-safe, so either call them from a single
thread, or perform proper synchronization around all calls.

## Macros ##

### kSbUserInvalid ###

Well-defined value for an invalid user.

## Enums ##

### SbUserPropertyId ###

A set of string properties that can be queried on a user.

#### Values ####

*   `kSbUserPropertyAvatarUrl`

    The URL to the avatar for a user. Avatars are not provided on all platforms.
*   `kSbUserPropertyHomeDirectory`

    The path to a user's home directory, if supported on this platform.
*   `kSbUserPropertyUserName`

    The username of a user, which may be the same as the User ID, or it may be
    friendlier.
*   `kSbUserPropertyUserId`

    A unique user ID of a user.

## Typedefs ##

### SbUser ###

A handle to a user.

#### Definition ####

```
typedef SbUserPrivate* SbUser
```

## Functions ##

### SbUserGetCurrent ###

Gets the current primary user, if one exists. This is the user that is
determined, in a platform-specific way, to be the primary user controlling the
application. For example, the determination might be made because that user
launched the app, though it should be made using whatever criteria are
appropriate for the platform.

It is expected that there will be a unique SbUser per signed-in user, and that
the referenced objects will persist for the lifetime of the app.

#### Declaration ####

```
SbUser SbUserGetCurrent()
```

### SbUserGetProperty ###

Retrieves the value of `property_id` for `user` and places it in `out_value`.
The function returns:

*   `true` if the property value is retrieved successfully

*   `false` if `user` is invalid; if `property_id` isn't recognized, supported,
    or set for `user`; or if `value_size` is too small.

`user`: The user for which property size data is being retrieved. `property_id`:
The property for which the data is requested. `out_value`: The retrieved
property value. `value_size`: The size of the retrieved property value.

#### Declaration ####

```
bool SbUserGetProperty(SbUser user, SbUserPropertyId property_id, char *out_value, int value_size)
```

### SbUserGetPropertySize ###

Returns the size of the value of `property_id` for `user`, INCLUDING the
terminating null character. The function returns `0` if `user` is invalid or if
`property_id` is not recognized, supported, or set for the user.

`user`: The user for which property size data is being retrieved. `property_id`:
The property for which the data is requested.

#### Declaration ####

```
int SbUserGetPropertySize(SbUser user, SbUserPropertyId property_id)
```

### SbUserGetSignedIn ###

Gets a list of up to `users_size` signed-in users and places the results in
`out_users`. The return value identifies the actual number of signed-in users,
which may be greater or less than `users_size`.

It is expected that there will be a unique `SbUser` per signed-in user and that
the referenced objects will persist for the lifetime of the app.

`out_users`: Handles for the retrieved users. `users_size`: The maximum number
of signed-in users to retrieve.

#### Declaration ####

```
int SbUserGetSignedIn(SbUser *out_users, int users_size)
```

### SbUserIsValid ###

Returns whether the given user handle is valid.

#### Declaration ####

```
static bool SbUserIsValid(SbUser user)
```

