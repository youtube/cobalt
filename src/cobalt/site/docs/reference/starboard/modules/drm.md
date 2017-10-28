---
layout: doc
title: "Starboard Module Reference: drm.h"
---

Provides definitions that allow for DRM support, which are common
between Player and Decoder interfaces.

## Enums

### SbDrmKeyStatus

Status of a particular media key.
https://w3c.github.io/encrypted-media/#idl-def-MediaKeyStatus

**Values**

*   `kSbDrmKeyStatusUsable`
*   `kSbDrmKeyStatusExpired`
*   `kSbDrmKeyStatusReleased`
*   `kSbDrmKeyStatusRestricted`
*   `kSbDrmKeyStatusDownscaled`
*   `kSbDrmKeyStatusPending`
*   `kSbDrmKeyStatusError`

## Structs

### SbDrmKeyId

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>uint8_t</code><br>        <code>identifier[16]</code></td>    <td>The ID of the license (or key) required to decrypt this sample. For
PlayReady, this is the license GUID in packed little-endian binary form.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>identifier_size</code></td>    <td></td>  </tr>
</table>

### SbDrmSampleInfo

All the optional information needed per sample for encrypted samples.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>uint8_t</code><br>        <code>initialization_vector[16]</code></td>    <td>The Initialization Vector needed to decrypt this sample.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>initialization_vector_size</code></td>    <td></td>  </tr>
  <tr>
    <td><code>uint8_t</code><br>        <code>identifier[16]</code></td>    <td>The ID of the license (or key) required to decrypt this sample. For
PlayReady, this is the license GUID in packed little-endian binary form.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>identifier_size</code></td>    <td></td>  </tr>
  <tr>
    <td><code>int32_t</code><br>        <code>subsample_count</code></td>    <td>The number of subsamples in this sample, must be at least 1.</td>  </tr>
  <tr>
    <td><code>const</code><br>        <code>SbDrmSubSampleMapping* subsample_mapping</code></td>    <td>The clear/encrypted mapping of each subsample in this sample. This must be
an array of <code>subsample_count</code> mappings.</td>  </tr>
</table>

### SbDrmSubSampleMapping

A mapping of clear and encrypted bytes for a single subsample. All
subsamples within a sample must be encrypted with the same encryption
parameters. The clear bytes always appear first in the sample.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int32_t</code><br>        <code>clear_byte_count</code></td>    <td>How many bytes of the corresponding subsample are not encrypted</td>  </tr>
  <tr>
    <td><code>int32_t</code><br>        <code>encrypted_byte_count</code></td>    <td>How many bytes of the corresponding subsample are encrypted.</td>  </tr>
</table>

### SbDrmSystem

A handle to a DRM system which can be used with either an SbDecoder or a
SbPlayer.

## Functions

### SbDrmCloseSession

**Description**

Clear any internal states/resources related to the specified `session_id`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDrmCloseSession-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDrmCloseSession-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDrmCloseSession-declaration">
<pre>
SB_EXPORT void SbDrmCloseSession(SbDrmSystem drm_system,
                                 const void* session_id,
                                 int session_id_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDrmCloseSession-stub">

```
#include "starboard/drm.h"

void SbDrmCloseSession(SbDrmSystem drm_system,
                       const void* session_id,
                       int session_id_size) {
  SB_UNREFERENCED_PARAMETER(drm_system);
  SB_UNREFERENCED_PARAMETER(session_id);
  SB_UNREFERENCED_PARAMETER(session_id_size);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDrmSystem</code><br>
        <code>drm_system</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const void*</code><br>
        <code>session_id</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>session_id_size</code></td>
    <td> </td>
  </tr>
</table>

### SbDrmCreateSystem

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDrmCreateSystem-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDrmCreateSystem-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDrmCreateSystem-declaration">
<pre>
SB_EXPORT SbDrmSystem
SbDrmCreateSystem(const char* key_system,
                  void* context,
                  SbDrmSessionUpdateRequestFunc update_request_callback,
                  SbDrmSessionUpdatedFunc session_updated_callback);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDrmCreateSystem-stub">

```
#include "starboard/drm.h"

#if SB_API_VERSION >= 6

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback,
    SbDrmSessionKeyStatusesChangedFunc key_statuses_changed_callback) {
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(key_system);
  SB_UNREFERENCED_PARAMETER(update_request_callback);
  SB_UNREFERENCED_PARAMETER(session_updated_callback);
  SB_UNREFERENCED_PARAMETER(key_statuses_changed_callback);
  return kSbDrmSystemInvalid;
}

#else  // SB_API_VERSION >= 6

SbDrmSystem SbDrmCreateSystem(
    const char* key_system,
    void* context,
    SbDrmSessionUpdateRequestFunc update_request_callback,
    SbDrmSessionUpdatedFunc session_updated_callback) {
  SB_UNREFERENCED_PARAMETER(context);
  SB_UNREFERENCED_PARAMETER(key_system);
  SB_UNREFERENCED_PARAMETER(update_request_callback);
  SB_UNREFERENCED_PARAMETER(session_updated_callback);
  return kSbDrmSystemInvalid;
}

#endif  // SB_API_VERSION >= 6
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>
        <code>key_system</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>void*</code><br>
        <code>context</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDrmSessionUpdateRequestFunc</code><br>
        <code>update_request_callback</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDrmSessionUpdatedFunc</code><br>
        <code>session_updated_callback</code></td>
    <td> </td>
  </tr>
</table>

### SbDrmDestroySystem

**Description**

Destroys `drm_system`, which implicitly removes all keys installed in it and
invalidates all outstanding session update requests. A DRM system cannot be
destroyed unless any associated SbPlayer or SbDecoder has first been
destroyed.<br>
All callbacks are guaranteed to be finished when this function returns.
As a result, if this function is called from a callback that is passed
to <code><a href="#sbdrmcreatesystem">SbDrmCreateSystem()</a></code>, a deadlock will occur.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDrmDestroySystem-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDrmDestroySystem-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDrmDestroySystem-declaration">
<pre>
SB_EXPORT void SbDrmDestroySystem(SbDrmSystem drm_system);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDrmDestroySystem-stub">

```
#include "starboard/drm.h"

void SbDrmDestroySystem(SbDrmSystem drm_system) {
  SB_UNREFERENCED_PARAMETER(drm_system);
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDrmSystem</code><br>        <code>drm_system</code></td>
    <td>The DRM system to be destroyed.</td>
  </tr>
</table>

### SbDrmGenerateSessionUpdateRequest

**Description**

Asynchronously generates a session update request payload for
`initialization_data`, of `initialization_data_size`, in case sensitive
`type`, extracted from the media stream, in `drm_system`'s key system.<br>
This function calls `drm_system`'s `update_request_callback` function,
which is defined when the DRM system is created by <code><a href="#sbdrmcreatesystem">SbDrmCreateSystem</a></code>. When
calling that function, this function either sends `context` (also from
`SbDrmCreateSystem`) and a populated request, or it sends NULL `session_id`
if an error occurred.<br>
`drm_system`'s `context` may be used to route callbacks back to an object
instance.<br>
Callbacks may be called either from the current thread before this function
returns or from another thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDrmGenerateSessionUpdateRequest-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDrmGenerateSessionUpdateRequest-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDrmGenerateSessionUpdateRequest-declaration">
<pre>
SB_EXPORT void SbDrmGenerateSessionUpdateRequest(
    SbDrmSystem drm_system,
    int ticket,
    const char* type,
    const void* initialization_data,
    int initialization_data_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDrmGenerateSessionUpdateRequest-stub">

```
#include "starboard/drm.h"

void SbDrmGenerateSessionUpdateRequest(SbDrmSystem /*drm_system*/,
                                       int /*ticket*/,
                                       const char* /*type*/,
                                       const void* /*initialization_data*/,
                                       int /*initialization_data_size*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDrmSystem</code><br>        <code>drm_system</code></td>
    <td>The DRM system that defines the key system used for the
session update request payload as well as the callback function that is
called as a result of the function being called.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>ticket</code></td>
    <td>The opaque ID that allows to distinguish callbacks from multiple
concurrent calls to <code>SbDrmGenerateSessionUpdateRequest()</code>, which will be passed
to <code>update_request_callback</code> as-is. It is the responsibility of the caller to
establish ticket uniqueness, issuing multiple request with the same ticket
may result in undefined behavior. The value <code>kSbDrmTicketInvalid</code> must not be
used.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>type</code></td>
    <td>The case-sensitive type of the session update request payload.</td>
  </tr>
  <tr>
    <td><code>const void*</code><br>        <code>initialization_data</code></td>
    <td>The data for which the session update request payload
is created.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>initialization_data_size</code></td>
    <td>The size of the session update request payload.</td>
  </tr>
</table>

### SbDrmGetKeyCount

**Description**

Returns the number of keys installed in `drm_system`.

**Declaration**

```
SB_EXPORT int SbDrmGetKeyCount(SbDrmSystem drm_system);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDrmSystem</code><br>        <code>drm_system</code></td>
    <td>The system for which the number of installed keys is retrieved.</td>
  </tr>
</table>

### SbDrmGetKeyStatus

**Description**

Gets `out_key`, `out_key_size`, and `out_status` for the key with `index`
in `drm_system`. Returns whether a key is installed at `index`.
If not, the output parameters, which all must not be NULL, will not be
modified.

**Declaration**

```
SB_EXPORT bool SbDrmGetKeyStatus(SbDrmSystem drm_system,
                                 const void* session_id,
                                 int session_id_size,
                                 int index,
                                 void** out_key,
                                 int* out_key_size,
                                 SbDrmKeyStatus* out_status);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDrmSystem</code><br>
        <code>drm_system</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const void*</code><br>
        <code>session_id</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>session_id_size</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>index</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>void**</code><br>
        <code>out_key</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int*</code><br>
        <code>out_key_size</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDrmKeyStatus*</code><br>
        <code>out_status</code></td>
    <td> </td>
  </tr>
</table>

### SbDrmRemoveAllKeys

**Description**

Removes all installed keys for `drm_system`. Any outstanding session update
requests are also invalidated.

**Declaration**

```
SB_EXPORT void SbDrmRemoveAllKeys(SbDrmSystem drm_system);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDrmSystem</code><br>        <code>drm_system</code></td>
    <td>The DRM system for which keys should be removed.</td>
  </tr>
</table>

### SbDrmSystemIsValid

**Description**

Indicates whether `drm_system` is a valid SbDrmSystem.

**Declaration**

```
static SB_C_FORCE_INLINE bool SbDrmSystemIsValid(SbDrmSystem drm) {
  return drm != kSbDrmSystemInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDrmSystem</code><br>
        <code>drm</code></td>
    <td> </td>
  </tr>
</table>

### SbDrmTicketIsValid

**Description**

Indicates whether `ticket` is a valid ticket.

**Declaration**

```
static SB_C_FORCE_INLINE bool SbDrmTicketIsValid(int ticket) {
  return ticket != kSbDrmTicketInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>
        <code>ticket</code></td>
    <td> </td>
  </tr>
</table>

### SbDrmUpdateSession

**Description**

Update session with `key`, in `drm_system`'s key system, from the license
server response. Calls `session_updated_callback` with `context` and whether
the update succeeded. `context` may be used to route callbacks back to
an object instance.<br>
`ticket` is the opaque ID that allows to distinguish callbacks from
multiple concurrent calls to <code>SbDrmUpdateSession()</code>, which will be passed
to `session_updated_callback` as-is. It is the responsibility of the caller
to establish ticket uniqueness, issuing multiple calls with the same ticket
may result in undefined behavior.<br>
Once the session is successfully updated, an SbPlayer or SbDecoder associated
with that DRM key system will be able to decrypt encrypted samples.<br>
`drm_system`'s `session_updated_callback` may called either from the
current thread before this function returns or from another thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbDrmUpdateSession-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbDrmUpdateSession-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbDrmUpdateSession-declaration">
<pre>
SB_EXPORT void SbDrmUpdateSession(SbDrmSystem drm_system,
                                  int ticket,
                                  const void* key,
                                  int key_size,
                                  const void* session_id,
                                  int session_id_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbDrmUpdateSession-stub">

```
#include "starboard/drm.h"

void SbDrmUpdateSession(SbDrmSystem /*drm_system*/,
                        int /*ticket*/,
                        const void* /*key*/,
                        int /*key_size*/,
                        const void* /*session_id*/,
                        int /*session_id_size*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDrmSystem</code><br>
        <code>drm_system</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>ticket</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const void*</code><br>
        <code>key</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>key_size</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const void*</code><br>
        <code>session_id</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>session_id_size</code></td>
    <td> </td>
  </tr>
</table>

