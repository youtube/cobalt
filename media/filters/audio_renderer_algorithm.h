// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioRendererAlgorithm buffers and transforms audio data. The owner of
// this object provides audio data to the object through EnqueueBuffer() and
// requests data from the buffer via FillBuffer(). The owner also sets the
// playback rate, and the AudioRendererAlgorithm will stretch or compress the
// buffered audio as necessary to match the playback rate when fulfilling
// FillBuffer() requests. AudioRendererAlgorithm can request more data to be
// buffered via a read callback passed in during initialization.
//
// This class is *not* thread-safe. Calls to enqueue and retrieve data must be
// locked if called from multiple threads.
//
// AudioRendererAlgorithm uses a simple pitch-preservation algorithm to
// stretch and compress audio data to meet playback speeds less than and
// greater than the natural playback of the audio stream.
//
// Audio at very low or very high playback rates are muted to preserve quality.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "media/base/seekable_buffer.h"

namespace media {

class Buffer;

class MEDIA_EXPORT AudioRendererAlgorithm {
 public:
  AudioRendererAlgorithm();
  ~AudioRendererAlgorithm();

  // Call prior to Initialize() to validate configuration.  Returns false if the
  // configuration is invalid.  Detailed error information will be DVLOG'd.
  static bool ValidateConfig(int channels,
                             int samples_per_second,
                             int bits_per_channel);

  // Initializes this object with information about the audio stream.
  // |samples_per_second| is in Hz. |read_request_callback| is called to
  // request more data from the client, requests that are fulfilled through
  // calls to EnqueueBuffer().
  void Initialize(int channels,
                  int samples_per_second,
                  int bits_per_channel,
                  float initial_playback_rate,
                  const base::Closure& request_read_cb);

  // Tries to fill |requested_frames| frames into |dest| with possibly scaled
  // data from our |audio_buffer_|. Data is scaled based on the playback rate,
  // using a variation of the Overlap-Add method to combine sample windows.
  //
  // Data from |audio_buffer_| is consumed in proportion to the playback rate.
  //
  // Returns the number of frames copied into |dest|.
  // May request more reads via |request_read_cb_| before returning.
  int FillBuffer(uint8* dest, int requested_frames);

  // Clears |audio_buffer_|.
  void FlushBuffers();

  // Returns the time of the next byte in our data or kNoTimestamp() if current
  // time is unknown.
  base::TimeDelta GetTime();

  // Enqueues a buffer. It is called from the owner of the algorithm after a
  // read completes.
  void EnqueueBuffer(Buffer* buffer_in);

  float playback_rate() const { return playback_rate_; }
  void SetPlaybackRate(float new_rate);

  // Returns whether the algorithm has enough data at the current playback rate
  // such that it can write data on the next call to FillBuffer().
  bool CanFillBuffer();

  // Returns true if |audio_buffer_| is at or exceeds capacity.
  bool IsQueueFull();

  // Returns the capacity of |audio_buffer_|.
  int QueueCapacity();

  // Increase the capacity of |audio_buffer_| if possible.
  void IncreaseQueueCapacity();

  // Returns the number of bytes left in |audio_buffer_|, which may be larger
  // than QueueCapacity() in the event that a read callback delivered more data
  // than |audio_buffer_| was intending to hold.
  int bytes_buffered() { return audio_buffer_.forward_bytes(); }

  int bytes_per_frame() { return bytes_per_frame_; }

  int bytes_per_channel() { return bytes_per_channel_; }

  bool is_muted() { return muted_; }

 private:
  // Fills |dest| with one frame of audio data at normal speed. Returns true if
  // a frame was rendered, false otherwise.
  bool OutputNormalPlayback(uint8* dest);

  // Fills |dest| with one frame of audio data at faster than normal speed.
  // Returns true if a frame was rendered, false otherwise.
  //
  // When the audio playback is > 1.0, we use a variant of Overlap-Add to squish
  // audio output while preserving pitch. Essentially, we play a bit of audio
  // data at normal speed, then we "fast forward" by dropping the next bit of
  // audio data, and then we stich the pieces together by crossfading from one
  // audio chunk to the next.
  bool OutputFasterPlayback(uint8* dest);

  // Fills |dest| with one frame of audio data at slower than normal speed.
  // Returns true if a frame was rendered, false otherwise.
  //
  // When the audio playback is < 1.0, we use a variant of Overlap-Add to
  // stretch audio output while preserving pitch. This works by outputting a
  // segment of audio data at normal speed. The next audio segment then starts
  // by repeating some of the audio data from the previous audio segment.
  // Segments are stiched together by crossfading from one audio chunk to the
  // next.
  bool OutputSlowerPlayback(uint8* dest);

  // Resets the window state to the start of a new window.
  void ResetWindow();

  // Copies a raw frame from |audio_buffer_| into |dest| without progressing
  // |audio_buffer_|'s internal "current" cursor. Optionally peeks at a forward
  // byte |offset|.
  void CopyWithoutAdvance(uint8* dest);
  void CopyWithoutAdvance(uint8* dest, int offset);

  // Copies a raw frame from |audio_buffer_| into |dest| and progresses the
  // |audio_buffer_| forward.
  void CopyWithAdvance(uint8* dest);

  // Moves the |audio_buffer_| forward by one frame.
  void DropFrame();

  // Does a linear crossfade from |intro| into |outtro| for one frame.
  // Assumes pointers are valid and are at least size of |bytes_per_frame_|.
  void OutputCrossfadedFrame(uint8* outtro, const uint8* intro);
  template <class Type>
  void CrossfadeFrame(uint8* outtro, const uint8* intro);

  // Rounds |*value| down to the nearest frame boundary.
  void AlignToFrameBoundary(int* value);

  // Number of channels in audio stream.
  int channels_;

  // Sample rate of audio stream.
  int samples_per_second_;

  // Byte depth of audio.
  int bytes_per_channel_;

  // Used by algorithm to scale output.
  float playback_rate_;

  // Used to request more data.
  base::Closure request_read_cb_;

  // Buffered audio data.
  SeekableBuffer audio_buffer_;

  // Length for crossfade in bytes.
  int bytes_in_crossfade_;

  // Length of frame in bytes.
  int bytes_per_frame_;

  // The current location in the audio window, between 0 and |window_size_|.
  // When |index_into_window_| reaches |window_size_|, the window resets.
  // Indexed by byte.
  int index_into_window_;

  // The frame number in the crossfade.
  int crossfade_frame_number_;

  // True if the audio should be muted.
  bool muted_;

  bool needs_more_data_;

  // Temporary buffer to hold crossfade data.
  scoped_array<uint8> crossfade_buffer_;

  // Window size, in bytes (calculated from audio properties).
  int window_size_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererAlgorithm);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_ALGORITHM_H_
