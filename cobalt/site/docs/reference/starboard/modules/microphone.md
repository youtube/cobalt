---
layout: doc
title: "Starboard Module Reference: microphone.h"
---

Defines functions for microphone creation, control, audio data fetching,
and destruction. This module supports multiple calls to |SbMicrophoneOpen|
and |SbMicrophoneClose|, and the implementation should handle multiple calls
to one of those functions on the same microphone. For example, your
implementation should handle cases where |SbMicrophoneOpen| is called twice
on the same microphone without a call to |SbMicrophoneClose| in between.<br>
This API is not thread-safe and must be called from a single thread.<br>
How to use this API:
<ol><li>Call |SbMicrophoneGetAvailableInfos| to get a list of available microphone
information.
</li><li>Create a supported microphone, using |SbMicrophoneCreate|, with enough
buffer size and sample rate. Use |SbMicrophoneIsSampleRateSupported| to
verify the sample rate.
</li><li>Use |SbMicrophoneOpen| to open the microphone port and start recording
audio data.
</li><li>Periodically read out the data from microphone with |SbMicrophoneRead|.
</li><li>Call |SbMicrophoneClose| to close the microphone port and stop recording
audio data.
</li><li>Destroy the microphone with |SbMicrophoneDestroy|.</li></ol>

## Enums

### SbMicrophoneType

All possible microphone types.

**Values**

*   `kSbMicrophoneCamera` - Built-in microphone in camera.
*   `kSbMicrophoneUSBHeadset` - Microphone in the headset that can be a wired or wireless USB headset.
*   `kSbMicrophoneVRHeadset` - Microphone in the VR headset.
*   `kSBMicrophoneAnalogHeadset` - Microphone in the analog headset.
*   `kSbMicrophoneUnknown` - Unknown microphone type. The microphone could be different than the otherenum descriptions or could fall under one of those descriptions.

## Structs

### SbMicrophoneId

An opaque handle to an implementation-private structure that represents a
microphone ID.

### SbMicrophoneInfo

Microphone information.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbMicrophoneId</code><br>        <code>id</code></td>    <td>Microphone id.</td>  </tr>
  <tr>
    <td><code>SbMicrophoneType</code><br>        <code>type</code></td>    <td>Microphone type.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>max_sample_rate_hz</code></td>    <td>The microphone's maximum supported sampling rate.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>min_read_size</code></td>    <td>The minimum read size required for each read from microphone.</td>  </tr>
</table>

### SbMicrophone

An opaque handle to an implementation-private structure that represents a
microphone.

## Functions

### SbMicrophoneClose

**Description**

Closes the microphone port, stops recording audio on `microphone`, and
clears the unread buffer if it is not empty. If the microphone has already
been stopped, this call is ignored. The return value indicates whether the
microphone is closed.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMicrophoneClose-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMicrophoneClose-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMicrophoneClose-declaration">
<pre>
SB_EXPORT bool SbMicrophoneClose(SbMicrophone microphone);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMicrophoneClose-stub">

```
#include "starboard/microphone.h"

#if !SB_HAS(MICROPHONE)
#error "SB_HAS_MICROPHONE must be set to build this file."
#endif

bool SbMicrophoneClose(SbMicrophone microphone) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophone</code><br>        <code>microphone</code></td>
    <td>The microphone to close.</td>
  </tr>
</table>

### SbMicrophoneCreate

**Description**

Creates a microphone with the specified ID, audio sample rate, and cached
audio buffer size. Starboard only requires support for creating one
microphone at a time, and implementations may return an error if a second
microphone is created before the first is destroyed.<br>
The function returns the newly created SbMicrophone object. However, if you
try to create a microphone that has already been initialized, if the sample
rate is unavailable, or if the buffer size is invalid, the function should
return `kSbMicrophoneInvalid`.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMicrophoneCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMicrophoneCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMicrophoneCreate-declaration">
<pre>
SB_EXPORT SbMicrophone SbMicrophoneCreate(SbMicrophoneId id,
                                          int sample_rate_in_hz,
                                          int buffer_size_bytes);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMicrophoneCreate-stub">

```
#include "starboard/microphone.h"

#if !SB_HAS(MICROPHONE)
#error "SB_HAS_MICROPHONE must be set to build this file."
#endif

SbMicrophone SbMicrophoneCreate(SbMicrophoneId id,
                                int sample_rate_in_hz,
                                int buffer_size) {
  return kSbMicrophoneInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophoneId</code><br>        <code>id</code></td>
    <td>The ID that will be assigned to the newly created SbMicrophone.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>sample_rate_in_hz</code></td>
    <td>The new microphone's audio sample rate in Hz.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>buffer_size_bytes</code></td>
    <td>The size of the buffer where signed 16-bit integer
audio data is temporarily cached to during the capturing. The audio data
is removed from the audio buffer if it has been read, and new audio data
can be read from this buffer in smaller chunks than this size. This
parameter must be set to a value greater than zero and the ideal size is
<code>2^n</code>.</td>
  </tr>
</table>

### SbMicrophoneDestroy

**Description**

Destroys a microphone. If the microphone is in started state, it is first
stopped and then destroyed. Any data that has been recorded and not read
is thrown away.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMicrophoneDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMicrophoneDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMicrophoneDestroy-declaration">
<pre>
SB_EXPORT void SbMicrophoneDestroy(SbMicrophone microphone);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMicrophoneDestroy-stub">

```
#include "starboard/microphone.h"

#if !SB_HAS(MICROPHONE)
#error "SB_HAS_MICROPHONE must be set to build this file."
#endif

void SbMicrophoneDestroy(SbMicrophone microphone) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophone</code><br>
        <code>microphone</code></td>
    <td> </td>
  </tr>
</table>

### SbMicrophoneGetAvailable

**Description**

Retrieves all currently available microphone information and stores it in
`out_info_array`. The return value is the number of the available
microphones. If the number of available microphones is larger than
`info_array_size`, then `out_info_array` is filled up with as many available
microphones as possible and the actual number of available microphones is
returned. A negative return value indicates that an internal error occurred.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMicrophoneGetAvailable-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMicrophoneGetAvailable-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMicrophoneGetAvailable-declaration">
<pre>
SB_EXPORT int SbMicrophoneGetAvailable(SbMicrophoneInfo* out_info_array,
                                       int info_array_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMicrophoneGetAvailable-stub">

```
#include "starboard/microphone.h"

#if !SB_HAS(MICROPHONE)
#error "SB_HAS_MICROPHONE must be set to build this file."
#endif

int SbMicrophoneGetAvailable(SbMicrophoneInfo* out_info_array,
                             int info_array_size) {
  return 0;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophoneInfo*</code><br>        <code>out_info_array</code></td>
    <td>All currently available information about the microphone
is placed into this output parameter.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>info_array_size</code></td>
    <td>The size of <code>out_info_array</code>.</td>
  </tr>
</table>

### SbMicrophoneIdIsValid

**Description**

Indicates whether the given microphone ID is valid.

**Declaration**

```
static SB_C_INLINE bool SbMicrophoneIdIsValid(SbMicrophoneId id) {
  return id != kSbMicrophoneIdInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophoneId</code><br>
        <code>id</code></td>
    <td> </td>
  </tr>
</table>

### SbMicrophoneIsSampleRateSupported

**Description**

Indicates whether the microphone supports the sample rate.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMicrophoneIsSampleRateSupported-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMicrophoneIsSampleRateSupported-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMicrophoneIsSampleRateSupported-declaration">
<pre>
SB_EXPORT bool SbMicrophoneIsSampleRateSupported(SbMicrophoneId id,
                                                 int sample_rate_in_hz);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMicrophoneIsSampleRateSupported-stub">

```
#include "starboard/microphone.h"

#if !SB_HAS(MICROPHONE)
#error "SB_HAS_MICROPHONE must be set to build this file."
#endif

bool SbMicrophoneIsSampleRateSupported(SbMicrophoneId id,
                                       int sample_rate_in_hz) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophoneId</code><br>
        <code>id</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>sample_rate_in_hz</code></td>
    <td> </td>
  </tr>
</table>

### SbMicrophoneIsValid

**Description**

Indicates whether the given microphone is valid.

**Declaration**

```
static SB_C_INLINE bool SbMicrophoneIsValid(SbMicrophone microphone) {
  return microphone != kSbMicrophoneInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophone</code><br>
        <code>microphone</code></td>
    <td> </td>
  </tr>
</table>

### SbMicrophoneOpen

**Description**

Opens the microphone port and starts recording audio on `microphone`.<br>
Once started, the client needs to periodically call `SbMicrophoneRead` to
receive the audio data. If the microphone has already been started, this call
clears the unread buffer. The return value indicates whether the microphone
is open.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMicrophoneOpen-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMicrophoneOpen-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMicrophoneOpen-declaration">
<pre>
SB_EXPORT bool SbMicrophoneOpen(SbMicrophone microphone);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMicrophoneOpen-stub">

```
#include "starboard/microphone.h"

#if !SB_HAS(MICROPHONE)
#error "SB_HAS_MICROPHONE must be set to build this file."
#endif

bool SbMicrophoneOpen(SbMicrophone microphone) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophone</code><br>        <code>microphone</code></td>
    <td>The microphone that will be opened and will start recording
audio.</td>
  </tr>
</table>

### SbMicrophoneRead

**Description**

Retrieves the recorded audio data from the microphone and writes that data
to `out_audio_data`.<br>
The return value is zero or the positive number of bytes that were read.
Neither the return value nor `audio_data_size` exceeds the buffer size.
A negative return value indicates that an error occurred.<br>
This function should be called frequently. Otherwise, the microphone only
buffers `buffer_size` bytes as configured in `SbMicrophoneCreate` and the
new audio data is thrown out. No audio data is read from a stopped
microphone.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbMicrophoneRead-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbMicrophoneRead-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbMicrophoneRead-declaration">
<pre>
SB_EXPORT int SbMicrophoneRead(SbMicrophone microphone,
                               void* out_audio_data,
                               int audio_data_size);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbMicrophoneRead-stub">

```
#include "starboard/microphone.h"

#if !SB_HAS(MICROPHONE)
#error "SB_HAS_MICROPHONE must be set to build this file."
#endif

int SbMicrophoneRead(SbMicrophone microphone,
                     void* out_audio_data,
                     int audio_data_size) {
  return -1;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMicrophone</code><br>        <code>microphone</code></td>
    <td>The microphone from which to retrieve recorded audio data.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>out_audio_data</code></td>
    <td>The buffer to which the retrieved data will be written.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>audio_data_size</code></td>
    <td>The number of requested bytes. If <code>audio_data_size</code> is
smaller than <code>min_read_size</code> of <code>SbMicrophoneInfo</code>, the extra audio data
that has already been read from the device is discarded.</td>
  </tr>
</table>

