// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#if SB_API_VERSION < 12
  // The decoder is not ready for any more samples, so do not send them.  Note
  // that this enum value has been deprecated and the SbPlayer implementation
  // should no longer use this value.
  kSbPlayerDecoderStateBufferFull,
  // The player has been destroyed, and will send no more callbacks.
  kSbPlayerDecoderStateDestroyed,
#endif  // SB_API_VERSION < 12
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

  // The player is presenting media, and it is either paused or actively playing
  // in real-time. Note that the implementation should use this state to signal
  // that the preroll has been finished.
  kSbPlayerStatePresenting,

  // The player is presenting media, but it is paused at the end of the stream.
  kSbPlayerStateEndOfStream,

  // The player has been destroyed, and will send no more callbacks.
  kSbPlayerStateDestroyed,
} SbPlayerState;

typedef enum SbPlayerError {
  kSbPlayerErrorDecode,
  // The playback capability of the player has changed, likely because of a
  // change of the system environment.  For example, the system may support vp9
  // decoding with an external GPU.  When the external GPU is detached, this
  // error code can signal the app to retry the playback, possibly with h264.
  kSbPlayerErrorCapabilityChanged,
  // The max value of SbPlayer error type. It should always at the bottom
  // of SbPlayerError and never be used.
  kSbPlayerErrorMax,
} SbPlayerError;

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

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

// The playback related parameters to pass into SbPlayerCreate() and
// SbPlayerGetPreferredOutputMode().
typedef struct SbPlayerCreationParam {
  // Provides an appropriate DRM system if the media stream has encrypted
  // portions.  It will be |kSbDrmSystemInvalid| if the stream does not have
  // encrypted portions.
  SbDrmSystem drm_system;
  // Contains a populated SbMediaAudioSampleInfo if |audio_sample_info.codec|
  // isn't |kSbMediaAudioCodecNone|.  When |audio_sample_info.codec| is
  // |kSbMediaAudioCodecNone|, the video doesn't have an audio track.
  SbMediaAudioSampleInfo audio_sample_info;
  // Contains a populated SbMediaVideoSampleInfo if |video_sample_info.codec|
  // isn't |kSbMediaVideoCodecNone|.  When |video_sample_info.codec| is
  // |kSbMediaVideoCodecNone|, the video is audio only.
  SbMediaVideoSampleInfo video_sample_info;
  // Selects how the decoded video frames will be output.  For example,
  // |kSbPlayerOutputModePunchOut| indicates that the decoded video frames will
  // be output to a background video layer by the platform, and
  // |kSbPlayerOutputDecodeToTexture| indicates that the decoded video frames
  // should be made available for the application to pull via calls to
  // SbPlayerGetCurrentFrame().
  SbPlayerOutputMode output_mode;
} SbPlayerCreationParam;

#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

// Identify the type of side data accompanied with |SbPlayerSampleInfo|, as side
// data may come from multiple sources.
typedef enum SbPlayerSampleSideDataType {
  // The side data comes from the BlockAdditional data in the Matroska/Webm
  // container, as specified in
  // https://tools.ietf.org/id/draft-lhomme-cellar-matroska-03.html#rfc.section.7.3.39
  // and https://www.webmproject.org/docs/container/#BlockAdditional.
  // The first 8 bytes of the data contains the value of BlockAddID in big
  // endian format, followed by the content of BlockAdditional.
  kMatroskaBlockAdditional,
} SbPlayerSampleSideDataType;

// Side data accompanied with |SbPlayerSampleInfo|, it can be arbitrary binary
// data coming from multiple sources.
typedef struct SbPlayerSampleSideData {
  SbPlayerSampleSideDataType type;
  // |data| will remain valid until SbPlayerDeallocateSampleFunc() is called on
  // the |SbPlayerSampleInfo::buffer| the data is associated with.
  const uint8_t* data;
  // The size of the data pointed by |data|, in bytes.
  size_t size;
} SbPlayerSampleSideData;

// Information about the samples to be written into SbPlayerWriteSample2.
typedef struct SbPlayerSampleInfo {
  SbMediaType type;
  // Points to the buffer containing the sample data.
  const void* buffer;
  // Size of the data pointed to by |buffer|.
  int buffer_size;
  // The timestamp of the sample in SbTime.
  SbTime timestamp;

  // Points to an array of side data for the input, when available.
  SbPlayerSampleSideData* side_data;
  // The number of side data pointed by |side_data|.  It should be set to 0 if
  // there is no side data for the input.
  int side_data_count;

  union {
    // Information about an audio sample. This value can only be used when
    // |type| is kSbMediaTypeAudio.
    SbMediaAudioSampleInfo audio_sample_info;
    // Information about a video sample. This value can only be used when |type|
    // is kSbMediaTypeVideo.
    SbMediaVideoSampleInfo video_sample_info;
  };

  // The DRM system related info for the media sample. This value is required
  // for encrypted samples. Otherwise, it must be |NULL|.
  const SbDrmSampleInfo* drm_info;
} SbPlayerSampleInfo;

// Information about the current media playback state.
typedef struct SbPlayerInfo2 {
  // The position of the playback head, as precisely as possible, in
  // microseconds.
  SbTime current_media_timestamp;

  // The known duration of the currently playing media stream, in microseconds.
  SbTime duration;

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
} SbPlayerInfo2;

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

// Callback for player errors, that may set a |message|.
// |error|: indicates the error code.
// |message|: provides specific informative diagnostic message about the error
//            condition encountered. It is ok for the message to be an empty
//            string or NULL if no information is available.
typedef void (*SbPlayerErrorFunc)(SbPlayer player,
                                  void* context,
                                  SbPlayerError error,
                                  const char* message);

// Callback to free the given sample buffer data.  When more than one buffer
// are sent in SbPlayerWriteSample(), the implementation only has to call this
// callback with |sample_buffer| points to the the first buffer.
typedef void (*SbPlayerDeallocateSampleFunc)(SbPlayer player,
                                             void* context,
                                             const void* sample_buffer);

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
//   that limit must return |kSbPlayerInvalid|. Multiple calls to SbPlayerCreate
//   must not cause a crash.
//
// |window|: The window that will display the player. |window| can be
//   |kSbWindowInvalid| for platforms where video is only displayed on a
//   particular window that the underlying implementation already has access to.
//
// |video_codec|: The video codec used for the player. If |video_codec| is
//   |kSbMediaVideoCodecNone|, the player is an audio-only player. If
//   |video_codec| is any other value, the player is an audio/video decoder.
//   This can be set to |kSbMediaVideoCodecNone| to play a video with only an
//   audio track.
//
// |audio_codec|: The audio codec used for the player. The caller must provide a
//   populated |audio_sample_info| if audio codec is |kSbMediaAudioCodecAac|.
//   Can be set to |kSbMediaAudioCodecNone| to play a video without any audio
//   track.  In such case |audio_sample_info| must be NULL.
//
// |drm_system|: If the media stream has encrypted portions, then this
//   parameter provides an appropriate DRM system, created with
//   |SbDrmCreateSystem()|. If the stream does not have encrypted portions,
//   then |drm_system| may be |kSbDrmSystemInvalid|.
//
// |audio_sample_info|: Note that the caller must provide a populated
//   |audio_sample_info| if the audio codec is |kSbMediaAudioCodecAac|.
//   Otherwise, |audio_sample_info| can be NULL. See media.h for the format of
//   the |SbMediaAudioSampleInfo| struct.
//
//   Note that |audio_specific_config| is a pointer and the content it points to
//   is no longer valid after this function returns.  The implementation has to
//   make a copy of the content if it is needed after the function returns.
//
// |max_video_capabilities|: This string communicates the max video capabilities
//   required to the platform. The web app will not provide a video stream
//   exceeding the maximums described by this parameter. Allows the platform to
//   optimize playback pipeline for low quality video streams if it knows that
//   it will never adapt to higher quality streams. The string uses the same
//   format as the string passed in to SbMediaCanPlayMimeAndKeySystem(), for
//   example, when it is set to "width=1920; height=1080; framerate=15;", the
//   video will never adapt to resolution higher than 1920x1080 or frame per
//   second higher than 15 fps. When the maximums are unknown, this will be set
//   to NULL.

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
// |player_error_func|: If not |NULL|, the player calls this function on an
//   internal thread to provide an update on the error status. This callback is
//   responsible for setting the media error message.
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
//   |kSbPlayerInvalid|.
//
// If |NULL| is passed to any of the callbacks (|sample_deallocator_func|,
// |decoder_status_func|, |player_status_func|, or |player_error_func| if it
// applies), then |kSbPlayerInvalid| must be returned.

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

SB_EXPORT SbPlayer
SbPlayerCreate(SbWindow window,
               const SbPlayerCreationParam* creation_param,
               SbPlayerDeallocateSampleFunc sample_deallocate_func,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               SbPlayerErrorFunc player_error_func,
               void* context,
               SbDecodeTargetGraphicsContextProvider* context_provider);

#else  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

SB_EXPORT SbPlayer
SbPlayerCreate(SbWindow window,
               SbMediaVideoCodec video_codec,
               SbMediaAudioCodec audio_codec,
               SbDrmSystem drm_system,
               const SbMediaAudioSampleInfo* audio_sample_info,
               const char* max_video_capabilities,
               SbPlayerDeallocateSampleFunc sample_deallocate_func,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               SbPlayerErrorFunc player_error_func,
               void* context,
               SbPlayerOutputMode output_mode,
               SbDecodeTargetGraphicsContextProvider* context_provider);

#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
// Returns the preferred output mode of the implementation when a video
// described by |creation_param| is played.  It is assumed that it is okay to
// call SbPlayerCreate() with the same video described by |creation_param|,
// with its |output_mode| member replaced by the returned output mode.
// When the caller has no preference on the output mode, it will set
// |creation_param->output_mode| to |kSbPlayerOutputModeInvalid|, and the
// implementation can return its preferred output mode based on the information
// contained in |creation_param|.  The caller can also set
// |creation_param->output_mode| to its preferred output mode, and the
// implementation should return the same output mode if it is supported,
// otherwise the implementation should return an output mode that it is
// supported, as if |creation_param->output_mode| is set to
// |kSbPlayerOutputModeInvalid| prior to the call.
// Note that it is not the responsibility of this function to verify whether the
// video described by |creation_param| can be played on the platform, and the
// implementation should try its best effort to return a valid output mode.
// |creation_param| will never be NULL.
SB_EXPORT SbPlayerOutputMode
SbPlayerGetPreferredOutputMode(const SbPlayerCreationParam* creation_param);

#else   // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
// Returns true if the given player output mode is supported by the platform.
// If this function returns true, it is okay to call SbPlayerCreate() with
// the given |output_mode|.
SB_EXPORT bool SbPlayerOutputModeSupported(SbPlayerOutputMode output_mode,
                                           SbMediaVideoCodec codec,
                                           SbDrmSystem drm_system);
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

// Destroys |player|, freeing all associated resources.
//  * Upon calling this method, there should be one call to the player status
//    callback (i.e. |player_status_func| used in the creation of the player)
//    which indicates the player is destroyed. Note, the callback has to be
//    in-flight when SbPlayerDestroyed is called.
//  * No more other callbacks should be issued after this function returns.
//  * It is not allowed to pass |player| into any other |SbPlayer| function
//    once SbPlayerDestroy has been called on that player.
// |player|: The player to be destroyed.
SB_EXPORT void SbPlayerDestroy(SbPlayer player);

// SbPlayerSeek2 is like the deprecated SbPlayerSeek, but accepts SbTime
// |seek_to_timestamp| instead of SbMediaTime |seek_to_pts|.

// Tells the player to freeze playback (if playback has already started),
// reset or flush the decoder pipeline, and go back to the Prerolling state.
// The player should restart playback once it can display the frame at
// |seek_to_timestamp|, or the closest it can get. (Some players can only seek
// to I-Frames, for example.)
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
// |seek_to_timestamp|: The frame at which playback should begin.
// |ticket|: A user-supplied unique ID that is be passed to all subsequent
//   |SbPlayerDecoderStatusFunc| calls. (That is the |decoder_status_func|
//   callback function specified when calling SbPlayerCreate.)
//
//   The |ticket| value is used to filter calls that may have been in flight
//   when SbPlayerSeek2 was called. To be very specific, once SbPlayerSeek2 has
//   been called with ticket X, a client should ignore all
//   |SbPlayerDecoderStatusFunc| calls that do not pass in ticket X.

SB_EXPORT void SbPlayerSeek2(SbPlayer player,
                             SbTime seek_to_timestamp,
                             int ticket);

// Writes samples of the given media type to |player|'s input stream. The
// lifetime of |sample_infos|, and the members of its elements like |buffer|,
// |video_sample_info|, and |drm_info| (as well as member |subsample_mapping|
// contained inside it) are not guaranteed past the call to
// SbPlayerWriteSample2. That means that before returning, the implementation
// must synchronously copy any information it wants to retain from those
// structures.
//
// SbPlayerWriteSample2 allows writing of multiple samples in one call.
//
// |player|: The player to which the sample is written.
// |sample_type|: The type of sample being written. See the |SbMediaType|
//   enum in media.h.
// |sample_infos|: A pointer to an array of SbPlayerSampleInfo with
//   |number_of_sample_infos| elements, each holds the data for an sample, i.e.
//   a sequence of whole NAL Units for video, or a complete audio frame.
//   |sample_infos| cannot be assumed to live past the call into
//   SbPlayerWriteSample2(), so it must be copied if its content will be used
//   after SbPlayerWriteSample2() returns.
// |number_of_sample_infos|: Specify the number of samples contained inside
//   |sample_infos|.  It has to be at least one, and less than the return value
//   of SbPlayerGetMaximumNumberOfSamplesPerWrite().
SB_EXPORT void SbPlayerWriteSample2(SbPlayer player,
                                    SbMediaType sample_type,
                                    const SbPlayerSampleInfo* sample_infos,
                                    int number_of_sample_infos);

// Writes a single sample of the given media type to |player|'s input stream.
// Its data may be passed in via more than one buffers.  The lifetime of
// |sample_buffers|, |sample_buffer_sizes|, |video_sample_info|, and
// |sample_drm_info| (as well as member |subsample_mapping| contained inside it)
// are not guaranteed past the call to SbPlayerWriteSample. That means that
// before returning, the implementation must synchronously copy any information
// it wants to retain from those structures.
//
// |player|: The player for which the number is retrieved.
// |sample_type|: The type of sample for which the number is retrieved. See the
//   |SbMediaType| enum in media.h.
SB_EXPORT int SbPlayerGetMaximumNumberOfSamplesPerWrite(
    SbPlayer player,
    SbMediaType sample_type);

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

// SbPlayerGetInfo2 is like the deprecated SbPlayerGetInfo, but accepts
// SbPlayerInfo2* |out_player_info2| instead of SbPlayerInfo |out_player_info|.

// Gets a snapshot of the current player state and writes it to
// |out_player_info|. This function may be called very frequently and is
// expected to be inexpensive.
//
// |player|: The player about which information is being retrieved.
// |out_player_info|: The information retrieved for the player.
SB_EXPORT void SbPlayerGetInfo2(SbPlayer player,
                                SbPlayerInfo2* out_player_info2);

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
