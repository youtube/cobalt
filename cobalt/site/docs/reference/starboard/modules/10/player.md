---
layout: doc
title: "Starboard Module Reference: player.h"
---

Defines an interface for controlling playback of media elementary streams.

## Macros ##

### SB_PLAYER_INITIAL_TICKET ###

The value of the initial ticket held by the player before the first seek. The
player will use this ticket value to make the first call to SbPlayerStatusFunc
with kSbPlayerStateInitialized.

### SB_PLAYER_NO_DURATION ###

The value to pass into SbPlayerCreate's `duration_pts` argument for cases where
the duration is unknown, such as for live streams.

### kSbPlayerInvalid ###

Well-defined value for an invalid player.

## Enums ##

### SbPlayerDecoderState ###

An indicator of whether the decoder can accept more samples.

#### Values ####

*   `kSbPlayerDecoderStateNeedsData`

    The decoder is asking for one more sample.
*   `kSbPlayerDecoderStateBufferFull`

    The decoder is not ready for any more samples, so do not send them. Note
    that this enum value has been deprecated and the SbPlayer implementation
    should no longer use this value.
*   `kSbPlayerDecoderStateDestroyed`

    The player has been destroyed, and will send no more callbacks.

### SbPlayerState ###

An indicator of the general playback state.

#### Values ####

*   `kSbPlayerStateInitialized`

    The player has just been initialized. It is expecting an SbPlayerSeek() call
    to enter the prerolling state.
*   `kSbPlayerStatePrerolling`

    The player is prerolling, collecting enough data to fill the pipeline before
    presentation starts. After the first preroll is completed, there should
    always be a video frame to render, even if the player goes back to
    Prerolling after a Seek.
*   `kSbPlayerStatePresenting`

    The player is presenting media, and it is either paused or actively playing
    in real-time. Note that the implementation should use this state to signal
    that the preroll has been finished.
*   `kSbPlayerStateEndOfStream`

    The player is presenting media, but it is paused at the end of the stream.
*   `kSbPlayerStateDestroyed`

    The player has been destroyed, and will send no more callbacks.

## Typedefs ##

### SbPlayer ###

An opaque handle to an implementation-private structure representing a player.

#### Definition ####

```
typedef struct SbPlayerPrivate* SbPlayer
```

### SbPlayerDeallocateSampleFunc ###

Callback to free the given sample buffer data. When more than one buffer are
sent in SbPlayerWriteSample(), the implementation only has to call this callback
with `sample_buffer` points to the the first buffer.

#### Definition ####

```
typedef void(* SbPlayerDeallocateSampleFunc) (SbPlayer player, void *context, const void *sample_buffer)
```

### SbPlayerDecoderStatusFunc ###

Callback for decoder status updates, called in response to a call to
SbPlayerSeek() or SbPlayerWriteSample(). This callback will never be called
until at least one call to SbPlayerSeek has occurred. `ticket` will be set to
the ticket passed into the last received call to SbPlayerSeek() at the time this
callback was dispatched. This is to distinguish status callbacks for
interrupting seeks. These callbacks will happen on a different thread than the
calling thread, and it is not valid to call SbPlayer functions from within this
callback. After an update with kSbPlayerDecoderStateNeedsData, the user of the
player will make at most one call to SbPlayerWriteSample() or
SbPlayerWriteEndOfStream(). The player implementation should update the decoder
status again after such call to notify its user to continue writing more frames.

#### Definition ####

```
typedef void(* SbPlayerDecoderStatusFunc) (SbPlayer player, void *context, SbMediaType type, SbPlayerDecoderState state, int ticket)
```

### SbPlayerErrorFunc ###

Callback for player errors, that may set a `message`. `error`: indicates the
error code. `message`: provides specific informative diagnostic message about
the error condition encountered. It is ok for the message to be an empty string
or NULL if no information is available.

#### Definition ####

```
typedef void(* SbPlayerErrorFunc) (SbPlayer player, void *context, SbPlayerError error, const char *message)
```

### SbPlayerStatusFunc ###

Callback for player status updates. These callbacks will happen on a different
thread than the calling thread, and it is not valid to call SbPlayer functions
from within this callback.

#### Definition ####

```
typedef void(* SbPlayerStatusFunc) (SbPlayer player, void *context, SbPlayerState state, int ticket)
```

## Structs ##

### SbPlayerCreationParam ###

The playback related parameters to pass into SbPlayerCreate() and
SbPlayerGetPreferredOutputMode().

#### Members ####

*   `SbDrmSystem drm_system`

    Provides an appropriate DRM system if the media stream has encrypted
    portions. It will be `kSbDrmSystemInvalid` if the stream does not have
    encrypted portions.
*   `SbMediaAudioSampleInfo audio_sample_info`

    Contains a populated SbMediaAudioSampleInfo if `audio_sample_info.codec`
    isn't `kSbMediaAudioCodecNone`. When `audio_sample_info.codec` is
    `kSbMediaAudioCodecNone`, the video doesn't have an audio track.
*   `SbMediaVideoSampleInfo video_sample_info`

    Contains a populated SbMediaVideoSampleInfo if `video_sample_info.codec`
    isn't `kSbMediaVideoCodecNone`. When `video_sample_info.codec` is
    `kSbMediaVideoCodecNone`, the video is audio only.
*   `SbPlayerOutputMode output_mode`

    Selects how the decoded video frames will be output. For example,
    `kSbPlayerOutputModePunchOut` indicates that the decoded video frames will
    be output to a background video layer by the platform, and
    `kSbPlayerOutputDecodeToTexture` indicates that the decoded video frames
    should be made available for the application to pull via calls to
    SbPlayerGetCurrentFrame().

### SbPlayerInfo2 ###

Information about the current media playback state.

#### Members ####

*   `SbTime current_media_timestamp`

    The position of the playback head, as precisely as possible, in
    microseconds.
*   `SbTime duration`

    The known duration of the currently playing media stream, in microseconds.
*   `SbTime start_date`

    The result of getStartDate for the currently playing media stream, in
    microseconds since the epoch of January 1, 1601 UTC.
*   `int frame_width`

    The width of the currently displayed frame, in pixels, or 0 if not provided
    by this player.
*   `int frame_height`

    The height of the currently displayed frame, in pixels, or 0 if not provided
    by this player.
*   `bool is_paused`

    Whether playback is currently paused.
*   `double volume`

    The current player volume in [0, 1].
*   `int total_video_frames`

    The number of video frames sent to the player since the creation of the
    player.
*   `int dropped_video_frames`

    The number of video frames decoded but not displayed since the creation of
    the player.
*   `int corrupted_video_frames`

    The number of video frames that failed to be decoded since the creation of
    the player.
*   `double playback_rate`

    The rate of playback. The video is played back in a speed that is
    proportional to this. By default it is 1.0 which indicates that the playback
    is at normal speed. When it is greater than one, the video is played in a
    faster than normal speed. When it is less than one, the video is played in a
    slower than normal speed. Negative speeds are not supported.

### SbPlayerSampleInfo ###

Information about the samples to be written into SbPlayerWriteSample2.

#### Members ####

*   `const void * buffer`

    Points to the buffer containing the sample data.
*   `int buffer_size`

    Size of the data pointed to by `buffer`.
*   `SbTime timestamp`

    The timestamp of the sample in SbTime.
*   `constSbMediaVideoSampleInfo* video_sample_info`

    Information about a video sample. This value is required for video samples.
    Otherwise, it must be `NULL`.
*   `constSbDrmSampleInfo* drm_info`

    The DRM system related info for the media sample. This value is required for
    encrypted samples. Otherwise, it must be `NULL`.

## Functions ##

### SbPlayerCreate ###

Creates a player that will be displayed on `window` for the specified
`video_codec` and `audio_codec`, acquiring all resources needed to operate it,
and returning an opaque handle to it. The expectation is that a new player will
be created and destroyed for every playback.

This function returns the created player. Note the following:

*   The associated decoder of the returned player should be assumed to not be in
    `kSbPlayerDecoderStateNeedsData` until SbPlayerSeek() has been called on it.

*   It is expected either that the thread that calls SbPlayerCreate is the same
    thread that calls the other `SbPlayer` functions for that player, or that
    there is a mutex guarding calls into each `SbPlayer` instance.

*   If there is a platform limitation on how many players can coexist
    simultaneously, then calls made to this function that attempt to exceed that
    limit must return `kSbPlayerInvalid`. Multiple calls to SbPlayerCreate must
    not cause a crash.

`window`: The window that will display the player. `window` can be
`kSbWindowInvalid` for platforms where video is only displayed on a particular
window that the underlying implementation already has access to.

`video_codec`: The video codec used for the player. If `video_codec` is
`kSbMediaVideoCodecNone`, the player is an audio-only player. If `video_codec`
is any other value, the player is an audio/video decoder. This can be set to
`kSbMediaVideoCodecNone` to play a video with only an audio track.

`audio_codec`: The audio codec used for the player. The caller must provide a
populated `audio_sample_info` if audio codec is `kSbMediaAudioCodecAac`. Can be
set to `kSbMediaAudioCodecNone` to play a video without any audio track. In such
case `audio_sample_info` must be NULL.

`drm_system`: If the media stream has encrypted portions, then this parameter
provides an appropriate DRM system, created with `SbDrmCreateSystem()`. If the
stream does not have encrypted portions, then `drm_system` may be
`kSbDrmSystemInvalid`. `audio_header`: `audio_header` is same as
`audio_sample_info` in old starboard version. When `audio_codec` is
`kSbMediaAudioCodecNone`, this must be set to NULL. Note that
`audio_specific_config` is a pointer and the content it points to is no longer
valid after this function returns. The implementation has to make a copy of the
content if it is needed after the function returns.

#### Declaration ####

```
SbPlayer SbPlayerCreate(SbWindow window, const SbPlayerCreationParam *creation_param, SbPlayerDeallocateSampleFunc sample_deallocate_func, SbPlayerDecoderStatusFunc decoder_status_func, SbPlayerStatusFunc player_status_func, SbPlayerErrorFunc player_error_func, void *context, SbDecodeTargetGraphicsContextProvider *context_provider)
```

### SbPlayerDestroy ###

Destroys `player`, freeing all associated resources.

*   Upon calling this method, there should be one call to the player status
    callback (i.e. `player_status_func` used in the creation of the player)
    which indicates the player is destroyed. Note, the callback has to be in-
    flight when SbPlayerDestroyed is called.

*   No more other callbacks should be issued after this function returns.

*   It is not allowed to pass `player` into any other `SbPlayer` function once
    SbPlayerDestroy has been called on that player. `player`: The player to be
    destroyed.

#### Declaration ####

```
void SbPlayerDestroy(SbPlayer player)
```

### SbPlayerGetCurrentFrame ###

Given a player created with the kSbPlayerOutputModeDecodeToTexture output mode,
it will return a SbDecodeTarget representing the current frame to be rasterized.
On GLES systems, this function must be called on a thread with an EGLContext
current, and specifically the EGLContext that will be used to eventually render
the frame. If this function is called with a `player` object that was created
with an output mode other than kSbPlayerOutputModeDecodeToTexture,
kSbDecodeTargetInvalid is returned.

#### Declaration ####

```
SbDecodeTarget SbPlayerGetCurrentFrame(SbPlayer player)
```

### SbPlayerGetInfo2 ###

Gets a snapshot of the current player state and writes it to `out_player_info`.
This function may be called very frequently and is expected to be inexpensive.

`player`: The player about which information is being retrieved.
`out_player_info`: The information retrieved for the player.

#### Declaration ####

```
void SbPlayerGetInfo2(SbPlayer player, SbPlayerInfo2 *out_player_info2)
```

### SbPlayerGetMaximumNumberOfSamplesPerWrite ###

Writes a single sample of the given media type to `player`'s input stream. Its
data may be passed in via more than one buffers. The lifetime of
`sample_buffers`, `sample_buffer_sizes`, `video_sample_info`, and
`sample_drm_info` (as well as member `subsample_mapping` contained inside it)
are not guaranteed past the call to SbPlayerWriteSample. That means that before
returning, the implementation must synchronously copy any information it wants
to retain from those structures.

`player`: The player for which the number is retrieved. `sample_type`: The type
of sample for which the number is retrieved. See the `SbMediaType` enum in
media.h.

#### Declaration ####

```
int SbPlayerGetMaximumNumberOfSamplesPerWrite(SbPlayer player, SbMediaType sample_type)
```

### SbPlayerGetPreferredOutputMode ###

Returns the preferred output mode of the implementation when a video described
by `creation_param` is played. It is assumed that it is okay to call
SbPlayerCreate() with the same video described by `creation_param`, with its
`output_mode` member replaced by the returned output mode. When the caller has
no preference on the output mode, it will set `creation_param->output_mode` to
`kSbPlayerOutputModeInvalid`, and the implementation can return its preferred
output mode based on the information contained in `creation_param`. The caller
can also set `creation_param->output_mode` to its preferred output mode, and the
implementation should return the same output mode if it is supported, otherwise
the implementation should return an output mode that it is supported, as if
`creation_param->output_mode` is set to `kSbPlayerOutputModeInvalid` prior to
the call. Note that it is not the responsibility of this function to verify
whether the video described by `creation_param` can be played on the platform,
and the implementation should try its best effort to return a valid output mode.
`creation_param` will never be NULL.

#### Declaration ####

```
SbPlayerOutputMode SbPlayerGetPreferredOutputMode(const SbPlayerCreationParam *creation_param)
```

### SbPlayerIsValid ###

Returns whether the given player handle is valid.

#### Declaration ####

```
static bool SbPlayerIsValid(SbPlayer player)
```

### SbPlayerSetBounds ###

Sets the player bounds to the given graphics plane coordinates. The changes do
not take effect until the next graphics frame buffer swap. The default bounds
for a player is the full screen. This function is only relevant when the
`player` is created with the kSbPlayerOutputModePunchOut output mode, and if
this is not the case then this function call can be ignored.

This function is called on every graphics frame that changes the video bounds.
For example, if the video bounds are being animated, then this will be called at
up to 60 Hz. Since the function could be called up to once per frame,
implementors should take care to avoid related performance concerns with such
frequent calls.

`player`: The player that is being resized. `z_index`: The z-index of the
player. When the bounds of multiple players are overlapped, the one with larger
z-index will be rendered on top of the ones with smaller z-index. `x`: The
x-coordinate of the upper-left corner of the player. `y`: The y-coordinate of
the upper-left corner of the player. `width`: The width of the player, in
pixels. `height`: The height of the player, in pixels.

#### Declaration ####

```
void SbPlayerSetBounds(SbPlayer player, int z_index, int x, int y, int width, int height)
```

### SbPlayerSetPlaybackRate ###

Set the playback rate of the `player`. `rate` is default to 1.0 which indicates
the playback is at its original speed. A `rate` greater than one will make the
playback faster than its original speed. For example, when `rate` is 2, the
video will be played at twice the speed as its original speed. A `rate` less
than 1.0 will make the playback slower than its original speed. When `rate` is
0, the playback will be paused. The function returns true when the playback rate
is set to `playback_rate` or to a rate that is close to `playback_rate` which
the implementation supports. It returns false when the playback rate is
unchanged, this can happen when `playback_rate` is negative or if it is too high
to support.

#### Declaration ####

```
bool SbPlayerSetPlaybackRate(SbPlayer player, double playback_rate)
```

### SbPlayerSetVolume ###

Sets the player's volume.

`player`: The player in which the volume is being adjusted. `volume`: The new
player volume. The value must be between `0.0` and `1.0`, inclusive. A value of
`0.0` means that the audio should be muted, and a value of `1.0` means that it
should be played at full volume.

#### Declaration ####

```
void SbPlayerSetVolume(SbPlayer player, double volume)
```

### SbPlayerWriteEndOfStream ###

Writes a marker to `player`'s input stream of `stream_type` indicating that
there are no more samples for that media type for the remainder of this media
stream. This marker is invalidated, along with the rest of the stream's
contents, after a call to SbPlayerSeek.

`player`: The player to which the marker is written. `stream_type`: The type of
stream for which the marker is written.

#### Declaration ####

```
void SbPlayerWriteEndOfStream(SbPlayer player, SbMediaType stream_type)
```

### SbPlayerWriteSample2 ###

Writes samples of the given media type to `player`'s input stream. The lifetime
of `sample_infos`, and the members of its elements like `buffer`,
`video_sample_info`, and `drm_info` (as well as member `subsample_mapping`
contained inside it) are not guaranteed past the call to SbPlayerWriteSample2.
That means that before returning, the implementation must synchronously copy any
information it wants to retain from those structures.

SbPlayerWriteSample2 allows writing of multiple samples in one call.

`player`: The player to which the sample is written. `sample_type`: The type of
sample being written. See the `SbMediaType` enum in media.h. `sample_infos`: A
pointer to an array of SbPlayerSampleInfo with `number_of_sample_infos`
elements, each holds the data for an sample, i.e. a sequence of whole NAL Units
for video, or a complete audio frame. `sample_infos` cannot be assumed to live
past the call into SbPlayerWriteSample2(), so it must be copied if its content
will be used after SbPlayerWriteSample2() returns. `number_of_sample_infos`:
Specify the number of samples contained inside `sample_infos`. It has to be at
least one, and less than the return value of
SbPlayerGetMaximumNumberOfSamplesPerWrite().

#### Declaration ####

```
void SbPlayerWriteSample2(SbPlayer player, SbMediaType sample_type, const SbPlayerSampleInfo *sample_infos, int number_of_sample_infos)
```

