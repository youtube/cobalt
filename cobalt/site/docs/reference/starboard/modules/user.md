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

### SbUserIsValid ###

Returns whether the given user handle is valid.

#### Declaration ####

```
static bool SbUserIsValid(SbUser user)
```
