---
layout: doc
title: "Starboard Module Reference: user.h"
---

Defines a user management API. This module defines functions only for
managing signed-in users. Platforms that do not have users must still
implement this API, always reporting a single user that is current and
signed in.<br>
These APIs are NOT expected to be thread-safe, so either call them from a
single thread, or perform proper synchronization around all calls.

## Enums

### SbUserPropertyId

A set of string properties that can be queried on a user.

**Values**

*   `kSbUserPropertyAvatarUrl` - The URL to the avatar for a user. Avatars are not provided on allplatforms.
*   `kSbUserPropertyHomeDirectory` - The path to a user's home directory, if supported on this platform.
*   `kSbUserPropertyUserName` - The username of a user, which may be the same as the User ID, or it may befriendlier.
*   `kSbUserPropertyUserId` - A unique user ID of a user.

## Structs

### SbUserPrivate

Private structure representing a device user.

## Functions

### SbUserGetCurrent

**Description**

Gets the current primary user, if one exists. This is the user that is
determined, in a platform-specific way, to be the primary user controlling
the application. For example, the determination might be made because that
user launched the app, though it should be made using whatever criteria are
appropriate for the platform.<br>
It is expected that there will be a unique SbUser per signed-in user, and
that the referenced objects will persist for the lifetime of the app.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbUserGetCurrent-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbUserGetCurrent-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbUserGetCurrent-declaration">
<pre>
SB_EXPORT SbUser SbUserGetCurrent();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbUserGetCurrent-stub">

```
#include "starboard/user.h"

SbUser SbUserGetCurrent() {
  return kSbUserInvalid;
}
```

  </div>
</div>

### SbUserGetProperty

**Description**

Retrieves the value of `property_id` for `user` and places it in `out_value`.
The function returns:
<ul><li>`true` if the property value is retrieved successfully
</li><li>`false` if `user` is invalid; if `property_id` isn't recognized, supported,
or set for `user`; or if `value_size` is too small.</li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbUserGetProperty-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbUserGetProperty-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbUserGetProperty-declaration">
<pre>
SB_EXPORT bool SbUserGetProperty(SbUser user,
                                 SbUserPropertyId property_id,
                                 char* out_value,
                                 int value_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbUserGetProperty-stub">

```
#include "starboard/user.h"

int SbUserGetPropertySize(SbUser /*user*/, SbUserPropertyId /*property_id*/) {
  return 0;
}

bool SbUserGetProperty(SbUser /*user*/,
                       SbUserPropertyId /*property_id*/,
                       char* /*out_value*/,
                       int /*value_size*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbUser</code><br>        <code>user</code></td>
    <td>The user for which property size data is being retrieved.</td>
  </tr>
  <tr>
    <td><code>SbUserPropertyId</code><br>        <code>property_id</code></td>
    <td>The property for which the data is requested.</td>
  </tr>
  <tr>
    <td><code>char*</code><br>        <code>out_value</code></td>
    <td>The retrieved property value.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>value_size</code></td>
    <td>The size of the retrieved property value.</td>
  </tr>
</table>

### SbUserGetPropertySize

**Description**

Returns the size of the value of `property_id` for `user`, INCLUDING the
terminating null character. The function returns `0` if `user` is invalid
or if `property_id` is not recognized, supported, or set for the user.

**Declaration**

```
SB_EXPORT int SbUserGetPropertySize(SbUser user, SbUserPropertyId property_id);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbUser</code><br>        <code>user</code></td>
    <td>The user for which property size data is being retrieved.</td>
  </tr>
  <tr>
    <td><code>SbUserPropertyId</code><br>        <code>property_id</code></td>
    <td>The property for which the data is requested.</td>
  </tr>
</table>

### SbUserGetSignedIn

**Description**

Gets a list of up to `users_size` signed-in users and places the results in
`out_users`. The return value identifies the actual number of signed-in
users, which may be greater or less than `users_size`.<br>
It is expected that there will be a unique `SbUser` per signed-in user and
that the referenced objects will persist for the lifetime of the app.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbUserGetSignedIn-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbUserGetSignedIn-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbUserGetSignedIn-declaration">
<pre>
SB_EXPORT int SbUserGetSignedIn(SbUser* out_users, int users_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbUserGetSignedIn-stub">

```
#include "starboard/user.h"

int SbUserGetSignedIn(SbUser* /*out_users*/, int /*users_size*/) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbUser*</code><br>        <code>out_users</code></td>
    <td>Handles for the retrieved users.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>users_size</code></td>
    <td>The maximum number of signed-in users to retrieve.</td>
  </tr>
</table>

### SbUserIsValid

**Description**

Returns whether the given user handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbUserIsValid(SbUser user) {
  return user != kSbUserInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbUser</code><br>
        <code>user</code></td>
    <td> </td>
  </tr>
</table>

