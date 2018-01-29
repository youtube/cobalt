---
layout: doc
title: "Starboard Module Reference: microphone.h"
---

Defines functions for microphone creation, control, audio data fetching, and
destruction. This module supports multiple calls to `SbMicrophoneOpen` and
`SbMicrophoneClose`, and the implementation should handle multiple calls to one
of those functions on the same microphone. For example, your implementation
should handle cases where `SbMicrophoneOpen` is called twice on the same
microphone without a call to `SbMicrophoneClose` in between.

This API is not thread-safe and must be called from a single thread.

How to use this API:

1.  Call `SbMicrophoneGetAvailableInfos` to get a list of available microphone
    information.

1.  Create a supported microphone, using `SbMicrophoneCreate`, with enough
    buffer size and sample rate. Use `SbMicrophoneIsSampleRateSupported` to
    verify the sample rate.

1.  Use `SbMicrophoneOpen` to open the microphone port and start recording audio
    data.

1.  Periodically read out the data from microphone with `SbMicrophoneRead`.

1.  Call `SbMicrophoneClose` to close the microphone port and stop recording
    audio data.

1.  Destroy the microphone with `SbMicrophoneDestroy`.

## Macros ##

### kSbMicrophoneIdInvalid ###

Well-defined value for an invalid microphone ID handle.

### kSbMicrophoneInvalid ###

Well-defined value for an invalid microphone handle.

## Enums ##

### SbMicrophoneType ###

All possible microphone types.

#### Values ####

*   `kSbMicrophoneCamera`

    Built-in microphone in camera.
*   `kSbMicrophoneUSBHeadset`

    Microphone in the headset that can be a wired or wireless USB headset.
*   `kSbMicrophoneVRHeadset`

    Microphone in the VR headset.
*   `kSBMicrophoneAnalogHeadset`

    Microphone in the analog headset.
*   `kSbMicrophoneUnknown`

    Unknown microphone type. The microphone could be different than the other
    enum descriptions or could fall under one of those descriptions.

## Typedefs ##

### SbMicrophone ###

An opaque handle to an implementation-private structure that represents a
microphone.

#### Definition ####

```
typedef struct SbMicrophonePrivate* SbMicrophone
```

### SbMicrophoneId ###

An opaque handle to an implementation-private structure that represents a
microphone ID.

#### Definition ####

```
typedef struct SbMicrophoneIdPrivate* SbMicrophoneId
```

## Structs ##

### SbMicrophoneInfo ###

Microphone information.

#### Members ####

*   `SbMicrophoneId id`

    Microphone id.
*   `SbMicrophoneType type`

    Microphone type.
*   `int max_sample_rate_hz`

    The microphone's maximum supported sampling rate.
*   `int min_read_size`

    The minimum read size required for each read from microphone.

## Functions ##

### SbMicrophoneClose ###

Closes the microphone port, stops recording audio on `microphone`, and clears
the unread buffer if it is not empty. If the microphone has already been
stopped, this call is ignored. The return value indicates whether the microphone
is closed.

`microphone`: The microphone to close.

#### Declaration ####

```
bool SbMicrophoneClose(SbMicrophone microphone)
```

### SbMicrophoneCreate ###

Creates a microphone with the specified ID, audio sample rate, and cached audio
buffer size. Starboard only requires support for creating one microphone at a
time, and implementations may return an error if a second microphone is created
before the first is destroyed.

The function returns the newly created SbMicrophone object. However, if you try
to create a microphone that has already been initialized, if the sample rate is
unavailable, or if the buffer size is invalid, the function should return
`kSbMicrophoneInvalid`.

`id`: The ID that will be assigned to the newly created SbMicrophone.
`sample_rate_in_hz`: The new microphone's audio sample rate in Hz.
`buffer_size_bytes`: The size of the buffer where signed 16-bit integer audio
data is temporarily cached to during the capturing. The audio data is removed
from the audio buffer if it has been read, and new audio data can be read from
this buffer in smaller chunks than this size. This parameter must be set to a
value greater than zero and the ideal size is `2^n`.

#### Declaration ####

```
SbMicrophone SbMicrophoneCreate(SbMicrophoneId id, int sample_rate_in_hz, int buffer_size_bytes)
```

### SbMicrophoneDestroy ###

Destroys a microphone. If the microphone is in started state, it is first
stopped and then destroyed. Any data that has been recorded and not read is
thrown away.

#### Declaration ####

```
void SbMicrophoneDestroy(SbMicrophone microphone)
```

### SbMicrophoneGetAvailable ###

Retrieves all currently available microphone information and stores it in
`out_info_array`. The return value is the number of the available microphones.
If the number of available microphones is larger than `info_array_size`, then
`out_info_array` is filled up with as many available microphones as possible and
the actual number of available microphones is returned. A negative return value
indicates that an internal error occurred.

`out_info_array`: All currently available information about the microphone is
placed into this output parameter. `info_array_size`: The size of
`out_info_array`.

#### Declaration ####

```
int SbMicrophoneGetAvailable(SbMicrophoneInfo *out_info_array, int info_array_size)
```

### SbMicrophoneIdIsValid ###

Indicates whether the given microphone ID is valid.

#### Declaration ####

```
static bool SbMicrophoneIdIsValid(SbMicrophoneId id)
```

### SbMicrophoneIsSampleRateSupported ###

Indicates whether the microphone supports the sample rate.

#### Declaration ####

```
bool SbMicrophoneIsSampleRateSupported(SbMicrophoneId id, int sample_rate_in_hz)
```

### SbMicrophoneIsValid ###

Indicates whether the given microphone is valid.

#### Declaration ####

```
static bool SbMicrophoneIsValid(SbMicrophone microphone)
```

### SbMicrophoneOpen ###

Opens the microphone port and starts recording audio on `microphone`.

Once started, the client needs to periodically call `SbMicrophoneRead` to
receive the audio data. If the microphone has already been started, this call
clears the unread buffer. The return value indicates whether the microphone is
open. `microphone`: The microphone that will be opened and will start recording
audio.

#### Declaration ####

```
bool SbMicrophoneOpen(SbMicrophone microphone)
```

### SbMicrophoneRead ###

Retrieves the recorded audio data from the microphone and writes that data to
`out_audio_data`.

The return value is zero or the positive number of bytes that were read. Neither
the return value nor `audio_data_size` exceeds the buffer size. A negative
return value indicates that an error occurred.

This function should be called frequently. Otherwise, the microphone only
buffers `buffer_size` bytes as configured in `SbMicrophoneCreate` and the new
audio data is thrown out. No audio data is read from a stopped microphone.

`microphone`: The microphone from which to retrieve recorded audio data.
`out_audio_data`: The buffer to which the retrieved data will be written.
`audio_data_size`: The number of requested bytes. If `audio_data_size` is
smaller than `min_read_size` of `SbMicrophoneInfo`, the extra audio data that
has already been read from the device is discarded.

#### Declaration ####

```
int SbMicrophoneRead(SbMicrophone microphone, void *out_audio_data, int audio_data_size)
```

