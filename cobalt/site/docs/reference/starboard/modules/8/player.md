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

The value to pass into SbPlayerCreate's `duration_ptr` argument for cases where
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
*   `kSbPlayerStateError`

    The player encountered an error. It expects an SbPlayerDestroy() call to
    tear down the player. Calls to other functions may be ignored and callbacks
    may not be triggered.

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
typedef void(* SbPlayerDeallocateSampleFunc)(SbPlayer player, void *context, const void *sample_buffer)
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
typedef void(* SbPlayerDecoderStatusFunc)(SbPlayer player, void *context, SbMediaType type, SbPlayerDecoderState state, int ticket)
```

### SbPlayerEncryptedMediaInitDataEncounteredCB ###

Callback to queue an encrypted event for initialization data encountered in
media data. `init_data_type` should be a string matching one of the EME
initialization data types : "cenc", "fairplay", "keyids", or "webm", `init_data`
is the initialization data, and `init_data_length` is the length of the data.

#### Definition ####

```
typedef void(* SbPlayerEncryptedMediaInitDataEncounteredCB)(SbPlayer player, void *context, const char *init_data_type, const unsigned char *init_data, unsigned int init_data_length)
```

### SbPlayerStatusFunc ###

Callback for player status updates. These callbacks will happen on a different
thread than the calling thread, and it is not valid to call SbPlayer functions
from within this callback.

#### Definition ####

```
typedef void(* SbPlayerStatusFunc)(SbPlayer player, void *context, SbPlayerState state, int ticket)
```

## Structs ##

### SbPlayerInfo ###

Information about the current media playback state.

#### Members ####

*   `SbMediaTime current_media_pts`

    The position of the playback head, as precisely as possible, in 90KHz ticks
    (PTS).
*   `SbMediaTime duration_pts`

    The known duration of the currently playing media stream, in 90KHz ticks
    (PTS).
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
*   `SbMediaTime buffer_start_pts`

    The position of the buffer head, as precisely as possible, in 90KHz ticks
    (PTS).
*   `SbMediaTime buffer_duration_pts`

    The known duration of the currently playing media buffer, in 90KHz ticks
    (PTS).

## Functions ##

### SbPlayerCreateWithUrl ###

Creates a URL-based SbPlayer that will be displayed on `window` for the
specified URL `url`, acquiring all resources needed to operate it, and returning
an opaque handle to it. The expectation is that a new player will be created and
destroyed for every playback.

In many ways this function is similar to SbPlayerCreate, but it is missing the
input arguments related to the configuration and format of the audio and video
stream, as well as the DRM system. The DRM system for a player created with
SbPlayerCreateWithUrl can be set after creation using SbPlayerSetDrmSystem.
Because the DRM system is not available at the time of SbPlayerCreateWithUrl, it
takes in a callback, `encrypted_media_init_data_encountered_cb`, which is run
when encrypted media initial data is encountered.

#### Declaration ####

```
SbPlayer SbPlayerCreateWithUrl(const char *url, SbWindow window, SbMediaTime duration_pts, SbPlayerStatusFunc player_status_func, SbPlayerEncryptedMediaInitDataEncounteredCB encrypted_media_init_data_encountered_cb, void *context)
```

### SbPlayerDestroy ###

Destroys `player`, freeing all associated resources. Each callback must receive
one more callback to say that the player was destroyed. Callbacks may be in-
flight when SbPlayerDestroy is called, and should be ignored once this function
is called.

It is not allowed to pass `player` into any other `SbPlayer` function once
SbPlayerDestroy has been called on that player.

`player`: The player to be destroyed.

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

### SbPlayerGetInfo ###

Gets a snapshot of the current player state and writes it to `out_player_info`.
This function may be called very frequently and is expected to be inexpensive.

`player`: The player about which information is being retrieved.
`out_player_info`: The information retrieved for the player.

#### Declaration ####

```
void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo *out_player_info)
```

### SbPlayerIsValid ###

Returns whether the given player handle is valid.

#### Declaration ####

```
static bool SbPlayerIsValid(SbPlayer player)
```

### SbPlayerOutputModeSupportedWithUrl ###

Returns true if the given URL player output mode is supported by the platform.
If this function returns true, it is okay to call SbPlayerCreate() with the
given `output_mode`.

#### Declaration ####

```
bool SbPlayerOutputModeSupportedWithUrl(SbPlayerOutputMode output_mode)
```

### SbPlayerSeek ###

Tells the player to freeze playback (if playback has already started), reset or
flush the decoder pipeline, and go back to the Prerolling state. The player
should restart playback once it can display the frame at `seek_to_pts`, or the
closest it can get. (Some players can only seek to I-Frames, for example.)

*   Seek must be called before samples are sent when starting playback for the
    first time, or the client never receives the
    `kSbPlayerDecoderStateNeedsData` signal.

*   A call to seek may interrupt another seek.

*   After this function is called, the client should not send any more audio or
    video samples until `SbPlayerDecoderStatusFunc` is called back with
    `kSbPlayerDecoderStateNeedsData` for each required media type.
    `SbPlayerDecoderStatusFunc` is the `decoder_status_func` callback function
    that was specified when the player was created (SbPlayerCreate).

`player`: The SbPlayer in which the seek operation is being performed.
`seek_to_pts`: The frame at which playback should begin. `ticket`: A user-
supplied unique ID that is be passed to all subsequent
`SbPlayerDecoderStatusFunc` calls. (That is the `decoder_status_func` callback
function specified when calling SbPlayerCreate.)

The `ticket` value is used to filter calls that may have been in flight when
SbPlayerSeek was called. To be very specific, once SbPlayerSeek has been called
with ticket X, a client should ignore all `SbPlayerDecoderStatusFunc` calls that
do not pass in ticket X.

#### Declaration ####

```
void SbPlayerSeek(SbPlayer player, SbMediaTime seek_to_pts, int ticket)
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

### SbPlayerSetDrmSystem ###

Sets the DRM system of a running URL-based SbPlayer created with
SbPlayerCreateWithUrl. This may only be run once for a given SbPlayer.

#### Declaration ####

```
void SbPlayerSetDrmSystem(SbPlayer player, SbDrmSystem drm_system)
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

### SbPlayerWriteSample ###

Writes a single sample of the given media type to `player`'s input stream. Its
data may be passed in via more than one buffers. The lifetime of
`sample_buffers`, `sample_buffer_sizes`, `video_sample_info`, and
`sample_drm_info` (as well as member `subsample_mapping` contained inside it)
are not guaranteed past the call to SbPlayerWriteSample. That means that before
returning, the implementation must synchronously copy any information it wants
to retain from those structures.

`player`: The player to which the sample is written. `sample_type`: The type of
sample being written. See the `SbMediaType` enum in media.h. `sample_buffers`: A
pointer to an array of buffers with `number_of_sample_buffers` elements that
hold the data for this sample. The buffers are expected to be a portion of a
bytestream of the codec type that the player was created with. The buffers
should contain a sequence of whole NAL Units for video, or a complete audio
frame. `sample_buffers` cannot be assumed to live past the call into
SbPlayerWriteSample(), so it must be copied if its content will be used after
SbPlayerWriteSample() returns. `sample_buffer_sizes`: A pointer to an array of
sizes with `number_of_sample_buffers` elements. Each of them specify the number
of bytes in the corresponding buffer contained in `sample_buffers`. None of them
can be 0. `sample_buffer_sizes` cannot be assumed to live past the call into
SbPlayerWriteSample(), so it must be copied if its content will be used after
SbPlayerWriteSample() returns. `number_of_sample_buffers`: Specify the number of
elements contained inside `sample_buffers` and `sample_buffer_sizes`. It has to
be at least one, or the call will be ignored. `sample_pts`: The timestamp of the
sample in 90KHz ticks (PTS). Note that samples MAY be written "slightly" out of
order. `video_sample_info`: Information about a video sample. This value is
required if `sample_type` is `kSbMediaTypeVideo`. Otherwise, it must be `NULL`.
`sample_drm_info`: The DRM system for the media sample. This value is required
for encrypted samples. Otherwise, it must be `NULL`.

#### Declaration ####

```
void SbPlayerWriteSample(SbPlayer player, SbMediaType sample_type, const void *const *sample_buffers, const int *sample_buffer_sizes, int number_of_sample_buffers, SbMediaTime sample_pts, const SbMediaVideoSampleInfo *video_sample_info, const SbDrmSampleInfo *sample_drm_info)
```

