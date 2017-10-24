---
layout: doc
title: "Starboard Module Reference: audio_sink.h"
---

Provides an interface to output raw audio data.

## Structs

### SbAudioSink

An opaque handle to an implementation-private structure representing an
audio sink.

## Functions

### SbAudioSinkCreate

**Description**

Creates an audio sink for the specified `channels` and
`sampling_frequency_hz`, acquires all resources needed to operate the
audio sink, and returns an opaque handle to the audio sink.<br>
If the particular platform doesn't support the requested audio sink, the
function returns kSbAudioSinkInvalid without calling any of the callbacks.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAudioSinkCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAudioSinkCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAudioSinkCreate-declaration">
<pre>
SB_EXPORT SbAudioSink
SbAudioSinkCreate(int channels,
                  int sampling_frequency_hz,
                  SbMediaAudioSampleType audio_sample_type,
                  SbMediaAudioFrameStorageType audio_frame_storage_type,
                  SbAudioSinkFrameBuffers frame_buffers,
                  int frames_per_channel,
                  SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
                  SbAudioSinkConsumeFramesFunc consume_frames_func,
                  void* context);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAudioSinkCreate-stub">

```
#include "starboard/audio_sink.h"

struct SbAudioSinkPrivate {};

namespace {
struct SbAudioSinkPrivate g_singleton_stub;
}

SbAudioSink SbAudioSinkCreate(
    int channels,
    int sampling_frequency_hz,
    SbMediaAudioSampleType audio_sample_type,
    SbMediaAudioFrameStorageType audio_frame_storage_type,
    SbAudioSinkFrameBuffers frame_buffers,
    int frames_per_channel,
    SbAudioSinkUpdateSourceStatusFunc update_source_status_func,
    SbAudioSinkConsumeFramesFunc consume_frames_func,
    void* context) {
  return &g_singleton_stub;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>        <code>channels</code></td>
    <td>The number of audio channels, such as left and right channels
in stereo audio.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>sampling_frequency_hz</code></td>
    <td>The sample frequency of the audio data being
streamed. For example, 22,000 Hz means 22,000 sample elements represents
one second of audio data.</td>
  </tr>
  <tr>
    <td><code>SbMediaAudioSampleType</code><br>        <code>audio_sample_type</code></td>
    <td>The type of each sample of the audio data --
<code>int16</code>, <code>float32</code>, etc.</td>
  </tr>
  <tr>
    <td><code>SbMediaAudioFrameStorageType</code><br>        <code>audio_frame_storage_type</code></td>
    <td>Indicates whether frames are interleaved or
planar.</td>
  </tr>
  <tr>
    <td><code>SbAudioSinkFrameBuffers</code><br>        <code>frame_buffers</code></td>
    <td>An array of pointers to sample data.
<ul><li>If the sink is operating in interleaved mode, the array contains only
one element, which is an array containing (<code>frames_per_channel</code> *
<code>channels</code>) samples.
</li><li>If the sink is operating in planar mode, the number of elements in the
array is the same as <code>channels</code>, and each element is an array of
<code>frames_per_channel</code> samples. The caller has to ensure that
<code>frame_buffers</code> is valid until <code><a href="#sbaudiosinkdestroy">SbAudioSinkDestroy</a></code> is called.</li></ul></td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>frames_per_channel</code></td>
    <td>The size of the frame buffers, in units of the
number of samples per channel. The frame, in this case, represents a
group of samples at the same media time, one for each channel.</td>
  </tr>
  <tr>
    <td><code>SbAudioSinkUpdateSourceStatusFunc</code><br>        <code>update_source_status_func</code></td>
    <td>The audio sink calls this function on an
internal thread to query the status of the source. The value cannot be NULL.  A function that the audio sink starts to call
immediately after <code>SbAudioSinkCreate</code> is called, even before it returns.
The caller has to ensure that the callback functions above return
meaningful values in this case.</td>
  </tr>
  <tr>
    <td><code>SbAudioSinkConsumeFramesFunc</code><br>        <code>consume_frames_func</code></td>
    <td>The audio sink calls this function on an internal
thread to report consumed frames. The value cannot be NULL.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>context</code></td>
    <td>A value that is passed back to all callbacks and is generally
used to point at a class or struct that contains state associated with the
audio sink.</td>
  </tr>
</table>

### SbAudioSinkDestroy

**Description**

Destroys `audio_sink`, freeing all associated resources. Before
returning, the function waits until all callbacks that are in progress
have finished. After the function returns, no further calls are made
callbacks passed into <code><a href="#sbaudiosinkcreate">SbAudioSinkCreate</a></code>. In addition, you can not pass
`audio_sink` to any other SbAudioSink functions after <code>SbAudioSinkDestroy</code>
has been called on it.<br>
This function can be called on any thread. However, it cannot be called
within any of the callbacks passed into <code><a href="#sbaudiosinkcreate">SbAudioSinkCreate</a></code>.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAudioSinkDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAudioSinkDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAudioSinkDestroy-declaration">
<pre>
SB_EXPORT void SbAudioSinkDestroy(SbAudioSink audio_sink);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAudioSinkDestroy-stub">

```
#include "starboard/audio_sink.h"

void SbAudioSinkDestroy(SbAudioSink /*audio_sink*/) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbAudioSink</code><br>        <code>audio_sink</code></td>
    <td>The audio sink to destroy.</td>
  </tr>
</table>

### SbAudioSinkGetMaxChannels

**Description**

Returns the maximum number of channels supported on the platform. For
example, the number would be `2` if the platform only supports stereo.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAudioSinkGetMaxChannels-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAudioSinkGetMaxChannels-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAudioSinkGetMaxChannels-declaration">
<pre>
SB_EXPORT int SbAudioSinkGetMaxChannels();
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAudioSinkGetMaxChannels-stub">

```
#include "starboard/audio_sink.h"

int SbAudioSinkGetMaxChannels() {
  return 2;
}
```

  </div>
</div>

### SbAudioSinkGetNearestSupportedSampleFrequency

**Description**

Returns the supported sample rate closest to `sampling_frequency_hz`.
On platforms that don't support all sample rates, it is the caller's
responsibility to resample the audio frames into the supported sample rate
returned by this function.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAudioSinkGetNearestSupportedSampleFrequency-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAudioSinkGetNearestSupportedSampleFrequency-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAudioSinkGetNearestSupportedSampleFrequency-declaration">
<pre>
SB_EXPORT int SbAudioSinkGetNearestSupportedSampleFrequency(
    int sampling_frequency_hz);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAudioSinkGetNearestSupportedSampleFrequency-stub">

```
#include "starboard/audio_sink.h"

int SbAudioSinkGetNearestSupportedSampleFrequency(
    int sampling_frequency_hz) {
  return sampling_frequency_hz;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>int</code><br>
        <code>sampling_frequency_hz</code></td>
    <td> </td>
  </tr>
</table>

### SbAudioSinkIsAudioFrameStorageTypeSupported

**Description**

Indicates whether `audio_frame_storage_type` is supported on this
platform.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAudioSinkIsAudioFrameStorageTypeSupported-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAudioSinkIsAudioFrameStorageTypeSupported-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAudioSinkIsAudioFrameStorageTypeSupported-declaration">
<pre>
SB_EXPORT bool SbAudioSinkIsAudioFrameStorageTypeSupported(
    SbMediaAudioFrameStorageType audio_frame_storage_type);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAudioSinkIsAudioFrameStorageTypeSupported-stub">

```
#include "starboard/audio_sink.h"

bool SbAudioSinkIsAudioFrameStorageTypeSupported(
    SbMediaAudioFrameStorageType audio_frame_storage_type) {
  return true;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMediaAudioFrameStorageType</code><br>
        <code>audio_frame_storage_type</code></td>
    <td> </td>
  </tr>
</table>

### SbAudioSinkIsAudioSampleTypeSupported

**Description**

Indicates whether `audio_sample_type` is supported on this platform.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAudioSinkIsAudioSampleTypeSupported-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAudioSinkIsAudioSampleTypeSupported-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAudioSinkIsAudioSampleTypeSupported-declaration">
<pre>
SB_EXPORT bool SbAudioSinkIsAudioSampleTypeSupported(
    SbMediaAudioSampleType audio_sample_type);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAudioSinkIsAudioSampleTypeSupported-stub">

```
#include "starboard/audio_sink.h"

bool SbAudioSinkIsAudioSampleTypeSupported(
    SbMediaAudioSampleType audio_sample_type) {
  return true;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbMediaAudioSampleType</code><br>
        <code>audio_sample_type</code></td>
    <td> </td>
  </tr>
</table>

### SbAudioSinkIsValid

**Description**

Indicates whether the given audio sink handle is valid.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbAudioSinkIsValid-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbAudioSinkIsValid-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbAudioSinkIsValid-declaration">
<pre>
SB_EXPORT bool SbAudioSinkIsValid(SbAudioSink audio_sink);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbAudioSinkIsValid-stub">

```
#include "starboard/audio_sink.h"

bool SbAudioSinkIsValid(SbAudioSink /*audio_sink*/) {
  return true;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbAudioSink</code><br>        <code>audio_sink</code></td>
    <td>The audio sink handle to check.</td>
  </tr>
</table>

