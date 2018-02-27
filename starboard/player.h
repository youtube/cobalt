// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Module Overview: Starboard Player module
//
// Defines an interface for controlling playback of media elementary streams.

#ifndef STARBOARD_PLAYER_H_
#define STARBOARD_PLAYER_H_

#include "starboard/configuration.h"

#include "starboard/decode_target.h"
#include "starboard/drm.h"
#include "starboard/export.h"
#include "starboard/media.h"
#include "starboard/types.h"
#include "starboard/window.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Types -----------------------------------------------------------------

// An indicator of whether the decoder can accept more samples.
typedef enum SbPlayerDecoderState {
  // The decoder is asking for one more sample.
  kSbPlayerDecoderStateNeedsData,

  // The decoder is not ready for any more samples, so do not send them.
  // Note that this enum value has been deprecated and the SbPlayer
  // implementation should no longer use this value.
  kSbPlayerDecoderStateBufferFull,

  // The player has been destroyed, and will send no more callbacks.
  kSbPlayerDecoderStateDestroyed,
} SbPlayerDecoderState;

// An indicator of the general playback state.
typedef enum SbPlayerState {
  // The player has just been initialized. It is expecting an SbPlayerSeek()
  // call to enter the prerolling state.
  kSbPlayerStateInitialized,

  // The player is prerolling, collecting enough data to fill the pipeline
  // before presentation starts. After the first preroll is completed, there
  // should always be a video frame to render, even if the player goes back to
  // Prerolling after a Seek.
  kSbPlayerStatePrerolling,

  // The player is presenting media, and it is either paused or actively
  // playing in real-time. Note that the implementation should use this
  // state to signal that the preroll has been finished.
  kSbPlayerStatePresenting,

  // The player is presenting media, but it is paused at the end of the stream.
  kSbPlayerStateEndOfStream,

  // The player has been destroyed, and will send no more callbacks.
  kSbPlayerStateDestroyed,
#if !SB_HAS(PLAYER_ERROR_MESSAGE)
  // The player encountered an error. It expects an SbPlayerDestroy() call
  // to tear down the player. Calls to other functions may be ignored and
  // callbacks may not be triggered.
  kSbPlayerStateError,
#endif  // !SB_HAS(PLAYER_ERROR_MESSAGE)
} SbPlayerState;

#if SB_HAS(PLAYER_ERROR_MESSAGE)
typedef enum SbPlayerError {
  kSbPlayerErrorDecode,
#if SB_HAS(PLAYER_WITH_URL)
  // The following error codes are used by the URL player to report detailed
  // errors. They are not required in non-URL player mode.
  kSbPlayerErrorNetwork,
  kSbPlayerErrorSrcNotSupported,
#endif  //  SB_HAS(PLAYER_WITH_URL)
} SbPlayerError;
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)

typedef enum SbPlayerOutputMode {
  // Requests for SbPlayer to produce an OpenGL texture that the client must
  // draw every frame with its graphics rendering. It may be that we get a
  // texture handle, but cannot perform operations like glReadPixels on it if it
  // is DRM-protected, or it may not support DRM-protected content at all.  When
  // this output mode is provided to SbPlayerCreate(), the application will be
  // able to pull frames via calls to SbPlayerGetCurrentFrame().
  kSbPlayerOutputModeDecodeToTexture,

  // Requests for SbPlayer to use a "punch-out" output mode, where video is
  // rendered to the far background, and the graphics plane is automatically
  // composited on top of the video by the platform. The client must punch an
  // alpha hole out of the graphics plane for video to show through.  In this
  // case, changing the video bounds must be tightly synchronized between the
  // player and the graphics plane.
  kSbPlayerOutputModePunchOut,

  // An invalid output mode.
  kSbPlayerOutputModeInvalid,
} SbPlayerOutputMode;

// Information about the current media playback state.
typedef struct SbPlayerInfo {
  // The position of the playback head, as precisely as possible, in 90KHz ticks
  // (PTS).
  SbMediaTime current_media_pts;

  // The known duration of the currently playing media stream, in 90KHz ticks
  // (PTS).
  SbMediaTime duration_pts;

  // The result of getStartDate for the currently playing media stream, in
  // microseconds since the epoch of January 1, 1601 UTC.
  SbTime start_date;

  // The width of the currently displayed frame, in pixels, or 0 if not provided
  // by this player.
  int frame_width;

  // The height of the currently displayed frame, in pixels, or 0 if not
  // provided by this player.
  int frame_height;

  // Whether playback is currently paused.
  bool is_paused;

  // The current player volume in [0, 1].
  double volume;

  // The number of video frames sent to the player since the creation of the
  // player.
  int total_video_frames;

  // The number of video frames decoded but not displayed since the creation of
  // the player.
  int dropped_video_frames;

  // The number of video frames that failed to be decoded since the creation of
  // the player.
  int corrupted_video_frames;

  // The rate of playback.  The video is played back in a speed that is
  // proportional to this.  By default it is 1.0 which indicates that the
  // playback is at normal speed.  When it is greater than one, the video is
  // played in a faster than normal speed.  When it is less than one, the video
  // is played in a slower than normal speed.  Negative speeds are not
  // supported.
  double playback_rate;

#if SB_HAS(PLAYER_WITH_URL)
  // The position of the buffer head, as precisely as possible, in 90KHz ticks
  // (PTS).
  SbMediaTime buffer_start_pts;

  // The known duration of the currently playing media buffer, in 90KHz ticks
  // (PTS).
  SbMediaTime buffer_duration_pts;
#endif  // SB_HAS(PLAYER_WITH_URL)
} SbPlayerInfo;

// An opaque handle to an implementation-private structure representing a
// player.
typedef struct SbPlayerPrivate* SbPlayer;

// Callback for decoder status updates, called in response to a call to
// SbPlayerSeek() or SbPlayerWriteSample(). This callback will never be called
// until at least one call to SbPlayerSeek has occurred. |ticket| will be set to
// the ticket passed into the last received call to SbPlayerSeek() at the time
// this callback was dispatched. This is to distinguish status callbacks for
// interrupting seeks. These callbacks will happen on a different thread than
// the calling thread, and it is not valid to call SbPlayer functions from
// within this callback. After an update with kSbPlayerDecoderStateNeedsData,
// the user of the player will make at most one call to SbPlayerWriteSample() or
// SbPlayerWriteEndOfStream(). The player implementation should update the
// decoder status again after such call to notify its user to continue writing
// more frames.
typedef void (*SbPlayerDecoderStatusFunc)(SbPlayer player,
                                          void* context,
                                          SbMediaType type,
                                          SbPlayerDecoderState state,
                                          int ticket);

// Callback for player status updates. These callbacks will happen on a
// different thread than the calling thread, and it is not valid to call
// SbPlayer functions from within this callback.
typedef void (*SbPlayerStatusFunc)(SbPlayer player,
                                   void* context,
                                   SbPlayerState state,
                                   int ticket);

#if SB_HAS(PLAYER_ERROR_MESSAGE)
// Callback for player errors, that may set a |message|.
// |error|: indicates the error code.
// |message|: provides specific informative diagnostic message about the error
//            condition encountered. It is ok for the message to be an empty
//            string or NULL if no information is available.
typedef void (*SbPlayerErrorFunc)(SbPlayer player,
                                  void* context,
                                  SbPlayerError error,
                                  const char* message);
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)

// Callback to free the given sample buffer data.  When more than one buffer
// are sent in SbPlayerWriteSample(), the implementation only has to call this
// callback with |sample_buffer| points to the the first buffer.
typedef void (*SbPlayerDeallocateSampleFunc)(SbPlayer player,
                                             void* context,
                                             const void* sample_buffer);

#if SB_HAS(PLAYER_WITH_URL)
// Callback to queue an encrypted event for initialization data
// encountered in media data. |init_data_type| should be a string
// matching one of the EME initialization data types : "cenc",
// "fairplay", "keyids", or "webm", |init_data| is the initialization
// data, and |init_data_length| is the length of the data.
typedef void (*SbPlayerEncryptedMediaInitDataEncounteredCB)(
    SbPlayer player,
    void* context,
    const char* init_data_type,
    const unsigned char* init_data,
    unsigned int init_data_length);
#endif  // SB_HAS(PLAYER_WITH_URL)

// --- Constants -------------------------------------------------------------

// The value to pass into SbPlayerCreate's |duration_pts| argument for cases
// where the duration is unknown, such as for live streams.
#define SB_PLAYER_NO_DURATION (-1)

// The value of the initial ticket held by the player before the first seek.
// The player will use this ticket value to make the first call to
// SbPlayerStatusFunc with kSbPlayerStateInitialized.
#define SB_PLAYER_INITIAL_TICKET (0)

// Well-defined value for an invalid player.
#define kSbPlayerInvalid ((SbPlayer)NULL)

// Returns whether the given player handle is valid.
static SB_C_INLINE bool SbPlayerIsValid(SbPlayer player) {
  return player != kSbPlayerInvalid;
}

// --- Functions -------------------------------------------------------------

#if SB_HAS(PLAYER_WITH_URL)

// Creates a URL-based SbPlayer that will be displayed on |window| for
// the specified URL |url|, acquiring all resources needed to operate
// it, and returning an opaque handle to it. The expectation is that a
// new player will be created and destroyed for every playback.
//
// In many ways this function is similar to SbPlayerCreate, but it is
// missing the input arguments related to the configuration and format
// of the audio and video stream, as well as the DRM system. The DRM
// system for a player created with SbPlayerCreateWithUrl can be set
// after creation using SbPlayerSetDrmSystem. Because the DRM system
// is not available at the time of SbPlayerCreateWithUrl, it takes in
// a callback, |encrypted_media_init_data_encountered_cb|, which is
// run when encrypted media initial data is encountered.
SB_EXPORT SbPlayer
SbPlayerCreateWithUrl(const char* url,
                      SbWindow window,
                      SbMediaTime duration_pts,
                      SbPlayerStatusFunc player_status_func,
                      SbPlayerEncryptedMediaInitDataEncounteredCB
                          encrypted_media_init_data_encountered_cb,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
                      SbPlayerErrorFunc player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
                      void* context);

// Sets the DRM system of a running URL-based SbPlayer created with
// SbPlayerCreateWithUrl. This may only be run once for a given
// SbPlayer.
SB_EXPORT void SbPlayerSetDrmSystem(SbPlayer player, SbDrmSystem drm_system);

// Returns true if the given URL player output mode is supported by
// the platform.  If this function returns true, it is okay to call
// SbPlayerCreate() with the given |output_mode|.
SB_EXPORT bool SbPlayerOutputModeSupportedWithUrl(
    SbPlayerOutputMode output_mode);

#else  // SB_HAS(PLAYER_WITH_URL)

// Creates a player that will be displayed on |window| for the specified
// |video_codec| and |audio_codec|, acquiring all resources needed to operate
// it, and returning an opaque handle to it. The expectation is that a new
// player will be created and destroyed for every playback.
//
// This function returns the created player. Note the following:
// - The associated decoder of the returned player should be assumed to not be
//   in |kSbPlayerDecoderStateNeedsData| until SbPlayerSeek() has been called
//   on it.
// - It is expected either that the thread that calls SbPlayerCreate is the same
//   thread that calls the other |SbPlayer| functions for that player, or that
//   there is a mutex guarding calls into each |SbPlayer| instance.
// - If there is a platform limitation on how many players can coexist
//   simultaneously, then calls made to this function that attempt to exceed
//   that limit will return |kSbPlayerInvalid|.
//
// |window|: The window that will display the player. |window| can be
//   |kSbWindowInvalid| for platforms where video is only displayed on a
//   particular window that the underlying implementation already has access to.
//
// |video_codec|: The video codec used for the player. If |video_codec| is
//   |kSbMediaVideoCodecNone|, the player is an audio-only player. If
//   |video_codec| is any other value, the player is an audio/video decoder.
//
// |audio_codec|: The audio codec used for the player. The value should never
//   be |kSbMediaAudioCodecNone|. In addition, the caller must provide a
//   populated |audio_header| if the audio codec is |kSbMediaAudioCodecAac|.
#if SB_HAS(AUDIOLESS_VIDEO)
//   This can be set to |kSbMediaAudioCodecNone| to play a video without any
//   audio track.  In such case |audio_header| must be NULL.
#endif  // SB_HAS(AUDIOLESS_VIDEO)
//
// |duration_pts|: The expected media duration in 90KHz ticks (PTS). It may be
//   set to |SB_PLAYER_NO_DURATION| for live streams.
//
// |drm_system|: If the media stream has encrypted portions, then this
//   parameter provides an appropriate DRM system, created with
//   |SbDrmCreateSystem()|. If the stream does not have encrypted portions,
//   then |drm_system| may be |kSbDrmSystemInvalid|.
//
// |audio_header|: Note that the caller must provide a populated |audio_header|
//   if the audio codec is |kSbMediaAudioCodecAac|. Otherwise, |audio_header|
//   can be NULL. See media.h for the format of the |SbMediaAudioHeader| struct.
#if SB_API_VERSION >= 6
//   Note that |audio_specific_config| is a pointer and the content it points to
//   is no longer valid after this function returns.  The implementation has to
//   make a copy of the content if it is needed after the function returns.
#endif  // SB_API_VERSION >= 6
#if SB_HAS(AUDIOLESS_VIDEO)
//   When |audio_codec| is |kSbMediaAudioCodecNone|, this must be set to NULL.
#endif  // SB_HAS(AUDIOLESS_VIDEO)
//
// |sample_deallocator_func|: If not |NULL|, the player calls this function
//   on an internal thread to free the sample buffers passed into
//   SbPlayerWriteSample().
//
// |decoder_status_func|: If not |NULL|, the decoder calls this function on an
//   internal thread to provide an update on the decoder's status. No work
//   should be done on this thread. Rather, it should just signal the client
//   thread interacting with the decoder.
//
// |player_status_func|: If not |NULL|, the player calls this function on an
//   internal thread to provide an update on the playback status. No work
//   should be done on this thread. Rather, it should just signal the client
//   thread interacting with the decoder.
//
#if SB_HAS(PLAYER_ERROR_MESSAGE)
// |player_error_func|: If not |NULL|, the player calls this function on an
//   internal thread to provide an update on the error status. This callback is
//   responsible for setting the media error message.
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
//
// |context|: This is passed to all callbacks and is generally used to point
//   at a class or struct that contains state associated with the player.
//
// |output_mode|: Selects how the decoded video frames will be output.  For
//   example, kSbPlayerOutputModePunchOut indicates that the decoded video
//   frames will be output to a background video layer by the platform, and
//   kSbPlayerOutputDecodeToTexture indicates that the decoded video frames
//   should be made available for the application to pull via calls to
//   SbPlayerGetCurrentFrame().
//
// |provider|: Only present in Starboard version 3 and up.  If not |NULL|,
//   then when output_mode == kSbPlayerOutputModeDecodeToTexture, the player MAY
//   use the provider to create SbDecodeTargets on the renderer thread. A
//   provider may not always be needed by the player, but if it is needed, and
//   the provider is not given, the player will fail by returning
//   kSbPlayerInvalid.
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
#if SB_HAS(PLAYER_ERROR_MESSAGE)
               SbPlayerErrorFunc player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
               void* context,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider* context_provider);
// Returns true if the given player output mode is supported by the platform.
// If this function returns true, it is okay to call SbPlayerCreate() with
// the given |output_mode|.
SB_EXPORT bool SbPlayerOutputModeSupported(SbPlayerOutputMode output_mode,
                                           SbMediaVideoCodec codec,
                                           SbDrmSystem drm_system);

#endif  // SB_HAS(PLAYER_WITH_URL)

// Destroys |player|, freeing all associated resources. Each callback must
// receive one more callback to say that the player was destroyed. Callbacks
// may be in-flight when SbPlayerDestroy is called, and should be ignored once
// this function is called.
//
// It is not allowed to pass |player| into any other |SbPlayer| function once
// SbPlayerDestroy has been called on that player.
//
// |player|: The player to be destroyed.
SB_EXPORT void SbPlayerDestroy(SbPlayer player);

// Tells the player to freeze playback (if playback has already started),
// reset or flush the decoder pipeline, and go back to the Prerolling state.
// The player should restart playback once it can display the frame at
// |seek_to_pts|, or the closest it can get. (Some players can only seek to
// I-Frames, for example.)
//
// - Seek must be called before samples are sent when starting playback for
//   the first time, or the client never receives the
//   |kSbPlayerDecoderStateNeedsData| signal.
// - A call to seek may interrupt another seek.
// - After this function is called, the client should not send any more audio
//   or video samples until |SbPlayerDecoderStatusFunc| is called back with
//   |kSbPlayerDecoderStateNeedsData| for each required media type.
//   |SbPlayerDecoderStatusFunc| is the |decoder_status_func| callback function
//   that was specified when the player was created (SbPlayerCreate).
//
// |player|: The SbPlayer in which the seek operation is being performed.
// |seek_to_pts|: The frame at which playback should begin.
// |ticket|: A user-supplied unique ID that is be passed to all subsequent
//   |SbPlayerDecoderStatusFunc| calls. (That is the |decoder_status_func|
//   callback function specified when calling SbPlayerCreate.)
//
//   The |ticket| value is used to filter calls that may have been in flight
//   when SbPlayerSeek was called. To be very specific, once SbPlayerSeek has
//   been called with ticket X, a client should ignore all
//   |SbPlayerDecoderStatusFunc| calls that do not pass in ticket X.
SB_EXPORT void SbPlayerSeek(SbPlayer player,
                            SbMediaTime seek_to_pts,
                            int ticket);

// Writes a single sample of the given media type to |player|'s input stream.
// Its data may be passed in via more than one buffers.  The lifetime of
// |sample_buffers|, |sample_buffer_sizes|, |video_sample_info|, and
// |sample_drm_info| (as well as member |subsample_mapping| contained inside it)
// are not guaranteed past the call to SbPlayerWriteSample. That means that
// before returning, the implementation must synchronously copy any information
// it wants to retain from those structures.
//
// |player|: The player to which the sample is written.
// |sample_type|: The type of sample being written. See the |SbMediaType|
//   enum in media.h.
// |sample_buffers|: A pointer to an array of buffers with
//   |number_of_sample_buffers| elements that hold the data for this sample. The
//   buffers are expected to be a portion of a bytestream of the codec type that
//   the player was created with. The buffers should contain a sequence of whole
//   NAL Units for video, or a complete audio frame.  |sample_buffers| cannot be
//   assumed to live past the call into SbPlayerWriteSample(), so it must be
//   copied if its content will be used after SbPlayerWriteSample() returns.
// |sample_buffer_sizes|: A pointer to an array of sizes with
//   |number_of_sample_buffers| elements.  Each of them specify the number of
//   bytes in the corresponding buffer contained in |sample_buffers|.  None of
//   them can be 0.  |sample_buffer_sizes| cannot be assumed to live past the
//   call into SbPlayerWriteSample(), so it must be copied if its content will
//   be used after SbPlayerWriteSample() returns.
// |number_of_sample_buffers|: Specify the number of elements contained inside
//   |sample_buffers| and |sample_buffer_sizes|.  It has to be at least one, or
//   the call will be ignored.
// |sample_pts|: The timestamp of the sample in 90KHz ticks (PTS). Note that
//   samples MAY be written "slightly" out of order.
// |video_sample_info|: Information about a video sample. This value is
//   required if |sample_type| is |kSbMediaTypeVideo|. Otherwise, it must be
//   |NULL|.
// |sample_drm_info|: The DRM system for the media sample. This value is
//   required for encrypted samples. Otherwise, it must be |NULL|.
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

// Writes a marker to |player|'s input stream of |stream_type| indicating that
// there are no more samples for that media type for the remainder of this
// media stream. This marker is invalidated, along with the rest of the stream's
// contents, after a call to SbPlayerSeek.
//
// |player|: The player to which the marker is written.
// |stream_type|: The type of stream for which the marker is written.
SB_EXPORT void SbPlayerWriteEndOfStream(SbPlayer player,
                                        SbMediaType stream_type);

// Sets the player bounds to the given graphics plane coordinates. The changes
// do not take effect until the next graphics frame buffer swap. The default
// bounds for a player is the full screen.  This function is only relevant when
// the |player| is created with the kSbPlayerOutputModePunchOut output mode, and
// if this is not the case then this function call can be ignored.
//
// This function is called on every graphics frame that changes the video
// bounds. For example, if the video bounds are being animated, then this will
// be called at up to 60 Hz. Since the function could be called up to once per
// frame, implementors should take care to avoid related performance concerns
// with such frequent calls.
//
// |player|: The player that is being resized.
// |z_index|: The z-index of the player.  When the bounds of multiple players
//            are overlapped, the one with larger z-index will be rendered on
//            top of the ones with smaller z-index.
// |x|: The x-coordinate of the upper-left corner of the player.
// |y|: The y-coordinate of the upper-left corner of the player.
// |width|: The width of the player, in pixels.
// |height|: The height of the player, in pixels.
SB_EXPORT void SbPlayerSetBounds(SbPlayer player,
                                 int z_index,
                                 int x,
                                 int y,
                                 int width,
                                 int height);

// Set the playback rate of the |player|.  |rate| is default to 1.0 which
// indicates the playback is at its original speed.  A |rate| greater than one
// will make the playback faster than its original speed.  For example, when
// |rate| is 2, the video will be played at twice the speed as its original
// speed.  A |rate| less than 1.0 will make the playback slower than its
// original speed.  When |rate| is 0, the playback will be paused.
// The function returns true when the playback rate is set to |playback_rate| or
// to a rate that is close to |playback_rate| which the implementation supports.
// It returns false when the playback rate is unchanged, this can happen when
// |playback_rate| is negative or if it is too high to support.
SB_EXPORT bool SbPlayerSetPlaybackRate(SbPlayer player, double playback_rate);

// Sets the player's volume.
//
// |player|: The player in which the volume is being adjusted.
// |volume|: The new player volume. The value must be between |0.0| and |1.0|,
//   inclusive. A value of |0.0| means that the audio should be muted, and a
//   value of |1.0| means that it should be played at full volume.
SB_EXPORT void SbPlayerSetVolume(SbPlayer player, double volume);

// Gets a snapshot of the current player state and writes it to
// |out_player_info|. This function may be called very frequently and is
// expected to be inexpensive.
//
// |player|: The player about which information is being retrieved.
// |out_player_info|: The information retrieved for the player.
SB_EXPORT void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo* out_player_info);

// Given a player created with the kSbPlayerOutputModeDecodeToTexture
// output mode, it will return a SbDecodeTarget representing the current frame
// to be rasterized.  On GLES systems, this function must be called on a
// thread with an EGLContext current, and specifically the EGLContext that will
// be used to eventually render the frame.  If this function is called with a
// |player| object that was created with an output mode other than
// kSbPlayerOutputModeDecodeToTexture, kSbDecodeTargetInvalid is returned.
SB_EXPORT SbDecodeTarget SbPlayerGetCurrentFrame(SbPlayer player);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_PLAYER_H_
