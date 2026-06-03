Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `audio_sink.h`

Provides an interface to output raw audio data.

## Macros

### kSbAudioSinkInvalid

Well-defined value for an invalid audio sink.

## Typedefs

### SbAudioSink

An opaque handle to an implementation-private structure representing an audio
sink.

#### Definition

```
typedef struct SbAudioSinkPrivate* SbAudioSink
```

### SbAudioSinkConsumeFramesFunc

Callback used to report consumed frames. Consumed frames are removed from the
source frame buffer to free space for new audio frames.

#### Definition

```
typedef void(* SbAudioSinkConsumeFramesFunc) (int frames_consumed, void *context)
```

### SbAudioSinkFrameBuffers

An array of frame buffers. For interleaved audio streams, there is only one
element in the array. For planar audio streams, the number of elements in the
array equals the number of channels.

#### Definition

```
typedef void** SbAudioSinkFrameBuffers
```

### SbAudioSinkUpdateSourceStatusFunc

A callback invoked periodically to retrieve the status of the audio source. The
first two output parameters indicate the fill level of the audio frame buffer
passed to `SbAudioSinkCreate` as `frame_buffers`. Because `frame_buffers` is a
circular buffer, use `offset_in_frames` to calculate the number of continuous
frames towards the end of the buffer. The audio sink can play the frames only
when `is_playing` is `true`. It should pause playback when `is_playing` is
`false`. The audio sink may cache a certain amount of audio frames before
starting playback. It starts playback immediately when `is_eos_reached` is
`true`, even if there are not enough cached audio frames, because no more frames
will be appended to the buffer. The host can set `is_eos_reached` to `false`
after setting it to `true` (typically due to a seek). All parameters except
`context` must not be `NULL`. This function only reports source status; it does
not remove audio data from the source frame buffer.

#### Definition

```
typedef void(* SbAudioSinkUpdateSourceStatusFunc) (int *frames_in_buffer, int *offset_in_frames, bool *is_playing, bool *is_eos_reached, void *context)
```

## Functions

### SbAudioSinkCreate

Creates an audio sink for the specified `channels` and `sampling_frequency_hz`,
acquires all resources needed to operate the audio sink, and returns an opaque
handle to the audio sink.

If the platform does not support the requested audio sink, the function returns
`kSbAudioSinkInvalid` without invoking any callbacks. If a platform limit on
coexisting audio sinks is exceeded, this function returns `kSbAudioSinkInvalid`.
Multiple calls to `SbAudioSinkCreate` must not cause a crash.

*   `channels`: The number of audio channels (for example, `2` for stereo).

*   `sampling_frequency_hz`: The sample frequency of the audio data being
    streamed. For example, 22,000 Hz means 22,000 sample elements represent one
    second of audio data.

*   `audio_sample_type`: The type of each sample of the audio data (`int16`,
    `float32`, etc.).

*   `audio_frame_storage_type`: Indicates whether frames are interleaved or
    planar.

*   `frame_buffers`: An array of pointers to sample data.

    *   If the sink operates in interleaved mode, the array contains only one
        element, which is an array containing (`frames_per_channel` *
        `channels`) samples.

    *   If the sink operates in planar mode, the number of elements in the array
        equals `channels`, and each element is an array of `frames_per_channel`
        samples. The caller must ensure that `frame_buffers` remains valid until
        `SbAudioSinkDestroy` is called.

*   `frames_per_channel`: The size of the frame buffers, in samples per channel.
    A frame represents a group of samples at the same media time, one for each
    channel.

*   `update_source_status_func`: A callback invoked by the audio sink on an
    internal thread to query the status of the source. It is called immediately
    during `SbAudioSinkCreate` (before it returns). The caller must ensure that
    the callback returns meaningful values. Must not be `NULL`.

*   `consume_frames_func`: A callback invoked by the audio sink on an internal
    thread to report consumed frames. Must not be `NULL`.

*   `context`: An opaque value passed to all callbacks, typically pointing to
    state associated with the audio sink.

#### Declaration

```
SbAudioSink SbAudioSinkCreate(int channels, int sampling_frequency_hz, SbMediaAudioSampleType audio_sample_type, SbMediaAudioFrameStorageType audio_frame_storage_type, SbAudioSinkFrameBuffers frame_buffers, int frames_per_channel, SbAudioSinkUpdateSourceStatusFunc update_source_status_func, SbAudioSinkConsumeFramesFunc consume_frames_func, void *context)
```

### SbAudioSinkDestroy

Destroys `audio_sink`, freeing all associated resources. Before returning, the
function waits until all active callbacks finish. After the function returns, no
further calls are made to callbacks passed to `SbAudioSinkCreate`. You cannot
pass `audio_sink` to other `SbAudioSink` functions after calling
`SbAudioSinkDestroy`.

This function can be called on any thread, but must not be called from within
any of the callbacks passed to `SbAudioSinkCreate`.

*   `audio_sink`: The audio sink to destroy.

#### Declaration

```
void SbAudioSinkDestroy(SbAudioSink audio_sink)
```

### SbAudioSinkGetMaxChannels

Returns the maximum number of channels supported on the platform. For example,
the number would be `2` if the platform only supports stereo.

#### Declaration

```
int SbAudioSinkGetMaxChannels()
```

### SbAudioSinkGetMinBufferSizeInFrames

Returns the minimum frames required by the audio sink to play without
underflows. Returns `-1` if `channels`, `sample_type`, or
`sampling_frequency_hz` is not supported. The caller must ensure that enough
frames are written to the audio sink during playback to prevent underflows.

*   `channels`: The number of audio channels, such as left and right channels in
    stereo audio.

*   `audio_sample_type`: The type of each sample of the audio data – `int16`,
    `float32`, etc.

*   `sampling_frequency_hz`: The sample frequency of the audio data being
    streamed. For example, 22,000 Hz means 22,000 sample elements represents one
    second of audio data.

#### Declaration

```
int SbAudioSinkGetMinBufferSizeInFrames(int channels, SbMediaAudioSampleType sample_type, int sampling_frequency_hz)
```

### SbAudioSinkGetNearestSupportedSampleFrequency

Returns the supported sample rate closest to `sampling_frequency_hz`. On
platforms that do not support all sample rates, the caller must resample the
audio frames to the supported sample rate returned by this function.

#### Declaration

```
int SbAudioSinkGetNearestSupportedSampleFrequency(int sampling_frequency_hz)
```

### SbAudioSinkIsAudioFrameStorageTypeSupported

Returns whether `audio_frame_storage_type` is supported on this platform.

#### Declaration

```
bool SbAudioSinkIsAudioFrameStorageTypeSupported(SbMediaAudioFrameStorageType audio_frame_storage_type)
```

### SbAudioSinkIsAudioSampleTypeSupported

Returns whether `audio_sample_type` is supported on this platform.

#### Declaration

```
bool SbAudioSinkIsAudioSampleTypeSupported(SbMediaAudioSampleType audio_sample_type)
```

### SbAudioSinkIsValid

Returns whether the audio sink handle is valid.

*   `audio_sink`: The audio sink handle to check.

#### Declaration

```
bool SbAudioSinkIsValid(SbAudioSink audio_sink)
```
