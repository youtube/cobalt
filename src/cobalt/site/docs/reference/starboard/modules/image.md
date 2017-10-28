---
layout: doc
title: "Starboard Module Reference: image.h"
---

API for hardware accelerated image decoding. This module allows for the
client to feed in raw, encoded data to be decoded directly into an
SbDecodeTarget.  It also provides an interface for the client to query what
combinations of encoded image formats and SbDecodeTargetFormats are
supported or not.<br>
All functions in this module are safe to call from any thread at any point
in time.<br>
#### SbImageIsDecodeSupported and SbImageDecode Example


Let's assume that we're on a Blitter platform.<br>
SbDecodeTargetProvider* provider = GetProviderFromSomewhere();
void* data = GetCompressedJPEGFromSomewhere();
int data_size = GetCompressedJPEGSizeFromSomewhere();
const char* mime_type = "image/jpeg";
SbDecodeTargetFormat format = kSbDecodeTargetFormat1PlaneRGBA;<br>
if (!SbImageIsDecodeSupported(mime_type, format)) {
return;
}<br>
SbDecodeTarget result_target = SbDecodeImage(provider, data, data_size,
mime_type, format);
SbBlitterSurface surface =
SbDecodeTargetGetPlane(target, kSbDecodeTargetPlaneRGBA);
// Do stuff with surface...<br>

## Functions

### SbImageDecode

**Description**

Attempt to decode encoded `mime_type` image data `data` of size `data_size`
into an SbDecodeTarget of SbDecodeFormatType `format`, possibly using
SbDecodeTargetProvider `provider`, if it is non-null.  Thus, four following
scenarios regarding the provider may happen:<br>
<ol><li>The provider is required by the `SbImageDecode` implementation and no
provider is given.  The implementation should gracefully fail by
immediately returning kSbDecodeTargetInvalid.
</li><li>The provider is required and is passed in.  The implementation will
proceed forward, using the SbDecodeTarget from the provider.
</li><li>The provider is not required and is passed in.  The provider will NOT be
called, and the implementation will proceed to decoding however it
desires.
</li><li>The provider is not required and is not passed in.  The implementation
will proceed forward.</li></ol>
Thus, it is NOT safe for clients of this API to assume that the `provider`
it passes in will be called.  Finally, if the decode succeeds, a new
SbDecodeTarget will be allocated. If `mime_type` image decoding for the
requested format is not supported or the decode fails,
kSbDecodeTargetInvalid will be returned, with any intermediate allocations
being cleaned up in the implementation.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbImageDecode-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbImageDecode-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbImageDecode-declaration">
<pre>
SB_EXPORT SbDecodeTarget SbImageDecode(
    SbDecodeTargetGraphicsContextProvider* context_provider,
    void* data,
    int data_size,
    const char* mime_type,
    SbDecodeTargetFormat format);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbImageDecode-stub">

```
#include "starboard/configuration.h"
#include "starboard/image.h"

#if !SB_HAS(GRAPHICS)
#error "SbImageDecode requires SB_HAS(GRAPHICS)."
#endif

SbDecodeTarget SbImageDecode(SbDecodeTargetGraphicsContextProvider* provider,
                             void* data,
                             int data_size,
                             const char* mime_type,
                             SbDecodeTargetFormat format) {
  SB_UNREFERENCED_PARAMETER(data);
  SB_UNREFERENCED_PARAMETER(data_size);
  SB_UNREFERENCED_PARAMETER(format);
  SB_UNREFERENCED_PARAMETER(mime_type);
  SB_UNREFERENCED_PARAMETER(provider);
  return kSbDecodeTargetInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbDecodeTargetGraphicsContextProvider*</code><br>
        <code>context_provider</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>void*</code><br>
        <code>data</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>int</code><br>
        <code>data_size</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>const char*</code><br>
        <code>mime_type</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDecodeTargetFormat</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
</table>

### SbImageIsDecodeSupported

**Description**

Whether the current platform supports hardware accelerated decoding an
image of mime type `mime_type` into SbDecodeTargetFormat `format`.  The
result of this function must not change over the course of the program,
which means that the results of this function may be cached indefinitely.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbImageIsDecodeSupported-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbImageIsDecodeSupported-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbImageIsDecodeSupported-declaration">
<pre>
SB_EXPORT bool SbImageIsDecodeSupported(const char* mime_type,
                                        SbDecodeTargetFormat format);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbImageIsDecodeSupported-stub">

```
#include "starboard/configuration.h"
#include "starboard/image.h"

#if !SB_HAS(GRAPHICS)
#error "Requires SB_HAS(GRAPHICS)."
#endif

bool SbImageIsDecodeSupported(const char* mime_type,
                              SbDecodeTargetFormat format) {
  SB_UNREFERENCED_PARAMETER(mime_type);
  SB_UNREFERENCED_PARAMETER(format);
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>
        <code>mime_type</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDecodeTargetFormat</code><br>
        <code>format</code></td>
    <td> </td>
  </tr>
</table>

