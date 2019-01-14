---
layout: doc
title: "Starboard Module Reference: image.h"
---

API for hardware accelerated image decoding. This module allows for the client
to feed in raw, encoded data to be decoded directly into an SbDecodeTarget. It
also provides an interface for the client to query what combinations of encoded
image formats and SbDecodeTargetFormats are supported or not.

All functions in this module are safe to call from any thread at any point in
time.

## SbImageIsDecodeSupported and SbImageDecode Example ##

Let's assume that we're on a Blitter platform.

```
SbDecodeTargetProvider* provider = GetProviderFromSomewhere();
void* data = GetCompressedJPEGFromSomewhere();
int data_size = GetCompressedJPEGSizeFromSomewhere();
const char* mime_type = "image/jpeg";
SbDecodeTargetFormat format = kSbDecodeTargetFormat1PlaneRGBA;

if (!SbImageIsDecodeSupported(mime_type, format)) {
  return;
}

SbDecodeTarget result_target = SbDecodeImage(provider, data, data_size,
                                             mime_type, format);
SbBlitterSurface surface =
    SbDecodeTargetGetPlane(target, kSbDecodeTargetPlaneRGBA);
// Do stuff with surface...
```

## Functions ##

### SbImageDecode ###

Attempt to decode encoded `mime_type` image data `data` of size `data_size` into
an SbDecodeTarget of SbDecodeFormatType `format`, possibly using
SbDecodeTargetProvider `provider`, if it is non-null. Thus, four following
scenarios regarding the provider may happen:

1.  The provider is required by the `SbImageDecode` implementation and no
    provider is given. The implementation should gracefully fail by immediately
    returning kSbDecodeTargetInvalid.

1.  The provider is required and is passed in. The implementation will proceed
    forward, using the SbDecodeTarget from the provider.

1.  The provider is not required and is passed in. The provider will NOT be
    called, and the implementation will proceed to decoding however it desires.

1.  The provider is not required and is not passed in. The implementation will
    proceed forward.

Thus, it is NOT safe for clients of this API to assume that the `provider` it
passes in will be called. Finally, if the decode succeeds, a new SbDecodeTarget
will be allocated. If `mime_type` image decoding for the requested format is not
supported or the decode fails, kSbDecodeTargetInvalid will be returned, with any
intermediate allocations being cleaned up in the implementation.

#### Declaration ####

```
SbDecodeTarget SbImageDecode(SbDecodeTargetGraphicsContextProvider *context_provider, void *data, int data_size, const char *mime_type, SbDecodeTargetFormat format)
```

### SbImageIsDecodeSupported ###

Whether the current platform supports hardware accelerated decoding an image of
mime type `mime_type` into SbDecodeTargetFormat `format`. The result of this
function must not change over the course of the program, which means that the
results of this function may be cached indefinitely.

#### Declaration ####

```
bool SbImageIsDecodeSupported(const char *mime_type, SbDecodeTargetFormat format)
```

