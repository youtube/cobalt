---
layout: doc
title: "Starboard Module Reference: storage.h"
---

Defines a user-based Storage API. This is a simple, all-at-once BLOB storage
and retrieval API that is intended for robust long-term, per-user storage.
Some platforms have different mechanisms for this kind of storage, so this
API exists to allow a client application to access this kind of storage.<br>
Note that there can be only one storage record per user and, thus, a maximum
of one open storage record can exist for each user. Attempting to open a
second record for a user will result in undefined behavior.<br>
These APIs are NOT expected to be thread-safe, so either call them from a
single thread, or perform proper synchronization around all calls.

## Structs

### SbStorageRecordPrivate

Private structure representing a single storage record.

## Functions

### SbStorageCloseRecord

**Description**

Closes `record`, synchronously ensuring that all written data is flushed.
This function performs blocking I/O on the calling thread.<br>
The return value indicates whether the operation succeeded. Storage writes
should be as atomic as possible, so the record should either be fully
written or deleted (or, even better, untouched).

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStorageCloseRecord-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStorageCloseRecord-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStorageCloseRecord-declaration">
<pre>
SB_EXPORT bool SbStorageCloseRecord(SbStorageRecord record);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStorageCloseRecord-stub">

```
#include "starboard/storage.h"

bool SbStorageCloseRecord(SbStorageRecord /*record*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbStorageRecord</code><br>        <code>record</code></td>
    <td>The storage record to close. <code>record</code> is invalid after this point,
and subsequent calls referring to <code>record</code> will fail.</td>
  </tr>
</table>

### SbStorageDeleteRecord

**Description**

Deletes the `SbStorageRecord` for `user` named `name`. The return value
indicates whether the record existed and was successfully deleted. If the
record did not exist or could not be deleted, the function returns `false`.<br>
If `name` is NULL, deletes the default storage record for the user, like what
would have been deleted with the previous version of <code>SbStorageDeleteRecord</code>.<br>
This function must not be called while the user's storage record is open.
This function performs blocking I/O on the calling thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStorageDeleteRecord-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStorageDeleteRecord-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStorageDeleteRecord-declaration">
<pre>
SB_EXPORT bool SbStorageDeleteRecord(SbUser user, const char* name);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStorageDeleteRecord-stub">

```
#include "starboard/storage.h"

bool SbStorageDeleteRecord(SbUser /*user*/
#if SB_API_VERSION >= 6
                           ,
                           const char* /*name*/
#endif  // SB_API_VERSION >= 6
                           ) {
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
    <td>The user for whom the record will be deleted.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>name</code></td>
    <td>The filesystem-safe name of the record to open.</td>
  </tr>
</table>

### SbStorageGetRecordSize

**Description**

Returns the size of `record`, or `-1` if there is an error. This function
performs blocking I/O on the calling thread.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStorageGetRecordSize-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStorageGetRecordSize-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStorageGetRecordSize-declaration">
<pre>
SB_EXPORT int64_t SbStorageGetRecordSize(SbStorageRecord record);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStorageGetRecordSize-stub">

```
#include "starboard/storage.h"

int64_t SbStorageGetRecordSize(SbStorageRecord /*record*/) {
  return -1;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbStorageRecord</code><br>        <code>record</code></td>
    <td>The record to retrieve the size of.</td>
  </tr>
</table>

### SbStorageIsValidRecord

**Description**

Returns whether the given storage record handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbStorageIsValidRecord(SbStorageRecord record) {
  return record != kSbStorageInvalidRecord;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbStorageRecord</code><br>
        <code>record</code></td>
    <td> </td>
  </tr>
</table>

### SbStorageOpenRecord

**Description**

Opens and returns the SbStorageRecord for `user` named `name`, blocking I/O
on the calling thread until the open is completed. If `user` is not a valid
`SbUser`, the function returns `kSbStorageInvalidRecord`. Will return an
`SbStorageRecord` of size zero if the record does not yet exist. Opening an
already-open `SbStorageRecord` has undefined behavior.<br>
If `name` is NULL, opens the default storage record for the user, like what
would have been saved with the previous version of <code>SbStorageOpenRecord</code>.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStorageOpenRecord-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStorageOpenRecord-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStorageOpenRecord-declaration">
<pre>
SB_EXPORT SbStorageRecord SbStorageOpenRecord(SbUser user, const char* name);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStorageOpenRecord-stub">

```
#include "starboard/storage.h"

SbStorageRecord SbStorageOpenRecord(SbUser /*user*/
#if SB_API_VERSION >= 6
                                    ,
                                    const char* /*name*/
#endif  // SB_API_VERSION >= 6
                                    ) {
  return kSbStorageInvalidRecord;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbUser</code><br>        <code>user</code></td>
    <td>The user for which the storage record will be opened.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>name</code></td>
    <td>The filesystem-safe name of the record to open.</td>
  </tr>
</table>

### SbStorageReadRecord

**Description**

Reads up to `data_size` bytes from `record`, starting at the beginning of
the record. The function returns the actual number of bytes read, which
must be <= `data_size`. The function returns `-1` in the event of an error.
This function makes a best-effort to read the entire record, and it performs
blocking I/O on the calling thread until the entire record is read or an
error is encountered.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStorageReadRecord-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStorageReadRecord-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStorageReadRecord-declaration">
<pre>
SB_EXPORT int64_t SbStorageReadRecord(SbStorageRecord record,
                                      char* out_data,
                                      int64_t data_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStorageReadRecord-stub">

```
#include "starboard/storage.h"

int64_t SbStorageReadRecord(SbStorageRecord /*record*/,
                            char* /*out_data*/,
                            int64_t /*data_size*/) {
  return -1;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbStorageRecord</code><br>        <code>record</code></td>
    <td>The record to be read.</td>
  </tr>
  <tr>
    <td><code>char*</code><br>        <code>out_data</code></td>
    <td>The data read from the record.</td>
  </tr>
  <tr>
    <td><code>int64_t</code><br>        <code>data_size</code></td>
    <td>The amount of data, in bytes, to read.</td>
  </tr>
</table>

### SbStorageWriteRecord

**Description**

Replaces the data in `record` with `data_size` bytes from `data`. This
function always deletes any previous data in that record. The return value
indicates whether the write succeeded. This function makes a best-effort to
write the entire record, and it may perform blocking I/O on the calling
thread until the entire record is written or an error is encountered.<br>
While `SbStorageWriteRecord()` may defer the persistence,
`SbStorageReadRecord()` is expected to work as expected immediately
afterwards, even without a call to `SbStorageCloseRecord()`. The data should
be persisted after a short time, even if there is an unexpected process
termination before `SbStorageCloseRecord()` is called.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbStorageWriteRecord-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbStorageWriteRecord-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbStorageWriteRecord-declaration">
<pre>
SB_EXPORT bool SbStorageWriteRecord(SbStorageRecord record,
                                    const char* data,
                                    int64_t data_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbStorageWriteRecord-stub">

```
#include "starboard/storage.h"

bool SbStorageWriteRecord(SbStorageRecord /*record*/,
                          const char* /*data*/,
                          int64_t /*data_size*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbStorageRecord</code><br>        <code>record</code></td>
    <td>The record to be written to.</td>
  </tr>
  <tr>
    <td><code>const char*</code><br>        <code>data</code></td>
    <td>The data to write to the record.</td>
  </tr>
  <tr>
    <td><code>int64_t</code><br>        <code>data_size</code></td>
    <td>The amount of <code>data</code>, in bytes, to write to the record. Thus,
if <code>data_size</code> is smaller than the total size of <code>data</code>, only part of
<code>data</code> is written to the record.</td>
  </tr>
</table>

