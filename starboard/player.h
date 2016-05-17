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

// An interface for controlling playback of media elementary streams.

#ifndef STARBOARD_PLAYER_H_
#define STARBOARD_PLAYER_H_

#include "starboard/configuration.h"

#if SB_HAS(PLAYER)

#include "starboard/drm.h"
#include "starboard/export.h"
#include "starboard/media.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Types -----------------------------------------------------------------

// An indicator of whether the decoder can accept more samples.
typedef enum SbPlayerDecoderState {
  // The decoder is ready for and eager to receive more samples.  Note that
  // once the decoder notifies the user with kSbPlayerDecoderStateNeedsData,
  // it cannot enter kSbPlayerDecoderStateBufferFull before SbPlayerWriteSample
  // or SbPlayerWriteEndOfStream is called.
  kSbPlayerDecoderStateNeedsData,

  // The decoder is not ready for any more samples, so do not send them.
  kSbPlayerDecoderStateBufferFull,

  // The player has been destroyed, and will send no more callbacks.
  kSbPlayerDecoderStateDestroyed,
} SbPlayerDecoderState;

// An indicator of the general playback state.
typedef enum SbPlayerState {
  // The player has just been initialized.  It is expecting an SbPlayerSeek()
  // call to enter the prerolling state.
  kSbPlayerStateInitialized,

  // The player is prerolling, collecting enough data to fill the pipeline
  // before presentation starts. After the first preroll is completed, there
  // should always be a video frame to render, even if the player goes back to
  // Prerolling after a Seek.
  kSbPlayerStatePrerolling,

  // The player is presenting media, and it is either actively playing in
  // real-time, or it is paused.
  kSbPlayerStatePresenting,

  // The player is presenting media, but it is paused at the end of the stream.
  kSbPlayerStateEndOfStream,

  // The player has been destroyed, and will send no more callbacks.
  kSbPlayerStateDestroyed,

  // The player encounters an error.  It is expecting an SbPlayerDestroy() call
  // to tear down the player.  Any call to other functions maybe ignored and
  // callbacks may not be triggered.
  kSbPlayerStateError,
} SbPlayerState;

// Information about the current media playback state.
typedef struct SbPlayerInfo {
  // The position of the playback head, as precisely as possible, in 90KHz ticks
  // (PTS).
  SbMediaTime current_media_pts;

  // The known duration of the currently playing media stream, in 90KHz ticks
  // (PTS).
  SbMediaTime duration_pts;

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
// within this callback.
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

// Callback to free the given sample buffer data.
typedef void (*SbPlayerDeallocateSampleFunc)(SbPlayer player,
                                             void* context,
                                             void* sample_buffer);

#if SB_IS(PLAYER_COMPOSITED)
// A handle that can be used to compose a player's video output with other
// composition layers.
// TODO(***REMOVED***): Define a SbCompositor interface with a composition handle type.
typedef uint32_t SbPlayerCompositionHandle;
#endif

// --- Constants -------------------------------------------------------------

// The value to pass into SbPlayerCreate's |duration_ptr| argument for cases
// where the duration is unknown, such as for live streams.
#define SB_PLAYER_NO_DURATION (-1)

// The value of the initial ticket held by the player before the first seek.
// The player will use this ticket value to make the first call to
// SbPlayerStatusFunc with kSbPlayerStateInitialized.
#define SB_PLAYER_INITIAL_TICKET (0)

// Well-defined value for an invalid player.
#define kSbPlayerInvalid ((SbPlayer)NULL)

// Returns whether the given player handle is valid.
SB_C_INLINE bool SbPlayerIsValid(SbPlayer player) {
  return player != kSbPlayerInvalid;
}

// --- Functions -------------------------------------------------------------

// Creates a player for the specified |video_codec| and |audio_codec|, acquiring
// all resources needed to operate it, and returning an opaque handle to it. If
// |video_codec| is kSbMediaVideoCodecNone, the player is an audio-only
// player. Otherwise, the player is an audio/video decoder. |audio_codec| should
// never be kSbMediaAudioCodecNone. The expectation is that a new player will be
// created and destroyed for every playback.
//
// |duration_pts| is the expected media duration in 90KHz ticks (PTS). It may be
// set to SB_PLAYER_NO_DURATION for live streams.
//
// If the media stream has encrypted portions, then an appropriate DRM session
// must first be created with SbDrmCreateSession() and passed into |drm|. If
// not, then |drm| may be SB_DRM_INVALID_SESSION.
//
// If |audio_codec| is kSbMediaAudioCodecAac, then the caller must provide a
// populated |audio_header|. Otherwise, this may be NULL.
//
// If not NULL, the player will call |sample_deallocator_func| on an internal
// thread to free the sample buffers passed into SbPlayerWriteSample().
//
// If not NULL, the decoder will call |decoder_status_func| on an internal
// thread to provide an update on the decoder's status. No work should be done
// on this thread, it should just signal the client thread interacting with the
// decoder.
//
// If not NULL, the player will call |player_status_func| on an internal thread
// to provide an update on the playback status. No work should be done on this
// thread, it should just signal the client thread interacting with the decoder.
//
// |context| will be passed back into all callbacks, and is generally used to
// point at a class or struct that contains state associated with the player.
//
// The associated decoder of the returned player should be assumed to be in
// kSbPlayerDecoderStateBufferFull until SbPlayerSeek() has been called on it.
//
// It is expected that the thread that calls SbPlayerCreate is the same thread
// that calls the other SbPlayer functions for that player, or that there is a
// mutex guarding calls into each SbPlayer instance.
//
// If there is a platform limitation on how many players can coexist
// simultaneously, then calls made to this function that attempt to exceed that
// limit will return kSbPlayerInvalid.
SB_EXPORT SbPlayer
SbPlayerCreate(SbMediaVideoCodec video_codec,
               SbMediaAudioCodec audio_codec,
               SbMediaTime duration_pts,
               SbDrmSession drm,
               SbMediaAudioHeader* audio_header,
               SbPlayerDeallocateSampleFunc sample_deallocate_func,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               void* context);

// Destroys |player|, freeing all associated resources. Each callback must
// receive one more callback to say that the player was destroyed.  Callbacks
// may be in-flight when SbPlayerDestroy is called, and should be ignored once
// this function is called.
//
// It is not allowed to pass |player| into any other SbPlayer function once
// SbPlayerDestroy has been called on it.
SB_EXPORT void SbPlayerDestroy(SbPlayer player);

// Tells the player to freeze playback where it is (if it has already started),
// reset/flush the decoder pipeline, and go back to the Prerolling state. The
// player should restart playback once it can display the frame at
// |seek_to_pts|, or the closest it can get (some players can only seek to
// I-Frames, for example). The client should send no more audio or video samples
// until SbPlayerDecoderStatusFunc is called back with
// kSbPlayerDecoderStateNeedsData, for each required media type.
//
// A call to seek may interrupt another seek.
//
// |ticket| is a user-supplied unique ID that will be passed to all subsequent
// SbPlayerDecoderStatusFunc calls. This is used to filter calls that may have
// been in flight when SbPlayerSeek is called. To be very specific, once
// SbPlayerSeek has been called with ticket X, a client should ignore all
// SbPlayerDecoderStatusFunc calls that don't pass in ticket X.
//
// Seek must also be called before samples are sent when starting playback for
// the first time, or the client will never receive the
// kSbPlayerDecoderStateNeedsData signal.
SB_EXPORT void SbPlayerSeek(SbPlayer player,
                            SbMediaTime seek_to_pts,
                            int ticket);

// Writes a sample of the given media type to |player|'s input stream.
// |sample_type| is the type of sample, audio or video. |sample_buffer| is a
// pointer to a buffer with the data for this sample. This buffer is expected to
// be a portion of a H.264 bytestream, containing a sequence of whole NAL Units
// for video, or a complete audio frame. |sample_buffer_size| is the number of
// bytes in the given sample. |sample_pts| is the timestamp of the sample in
// 90KHz ticks (PTS), and samples MAY be written "slightly" out-of-order.
// |video_sample_info| must be provided for every call where |sample_type| is
// kSbMediaTypeVideo, and must be NULL otherwise.  |sample_drm_info| must be
// provided for encrypted samples, and must be NULL otherwise.
//
// The lifetime of |video_sample_info| and |sample_drm_info| are not guaranteed
// past the call to SbPlayerWriteSample, so the implementation must copy any
// information it wants to retain from those structures synchronously, before it
// returns.
SB_EXPORT void SbPlayerWriteSample(SbPlayer player,
                                   SbMediaType sample_type,
                                   void* sample_buffer,
                                   int sample_buffer_size,
                                   SbMediaTime sample_pts,
                                   SbMediaVideoSampleInfo* video_sample_info,
                                   SbDrmSampleInfo* sample_drm_info);

// Writes a marker to |player|'s input stream of |stream_type| that there are no
// more samples for that media type for the remainder of this media stream.
// This marker is invalidated, along with the rest of the contents of the
// stream, after a call to SbPlayerSeek.
SB_EXPORT void SbPlayerWriteEndOfStream(SbPlayer player,
                                        SbMediaType stream_type);

#if SB_IS(PLAYER_PUNCHED_OUT)
// Sets the player bounds to the given graphics plane coordinates. Will not take
// effect until the next graphics frame buffer swap. The default bounds for a
// player are the full screen.
SB_EXPORT void SbPlayerSetBounds(SbPlayer player,
                                 int x,
                                 int y,
                                 int width,
                                 int height);
#endif

// Sets the pause status for |player|. If |player| is in kPlayerStatePrerolling,
// this will set the initial pause state for the current seek target.
SB_EXPORT void SbPlayerSetPause(SbPlayer player, bool pause);

// Sets the volume for |player|, in [0.0, 1.0]. 0.0 means the audio should be
// muted, and 1.0 means it should be played at full volume.
SB_EXPORT void SbPlayerSetVolume(SbPlayer player, double volume);

// Gets a snapshot of the current player state and writes it into
// |out_player_info|. This function is expected to be inexpensive, and may be
// called very frequently.
SB_EXPORT void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo* out_player_info);

#if SB_IS(PLAYER_COMPOSITED)
// Gets a handle that represents the player's video output, for the purpose of
// composing with SbCompositor (currently undefined).
SB_EXPORT SbPlayerCompositionHandle
SbPlayerGetCompositionHandle(SbPlayer player);
#endif

#if SB_IS(PLAYER_PRODUCING_TEXTURE)
// Gets an OpenGL texture ID that points to the player's video output frame at
// the time it was called. This can be called once, and the same texture ID will
// be appropriately mapped to the current video frame when drawn.
SB_EXPORT uint32_t SbPlayerGetTextureId(SbPlayer player);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SB_HAS(PLAYER)

#endif  // STARBOARD_PLAYER_H_
