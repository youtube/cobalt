Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `microphone.h`

Defines functions for microphone creation, control, audio data fetching, and
destruction. The implementation must handle multiple calls to `SbMicrophoneOpen`
and `SbMicrophoneClose` on the same microphone. For example, calling
`SbMicrophoneOpen` twice without an intervening `SbMicrophoneClose` must be
handled gracefully.

This API is not thread-safe and must be called from a single thread.

API Usage:

1.  Query available microphones by calling `SbMicrophoneGetAvailable`.

1.  Create a supported microphone using `SbMicrophoneCreate`, specifying a
    supported sample rate and buffer size. Verify the sample rate with
    `SbMicrophoneIsSampleRateSupported`.

1.  Open the microphone port and start recording using `SbMicrophoneOpen`.

1.  Periodically read audio data using `SbMicrophoneRead`.

1.  Close the port and stop recording using `SbMicrophoneClose`.

1.  Destroy the microphone with `SbMicrophoneDestroy`.

## Macros

### kSbMicrophoneIdInvalid

Well-defined value for an invalid microphone ID handle.

### kSbMicrophoneInvalid

Well-defined value for an invalid microphone handle.

## Enums

### SbMicrophoneType

All possible microphone types.

#### Values

*   `kSbMicrophoneCamera`

    Built-in camera microphone.
*   `kSbMicrophoneUSBHeadset`

    USB headset microphone (wired or wireless).
*   `kSbMicrophoneVRHeadset`

    VR headset microphone.
*   `kSBMicrophoneAnalogHeadset`

    Analog headset microphone.
*   `kSbMicrophoneUnknown`

    Unknown microphone type. Used if the microphone does not map to other enum
    values.

## Typedefs

### SbMicrophone

An opaque handle to an implementation-private structure that represents a
microphone.

#### Definition

```
typedef struct SbMicrophonePrivate* SbMicrophone
```

### SbMicrophoneId

An opaque handle to an implementation-private structure that represents a
microphone ID.

#### Definition

```
typedef struct SbMicrophoneIdPrivate* SbMicrophoneId
```

## Structs

### SbMicrophoneInfo

Microphone information.

#### Members

*   `SbMicrophoneId id`

    Microphone ID.
*   `SbMicrophoneType type`

    Microphone type.
*   `int max_sample_rate_hz`

    The microphone's maximum supported sampling rate.
*   `int min_read_size`

    The minimum read size (in bytes) required for each read.
*   `char label`

    A user-friendly name for the microphone (for example, "Headset Microphone").
    Can be empty. The string must be null-terminated.

## Functions

### SbMicrophoneClose

Closes the microphone port, stops recording on `microphone`, and clears any
unread data from the buffer. If the microphone is already stopped, this call is
ignored. Returns `true` if the microphone is successfully closed.

*   `microphone`: The microphone to close.

#### Declaration

```
bool SbMicrophoneClose(SbMicrophone microphone)
```

### SbMicrophoneCreate

Creates a microphone with the specified ID, sample rate, and buffer size.
Starboard only requires support for one active microphone at a time; creating a
second microphone before destroying the first may fail.

Returns the newly created `SbMicrophone` object. Returns `kSbMicrophoneInvalid`
if the microphone is already initialized, the sample rate is unsupported, or the
buffer size is invalid.

*   `id`: The ID that will be assigned to the newly created SbMicrophone.

*   `sample_rate_in_hz`: The new microphone's audio sample rate in Hz.

*   `buffer_size_bytes`: The size of the buffer where signed 16-bit integer
    audio data is temporarily cached during capture. Audio data is removed from
    the buffer after it is read. New data can be read from this buffer in chunks
    smaller than the buffer size. This parameter must be greater than zero, and
    ideally a power of two (`2^n`).

#### Declaration

```
SbMicrophone SbMicrophoneCreate(SbMicrophoneId id, int sample_rate_in_hz, int buffer_size_bytes)
```

### SbMicrophoneDestroy

Destroys a microphone. If the microphone is recording, it is stopped before
being destroyed. Any unread recorded data is discarded.

#### Declaration

```
void SbMicrophoneDestroy(SbMicrophone microphone)
```

### SbMicrophoneGetAvailable

Retrieves information for all available microphones and stores it in
`out_info_array`. Returns the number of available microphones. If this count
exceeds `info_array_size`, the array is filled to capacity with available
microphone info, and the total count of available microphones is returned. A
negative return value indicates an error.

*   `out_info_array`: The destination buffer for available microphone info.

*   `info_array_size`: The capacity of `out_info_array`.

#### Declaration

```
int SbMicrophoneGetAvailable(SbMicrophoneInfo *out_info_array, int info_array_size)
```

### SbMicrophoneIdIsValid

Indicates whether the given microphone ID is valid.

#### Declaration

```
static bool SbMicrophoneIdIsValid(SbMicrophoneId id)
```

### SbMicrophoneIsSampleRateSupported

Returns whether the microphone supports the specified sample rate.

#### Declaration

```
bool SbMicrophoneIsSampleRateSupported(SbMicrophoneId id, int sample_rate_in_hz)
```

### SbMicrophoneIsValid

Indicates whether the given microphone is valid.

#### Declaration

```
static bool SbMicrophoneIsValid(SbMicrophone microphone)
```

### SbMicrophoneOpen

Opens the microphone port and starts recording on `microphone`. Once started,
call `SbMicrophoneRead` periodically to retrieve audio data. If the microphone
is already open, this call clears the unread buffer. Returns `true` if the
microphone is successfully opened.

*   `microphone`: The microphone to open.

#### Declaration

```
bool SbMicrophoneOpen(SbMicrophone microphone)
```

### SbMicrophoneRead

Retrieves the recorded audio data from the microphone and writes that data to
`out_audio_data`.

Returns the number of bytes read (greater than or equal to zero). The returned
size does not exceed `audio_data_size` or the internal buffer size. Returns a
negative value on error.

Call this function frequently. If the internal buffer (configured in
`SbMicrophoneCreate`) fills up, new audio data is discarded. You cannot read
data from a stopped microphone.

*   `microphone`: The microphone to read from.

*   `out_audio_data`: The destination buffer for the read data.

*   `audio_data_size`: The number of bytes to read. If `audio_data_size` is less
    than `min_read_size` (from `SbMicrophoneInfo`), any additional audio data
    read from the device is discarded.

#### Declaration

```
int SbMicrophoneRead(SbMicrophone microphone, void *out_audio_data, int audio_data_size)
```
