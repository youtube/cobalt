---
layout: doc
title: "Starboard Module Reference: player.h"
---

Defines an interface for controlling playback of media elementary streams.

## Enums

### SbPlayerDecoderState

An indicator of whether the decoder can accept more samples.

**Values**

*   `kSbPlayerDecoderStateNeedsData` - The decoder is asking for one more sample.
*   `kSbPlayerDecoderStateBufferFull` - The decoder is not ready for any more samples, so do not send them.Note that this enum value has been deprecated and the SbPlayerimplementation should no longer use this value.
*   `kSbPlayerDecoderStateDestroyed` - The player has been destroyed, and will send no more callbacks.

### SbPlayerOutputMode

**Values**

*   `kSbPlayerOutputModeDecodeToTexture` - Requests for SbPlayer to produce an OpenGL texture that the client mustdraw every frame with its graphics rendering. It may be that we get atexture handle, but cannot perform operations like glReadPixels on it if itis DRM-protected, or it may not support DRM-protected content at all.  Whenthis output mode is provided to SbPlayerCreate(), the application will beable to pull frames via calls to SbPlayerGetCurrentFrame().
*   `kSbPlayerOutputModePunchOut` - Requests for SbPlayer to use a "punch-out" output mode, where video isrendered to the far background, and the graphics plane is automaticallycomposited on top of the video by the platform. The client must punch analpha hole out of the graphics plane for video to show through.  In thiscase, changing the video bounds must be tightly synchronized between theplayer and the graphics plane.
*   `kSbPlayerOutputModeInvalid` - An invalid output mode.

### SbPlayerState

An indicator of the general playback state.

**Values**

*   `kSbPlayerStateInitialized` - The player has just been initialized. It is expecting an SbPlayerSeek()call to enter the prerolling state.
*   `kSbPlayerStatePrerolling` - The player is prerolling, collecting enough data to fill the pipelinebefore presentation starts. After the first preroll is completed, thereshould always be a video frame to render, even if the player goes back toPrerolling after a Seek.
*   `kSbPlayerStatePresenting` - The player is presenting media, and it is either paused or activelyplaying in real-time. Note that the implementation should use thisstate to signal that the preroll has been finished.
*   `kSbPlayerStateEndOfStream` - The player is presenting media, but it is paused at the end of the stream.
*   `kSbPlayerStateDestroyed` - The player has been destroyed, and will send no more callbacks.
*   `kSbPlayerStateError` - The player encountered an error. It expects an SbPlayerDestroy() callto tear down the player. Calls to other functions may be ignored andcallbacks may not be triggered.

## Macros

<div id="macro-documentation-section">

<h3 id="sb_player_initial_ticket" class="small-h3">SB_PLAYER_INITIAL_TICKET</h3>

The value of the initial ticket held by the player before the first seek.
The player will use this ticket value to make the first call to
SbPlayerStatusFunc with kSbPlayerStateInitialized.

<h3 id="sb_player_no_duration" class="small-h3">SB_PLAYER_NO_DURATION</h3>

The value to pass into SbPlayerCreate's `duration_ptr` argument for cases
where the duration is unknown, such as for live streams.

</div>

## Structs

### SbPlayerInfo

Information about the current media playback state.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbMediaTime</code><br>        <code>current_media_pts</code></td>    <td>The position of the playback head, as precisely as possible, in 90KHz ticks
(PTS).</td>  </tr>
  <tr>
    <td><code>SbMediaTime</code><br>        <code>duration_pts</code></td>    <td>The known duration of the currently playing media stream, in 90KHz ticks
(PTS).</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>frame_width</code></td>    <td>The width of the currently displayed frame, in pixels, or 0 if not provided
by this player.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>frame_height</code></td>    <td>The height of the currently displayed frame, in pixels, or 0 if not
provided by this player.</td>  </tr>
  <tr>
    <td><code>bool</code><br>        <code>is_paused</code></td>    <td>Whether playback is currently paused.</td>  </tr>
  <tr>
    <td><code>double</code><br>        <code>volume</code></td>    <td>The current player volume in [0, 1].</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>total_video_frames</code></td>    <td>The number of video frames sent to the player since the creation of the
player.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>dropped_video_frames</code></td>    <td>The number of video frames decoded but not displayed since the creation of
the player.</td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>corrupted_video_frames</code></td>    <td>The number of video frames that failed to be decoded since the creation of
the player.</td>  </tr>
  <tr>
    <td><code>double</code><br>        <code>playback_rate</code></td>    <td>The rate of playback.  The video is played back in a speed that is
proportional to this.  By default it is 1.0 which indicates that the
playback is at normal speed.  When it is greater than one, the video is
played in a faster than normal speed.  When it is less than one, the video
is played in a slower than normal speed.  Negative speeds are not
supported.</td>  </tr>
</table>

### SbPlayer

An opaque handle to an implementation-private structure representing a
player.

## Functions

### SbPlayerCreate

**Description**

Creates a player that will be displayed on `window` for the specified
`video_codec` and `audio_codec`, acquiring all resources needed to operate
it, and returning an opaque handle to it. The expectation is that a new
player will be created and destroyed for every playback.<br>
This function returns the created player. Note the following:
<ul><li>The associated decoder of the returned player should be assumed to not be
in `kSbPlayerDecoderStateNeedsData` until <code><a href="#sbplayerseek">SbPlayerSeek()</a></code> has been called
on it.
</li><li>It is expected either that the thread that calls <code>SbPlayerCreate</code> is the same
thread that calls the other `SbPlayer` functions for that player, or that
there is a mutex guarding calls into each `SbPlayer` instance.
</li><li>If there is a platform limitation on how many players can coexist
simultaneously, then calls made to this function that attempt to exceed
that limit will return `kSbPlayerInvalid`.</li></ul>

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerCreate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerCreate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerCreate-declaration">
<pre>
SB_EXPORT SbPlayer
SbPlayerCreate(SbWindow window,
               SbMediaVideoCodec video_codec,
               SbMediaAudioCodec audio_codec,
               SbMediaTime duration_pts,
               SbDrmSystem drm_system,
               const SbMediaAudioHeader* audio_header,
               SbPlayerDeallocateSampleFunc sample_deallocate_func,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               void* context,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider* context_provider);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerCreate-stub">

```
#include "starboard/player.h"

SbPlayer SbPlayerCreate(SbWindow /*window*/,
                        SbMediaVideoCodec /*video_codec*/,
                        SbMediaAudioCodec /*audio_codec*/,
                        SbMediaTime /*duration_pts*/,
                        SbDrmSystem /*drm_system*/,
                        const SbMediaAudioHeader* /*audio_header*/,
                        SbPlayerDeallocateSampleFunc /*sample_deallocate_func*/,
                        SbPlayerDecoderStatusFunc /*decoder_status_func*/,
                        SbPlayerStatusFunc /*player_status_func*/,
                        void* /*context*/,
                        SbPlayerOutputMode /*output_mode*/,
                        SbDecodeTargetGraphicsContextProvider* /*provider*/) {
  return kSbPlayerInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbWindow</code><br>        <code>window</code></td>
    <td>The window that will display the player. <code>window</code> can be
<code>kSbWindowInvalid</code> for platforms where video is only displayed on a
particular window that the underlying implementation already has access to.</td>
  </tr>
  <tr>
    <td><code>SbMediaVideoCodec</code><br>        <code>video_codec</code></td>
    <td>The video codec used for the player. If <code>video_codec</code> is
<code>kSbMediaVideoCodecNone</code>, the player is an audio-only player. If
<code>video_codec</code> is any other value, the player is an audio/video decoder.</td>
  </tr>
  <tr>
    <td><code>SbMediaAudioCodec</code><br>        <code>audio_codec</code></td>
    <td>The audio codec used for the player. The value should never
be <code>kSbMediaAudioCodecNone</code>. In addition, the caller must provide a
populated <code>audio_header</code> if the audio codec is <code>kSbMediaAudioCodecAac</code>.</td>
  </tr>
  <tr>
    <td><code>SbMediaTime</code><br>        <code>duration_pts</code></td>
    <td>The expected media duration in 90KHz ticks (PTS). It may be
set to <code>SB_PLAYER_NO_DURATION</code> for live streams.</td>
  </tr>
  <tr>
    <td><code>SbDrmSystem</code><br>        <code>drm_system</code></td>
    <td>If the media stream has encrypted portions, then this
parameter provides an appropriate DRM system, created with
<code>SbDrmCreateSystem()</code>. If the stream does not have encrypted portions,
then <code>drm_system</code> may be <code>kSbDrmSystemInvalid</code>.</td>
  </tr>
  <tr>
    <td><code>const SbMediaAudioHeader*</code><br>        <code>audio_header</code></td>
    <td>Note that the caller must provide a populated <code>audio_header</code>
if the audio codec is <code>kSbMediaAudioCodecAac</code>. Otherwise, <code>audio_header</code>
can be NULL. See media.h for the format of the <code>SbMediaAudioHeader</code> struct.
Note that <code>audio_specific_config</code> is a pointer and the content it points to
is no longer valid after this function returns.  The implementation has to
make a copy of the content if it is needed after the function returns.</td>
  </tr>
  <tr>
    <td><code>SbPlayerDeallocateSampleFunc</code><br>        <code>sample_deallocate_func</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>SbPlayerDecoderStatusFunc</code><br>        <code>decoder_status_func</code></td>
    <td>If not <code>NULL</code>, the decoder calls this function on an
internal thread to provide an update on the decoder's status. No work
should be done on this thread. Rather, it should just signal the client
thread interacting with the decoder.</td>
  </tr>
  <tr>
    <td><code>SbPlayerStatusFunc</code><br>        <code>player_status_func</code></td>
    <td>If not <code>NULL</code>, the player calls this function on an
internal thread to provide an update on the playback status. No work
should be done on this thread. Rather, it should just signal the client
thread interacting with the decoder.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>context</code></td>
    <td>This is passed to all callbacks and is generally used to point
at a class or struct that contains state associated with the player.</td>
  </tr>
  <tr>
    <td><code>SbPlayerOutputMode</code><br>        <code>output_mode</code></td>
    <td>Selects how the decoded video frames will be output.  For
example, kSbPlayerOutputModePunchOut indicates that the decoded video
frames will be output to a background video layer by the platform, and
kSbPlayerOutputDecodeToTexture indicates that the decoded video frames
should be made available for the application to pull via calls to
SbPlayerGetCurrentFrame().</td>
  </tr>
  <tr>
    <td><code>SbDecodeTargetGraphicsContextProvider*</code><br>        <code>context_provider</code></td>
    <td> </td>  </tr>
</table>

### SbPlayerCreateWithUrl

**Description**

Creates a URL-based SbPlayer that will be displayed on `window` for
the specified URL `url`, acquiring all resources needed to operate
it, and returning an opaque handle to it. The expectation is that a
new player will be created and destroyed for every playback.<br>
In many ways this function is similar to <code><a href="#sbplayercreate">SbPlayerCreate</a></code>, but it is
missing the input arguments related to the configuration and format
of the audio and video stream, as well as the DRM system. The DRM
system for a player created with <code>SbPlayerCreateWithUrl</code> can be set
after creation using <code><a href="#sbplayersetdrmsystem">SbPlayerSetDrmSystem</a></code>. Because the DRM system
is not available at the time of <code>SbPlayerCreateWithUrl</code>, it takes in
a callback, `encrypted_media_init_data_encountered_cb`, which is
run when encrypted media initial data is encountered.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerCreateWithUrl-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerCreateWithUrl-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerCreateWithUrl-declaration">
<pre>
SB_EXPORT SbPlayer
SbPlayerCreateWithUrl(const char* url,
                      SbWindow window,
                      SbMediaTime duration_pts,
                      SbPlayerStatusFunc player_status_func,
                      SbPlayerEncryptedMediaInitDataEncounteredCB
                          encrypted_media_init_data_encountered_cb,
                      void* context);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerCreateWithUrl-stub">

```
#include "starboard/player.h"

#if SB_HAS(PLAYER_WITH_URL)

SbPlayer SbPlayerCreateWithUrl(
    const char* /* url */,
    SbWindow /* window */,
    SbMediaTime /* duration_pts */,
    SbPlayerStatusFunc /* player_status_func */,
    SbPlayerEncryptedMediaInitDataEncounteredCB /* cb */,
    void* /* context */) {
  // Stub.
  return kSbPlayerInvalid;
}

// TODO: Actually move this to a separate file or get rid of these URL
// player stubs altogether.
void SbPlayerSetDrmSystem(SbPlayer player, SbDrmSystem drm_system) { /* Stub */
}

#endif  // SB_HAS(PLAYER_WITH_URL)
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const char*</code><br>
        <code>url</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbWindow</code><br>
        <code>window</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbMediaTime</code><br>
        <code>duration_pts</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbPlayerStatusFunc</code><br>
        <code>player_status_func</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code></code><br>
        <code>SbPlayerEncryptedMediaInitDataEncounteredCB</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code></code><br>
        <code>encrypted_media_init_data_encountered_cb</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>void*</code><br>
        <code>context</code></td>
    <td> </td>
  </tr>
</table>

### SbPlayerDestroy

**Description**

Destroys `player`, freeing all associated resources. Each callback must
receive one more callback to say that the player was destroyed. Callbacks
may be in-flight when <code>SbPlayerDestroy</code> is called, and should be ignored once
this function is called.<br>
It is not allowed to pass `player` into any other `SbPlayer` function once
SbPlayerDestroy has been called on that player.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerDestroy-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerDestroy-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerDestroy-declaration">
<pre>
SB_EXPORT void SbPlayerDestroy(SbPlayer player);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerDestroy-stub">

```
#include "starboard/player.h"

void SbPlayerDestroy(SbPlayer /*player*/) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>        <code>player</code></td>
    <td>The player to be destroyed.</td>
  </tr>
</table>

### SbPlayerGetCurrentFrame

**Description**

Given a player created with the kSbPlayerOutputModeDecodeToTexture
output mode, it will return a SbDecodeTarget representing the current frame
to be rasterized.  On GLES systems, this function must be called on a
thread with an EGLContext current, and specifically the EGLContext that will
be used to eventually render the frame.  If this function is called with a
`player` object that was created with an output mode other than
kSbPlayerOutputModeDecodeToTexture, kSbDecodeTargetInvalid is returned.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerGetCurrentFrame-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerGetCurrentFrame-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerGetCurrentFrame-declaration">
<pre>
SB_EXPORT SbDecodeTarget SbPlayerGetCurrentFrame(SbPlayer player);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerGetCurrentFrame-stub">

```
#include "starboard/player.h"

SbDecodeTarget SbPlayerGetCurrentFrame(SbPlayer /*player*/) {
  return kSbDecodeTargetInvalid;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>
        <code>player</code></td>
    <td> </td>
  </tr>
</table>

### SbPlayerGetInfo

**Description**

Gets a snapshot of the current player state and writes it to
`out_player_info`. This function may be called very frequently and is
expected to be inexpensive.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerGetInfo-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerGetInfo-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerGetInfo-declaration">
<pre>
SB_EXPORT void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo* out_player_info);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerGetInfo-stub">

```
#include "starboard/player.h"

void SbPlayerGetInfo(SbPlayer /*player*/, SbPlayerInfo* /*out_player_info*/) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>        <code>player</code></td>
    <td>The player about which information is being retrieved.</td>
  </tr>
  <tr>
    <td><code>SbPlayerInfo*</code><br>        <code>out_player_info</code></td>
    <td>The information retrieved for the player.</td>
  </tr>
</table>

### SbPlayerIsValid

**Description**

Returns whether the given player handle is valid.

**Declaration**

```
static SB_C_INLINE bool SbPlayerIsValid(SbPlayer player) {
  return player != kSbPlayerInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>
        <code>player</code></td>
    <td> </td>
  </tr>
</table>

### SbPlayerOutputModeSupported

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerOutputModeSupported-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerOutputModeSupported-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerOutputModeSupported-declaration">
<pre>
SB_EXPORT bool SbPlayerOutputModeSupported(SbPlayerOutputMode output_mode,
                                           SbMediaVideoCodec codec,
                                           SbDrmSystem drm_system);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerOutputModeSupported-stub">

```
#include "starboard/player.h"

bool SbPlayerOutputModeSupported(SbPlayerOutputMode /*output_mode*/,
                                 SbMediaVideoCodec /*codec*/,
                                 SbDrmSystem /*drm_system*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayerOutputMode</code><br>
        <code>output_mode</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbMediaVideoCodec</code><br>
        <code>codec</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDrmSystem</code><br>
        <code>drm_system</code></td>
    <td> </td>
  </tr>
</table>

### SbPlayerOutputModeSupportedWithUrl

**Description**

Returns true if the given URL player output mode is supported by
the platform.  If this function returns true, it is okay to call
SbPlayerCreate() with the given `output_mode`.

**Declaration**

```
SB_EXPORT bool SbPlayerOutputModeSupportedWithUrl(
    SbPlayerOutputMode output_mode);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayerOutputMode</code><br>
        <code>output_mode</code></td>
    <td> </td>
  </tr>
</table>

### SbPlayerSeek

**Description**

Tells the player to freeze playback (if playback has already started),
reset or flush the decoder pipeline, and go back to the Prerolling state.
The player should restart playback once it can display the frame at
`seek_to_pts`, or the closest it can get. (Some players can only seek to
I-Frames, for example.)<br>
<ul><li>Seek must be called before samples are sent when starting playback for
the first time, or the client never receives the
`kSbPlayerDecoderStateNeedsData` signal.
</li><li>A call to seek may interrupt another seek.
</li><li>After this function is called, the client should not send any more audio
or video samples until `SbPlayerDecoderStatusFunc` is called back with
`kSbPlayerDecoderStateNeedsData` for each required media type.
`SbPlayerDecoderStatusFunc` is the `decoder_status_func` callback function
that was specified when the player was created (SbPlayerCreate).</li></ul><br>
The `ticket` value is used to filter calls that may have been in flight
when <code>SbPlayerSeek</code> was called. To be very specific, once <code>SbPlayerSeek</code> has
been called with ticket X, a client should ignore all
`SbPlayerDecoderStatusFunc` calls that do not pass in ticket X.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerSeek-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerSeek-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerSeek-declaration">
<pre>
SB_EXPORT void SbPlayerSeek(SbPlayer player,
                            SbMediaTime seek_to_pts,
                            int ticket);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerSeek-stub">

```
#include "starboard/player.h"

void SbPlayerSeek(SbPlayer /*player*/,
                  SbMediaTime /*seek_to_pts*/,
                  int /*ticket*/) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>        <code>player</code></td>
    <td>The SbPlayer in which the seek operation is being performed.</td>
  </tr>
  <tr>
    <td><code>SbMediaTime</code><br>        <code>seek_to_pts</code></td>
    <td>The frame at which playback should begin.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>ticket</code></td>
    <td>A user-supplied unique ID that is be passed to all subsequent
<code>SbPlayerDecoderStatusFunc</code> calls. (That is the <code>decoder_status_func</code>
callback function specified when calling <code><a href="#sbplayercreate">SbPlayerCreate</a></code>.)</td>
  </tr>
</table>

### SbPlayerSetBounds

**Description**

Sets the player bounds to the given graphics plane coordinates. The changes
do not take effect until the next graphics frame buffer swap. The default
bounds for a player is the full screen.  This function is only relevant when
the `player` is created with the kSbPlayerOutputModePunchOut output mode, and
if this is not the case then this function call can be ignored.<br>
This function is called on every graphics frame that changes the video
bounds. For example, if the video bounds are being animated, then this will
be called at up to 60 Hz. Since the function could be called up to once per
frame, implementors should take care to avoid related performance concerns
with such frequent calls.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerSetBounds-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerSetBounds-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerSetBounds-declaration">
<pre>
SB_EXPORT void SbPlayerSetBounds(SbPlayer player,
                                 int z_index,
                                 int x,
                                 int y,
                                 int width,
                                 int height);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerSetBounds-stub">

```
#include "starboard/player.h"

void SbPlayerSetBounds(SbPlayer /*player*/,
                       int /*z_index*/,
                       int /*x*/,
                       int /*y*/,
                       int /*width*/,
                       int /*height*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>        <code>player</code></td>
    <td>The player that is being resized.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>z_index</code></td>
    <td>The z-index of the player.  When the bounds of multiple players
are overlapped, the one with larger z-index will be rendered on
top of the ones with smaller z-index.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>x</code></td>
    <td>The x-coordinate of the upper-left corner of the player.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>y</code></td>
    <td>The y-coordinate of the upper-left corner of the player.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>width</code></td>
    <td>The width of the player, in pixels.</td>
  </tr>
  <tr>
    <td><code>int</code><br>        <code>height</code></td>
    <td>The height of the player, in pixels.</td>
  </tr>
</table>

### SbPlayerSetDrmSystem

**Description**

Sets the DRM system of a running URL-based SbPlayer created with
SbPlayerCreateWithUrl. This may only be run once for a given
SbPlayer.

**Declaration**

```
SB_EXPORT void SbPlayerSetDrmSystem(SbPlayer player, SbDrmSystem drm_system);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>
        <code>player</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>SbDrmSystem</code><br>
        <code>drm_system</code></td>
    <td> </td>
  </tr>
</table>

### SbPlayerSetPlaybackRate

**Description**

Set the playback rate of the `player`.  `rate` is default to 1.0 which
indicates the playback is at its original speed.  A `rate` greater than one
will make the playback faster than its original speed.  For example, when
`rate` is 2, the video will be played at twice the speed as its original
speed.  A `rate` less than 1.0 will make the playback slower than its
original speed.  When `rate` is 0, the playback will be paused.
The function returns true when the playback rate is set to `playback_rate` or
to a rate that is close to `playback_rate` which the implementation supports.
It returns false when the playback rate is unchanged, this can happen when
`playback_rate` is negative or if it is too high to support.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerSetPlaybackRate-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerSetPlaybackRate-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerSetPlaybackRate-declaration">
<pre>
SB_EXPORT bool SbPlayerSetPlaybackRate(SbPlayer player, double playback_rate);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerSetPlaybackRate-stub">

```
#include "starboard/player.h"

bool SbPlayerSetPlaybackRate(SbPlayer /*player*/, double /*playback_rate*/) {
  return false;
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>
        <code>player</code></td>
    <td> </td>
  </tr>
  <tr>
    <td><code>double</code><br>
        <code>playback_rate</code></td>
    <td> </td>
  </tr>
</table>

### SbPlayerSetVolume

**Description**

Sets the player's volume.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerSetVolume-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerSetVolume-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerSetVolume-declaration">
<pre>
SB_EXPORT void SbPlayerSetVolume(SbPlayer player, double volume);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerSetVolume-stub">

```
#include "starboard/player.h"

void SbPlayerSetVolume(SbPlayer /*player*/, double /*volume*/) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>        <code>player</code></td>
    <td>The player in which the volume is being adjusted.</td>
  </tr>
  <tr>
    <td><code>double</code><br>        <code>volume</code></td>
    <td>The new player volume. The value must be between <code>0.0</code> and <code>1.0</code>,
inclusive. A value of <code>0.0</code> means that the audio should be muted, and a
value of <code>1.0</code> means that it should be played at full volume.</td>
  </tr>
</table>

### SbPlayerWriteEndOfStream

**Description**

Writes a marker to `player`'s input stream of `stream_type` indicating that
there are no more samples for that media type for the remainder of this
media stream. This marker is invalidated, along with the rest of the stream's
contents, after a call to <code><a href="#sbplayerseek">SbPlayerSeek</a></code>.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerWriteEndOfStream-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerWriteEndOfStream-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerWriteEndOfStream-declaration">
<pre>
SB_EXPORT void SbPlayerWriteEndOfStream(SbPlayer player,
                                        SbMediaType stream_type);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerWriteEndOfStream-stub">

```
#include "starboard/player.h"

void SbPlayerWriteEndOfStream(SbPlayer /*player*/,
                              SbMediaType /*stream_type*/) {}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>        <code>player</code></td>
    <td>The player to which the marker is written.</td>
  </tr>
  <tr>
    <td><code>SbMediaType</code><br>        <code>stream_type</code></td>
    <td>The type of stream for which the marker is written.</td>
  </tr>
</table>

### SbPlayerWriteSample

**Description**

Writes a single sample of the given media type to `player`'s input stream.
Its data may be passed in via more than one buffers.  The lifetime of
`sample_buffers`, `sample_buffer_sizes`, `video_sample_info`, and
`sample_drm_info` (as well as member `subsample_mapping` contained inside it)
are not guaranteed past the call to <code>SbPlayerWriteSample</code>. That means that
before returning, the implementation must synchronously copy any information
it wants to retain from those structures.

**Declaration and definitions**

<div class="mdl-tabs mdl-js-tabs mdl-js-ripple-effect">
  <div class="mdl-tabs__tab-bar">
    <a href="#SbPlayerWriteSample-declaration" class="mdl-tabs__tab is-active">Declaration</a>
    <a href="#SbPlayerWriteSample-stub" class="mdl-tabs__tab">stub</a>
  </div>
  <div class="mdl-tabs__panel is-active" id="SbPlayerWriteSample-declaration">
<pre>
SB_EXPORT void SbPlayerWriteSample(
    SbPlayer player,
    SbMediaType sample_type,
#if SB_API_VERSION >= 6
    const void* const* sample_buffers,
    const int* sample_buffer_sizes,
#else   // SB_API_VERSION >= 6
    const void** sample_buffers,
    int* sample_buffer_sizes,
#endif  // SB_API_VERSION >= 6
    int number_of_sample_buffers,
    SbMediaTime sample_pts,
    const SbMediaVideoSampleInfo* video_sample_info,
    const SbDrmSampleInfo* sample_drm_info);
</pre>
</div>
  <div class="mdl-tabs__panel" id="SbPlayerWriteSample-stub">

```
#include "starboard/player.h"

void SbPlayerWriteSample(SbPlayer /*player*/,
                         SbMediaType /*sample_type*/,
#if SB_API_VERSION >= 6
                         const void* const* /*sample_buffers*/,
                         const int* /*sample_buffer_sizes*/,
#else   // SB_API_VERSION >= 6
                         const void** /*sample_buffers*/,
                         int* /*sample_buffer_sizes*/,
#endif  // SB_API_VERSION >= 6
                         int /*number_of_sample_buffers*/,
                         SbMediaTime /*sample_pts*/,
                         const SbMediaVideoSampleInfo* /*video_sample_info*/,
                         const SbDrmSampleInfo* /*sample_drm_info*/) {
}
```

  </div>
</div>

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbPlayer</code><br>        <code>player</code></td>
    <td>The player to which the sample is written.</td>
  </tr>
  <tr>
    <td><code>SbMediaType</code><br>        <code>sample_type</code></td>
    <td>The type of sample being written. See the <code>SbMediaType</code>
enum in media.h.</td>
  </tr>
  <tr>
    <td><code>#if SB_API_VERSION >=</code><br>        <code>6</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>const void* const*</code><br>        <code>sample_buffers</code></td>
    <td>A pointer to an array of buffers with
<code>number_of_sample_buffers</code> elements that hold the data for this sample. The
buffers are expected to be a portion of a bytestream of the codec type that
the player was created with. The buffers should contain a sequence of whole
NAL Units for video, or a complete audio frame.  <code>sample_buffers</code> cannot be
assumed to live past the call into <code>SbPlayerWriteSample()</code>, so it must be
copied if its content will be used after <code>SbPlayerWriteSample()</code> returns.</td>
  </tr>
  <tr>
    <td><code>const int*</code><br>        <code>sample_buffer_sizes</code></td>
    <td>A pointer to an array of sizes with
<code>number_of_sample_buffers</code> elements.  Each of them specify the number of
bytes in the corresponding buffer contained in <code>sample_buffers</code>.  None of
them can be 0.  <code>sample_buffer_sizes</code> cannot be assumed to live past the
call into <code>SbPlayerWriteSample()</code>, so it must be copied if its content will
be used after <code>SbPlayerWriteSample()</code> returns.</td>
  </tr>
  <tr>
    <td><code>#else   // SB_API_VERSION >=</code><br>        <code>6</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>const void**</code><br>        <code>sample_buffers</code></td>
    <td>A pointer to an array of buffers with
<code>number_of_sample_buffers</code> elements that hold the data for this sample. The
buffers are expected to be a portion of a bytestream of the codec type that
the player was created with. The buffers should contain a sequence of whole
NAL Units for video, or a complete audio frame.  <code>sample_buffers</code> cannot be
assumed to live past the call into <code>SbPlayerWriteSample()</code>, so it must be
copied if its content will be used after <code>SbPlayerWriteSample()</code> returns.</td>
  </tr>
  <tr>
    <td><code>int*</code><br>        <code>sample_buffer_sizes</code></td>
    <td>A pointer to an array of sizes with
<code>number_of_sample_buffers</code> elements.  Each of them specify the number of
bytes in the corresponding buffer contained in <code>sample_buffers</code>.  None of
them can be 0.  <code>sample_buffer_sizes</code> cannot be assumed to live past the
call into <code>SbPlayerWriteSample()</code>, so it must be copied if its content will
be used after <code>SbPlayerWriteSample()</code> returns.</td>
  </tr>
  <tr>
    <td><code>#endif  // SB_API_VERSION >=</code><br>        <code>6</code></td>
    <td> </td>  </tr>
  <tr>
    <td><code>int</code><br>        <code>number_of_sample_buffers</code></td>
    <td>Specify the number of elements contained inside
<code>sample_buffers</code> and <code>sample_buffer_sizes</code>.  It has to be at least one, or
the call will be ignored.</td>
  </tr>
  <tr>
    <td><code>SbMediaTime</code><br>        <code>sample_pts</code></td>
    <td>The timestamp of the sample in 90KHz ticks (PTS). Note that
samples MAY be written "slightly" out of order.</td>
  </tr>
  <tr>
    <td><code>const SbMediaVideoSampleInfo*</code><br>        <code>video_sample_info</code></td>
    <td>Information about a video sample. This value is
required if <code>sample_type</code> is <code>kSbMediaTypeVideo</code>. Otherwise, it must be
<code>NULL</code>.</td>
  </tr>
  <tr>
    <td><code>const SbDrmSampleInfo*</code><br>        <code>sample_drm_info</code></td>
    <td>The DRM system for the media sample. This value is
required for encrypted samples. Otherwise, it must be <code>NULL</code>.</td>
  </tr>
</table>

