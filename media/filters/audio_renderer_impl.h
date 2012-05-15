// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Audio rendering unit utilizing an AudioRendererSink to output data.
//
// This class lives inside three threads during it's lifetime, namely:
// 1. Render thread.
//    This object is created on the render thread.
// 2. Pipeline thread
//    Initialize() is called here with the audio format.
//    Play/Pause/Seek also happens here.
// 3. Audio thread created by the AudioRendererSink.
//    Render() is called here where audio data is decoded into raw PCM data.
//
// AudioRendererImpl talks to an AudioRendererAlgorithm that takes care of
// queueing audio data and stretching/shrinking audio data when playback rate !=
// 1.0 or 0.0.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_

#include <deque>

#include "base/synchronization/lock.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/buffers.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_algorithm.h"

namespace media {

class MEDIA_EXPORT AudioRendererImpl
    : public AudioRenderer,
      NON_EXPORTED_BASE(public media::AudioRendererSink::RenderCallback) {
 public:
  // Methods called on Render thread ------------------------------------------
  // An AudioRendererSink is used as the destination for the rendered audio.
  explicit AudioRendererImpl(media::AudioRendererSink* sink);
  virtual ~AudioRendererImpl();

  // Methods called on pipeline thread ----------------------------------------
  // Filter implementation.
  virtual void Play(const base::Closure& callback) OVERRIDE;
  virtual void Pause(const base::Closure& callback) OVERRIDE;
  virtual void Flush(const base::Closure& callback) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void SetPlaybackRate(float rate) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& cb) OVERRIDE;

  // AudioRenderer implementation.
  virtual void Initialize(const scoped_refptr<AudioDecoder>& decoder,
                          const PipelineStatusCB& init_cb,
                          const base::Closure& underflow_cb,
                          const TimeCB& time_cb) OVERRIDE;
  virtual bool HasEnded() OVERRIDE;
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) OVERRIDE;
  virtual void SetVolume(float volume) OVERRIDE;

 private:
  friend class AudioRendererImplTest;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, EndOfStream);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, Underflow_EndOfStream);

  // Callback from the audio decoder delivering decoded audio samples.
  void DecodedAudioReady(scoped_refptr<Buffer> buffer);

  // Fills the given buffer with audio data by delegating to its |algorithm_|.
  // FillBuffer() also takes care of updating the clock. Returns the number of
  // frames copied into |dest|, which may be less than or equal to
  // |requested_frames|.
  //
  // If this method returns fewer frames than |requested_frames|, it could
  // be a sign that the pipeline is stalled or unable to stream the data fast
  // enough.  In such scenarios, the callee should zero out unused portions
  // of their buffer to playback silence.
  //
  // FillBuffer() updates the pipeline's playback timestamp. If FillBuffer() is
  // not called at the same rate as audio samples are played, then the reported
  // timestamp in the pipeline will be ahead of the actual audio playback. In
  // this case |playback_delay| should be used to indicate when in the future
  // should the filled buffer be played. If FillBuffer() is called as the audio
  // hardware plays the buffer, then |playback_delay| should be zero.
  //
  // FillBuffer() calls SignalEndOfStream() when it reaches end of stream.
  //
  // Safe to call on any thread.
  uint32 FillBuffer(uint8* dest,
                    uint32 requested_frames,
                    const base::TimeDelta& playback_delay);

  // Called at the end of stream when all the hardware buffers become empty
  // (i.e. when all the data written to the device has been played).
  void SignalEndOfStream();

  // Get the playback rate of |algorithm_|.
  float GetPlaybackRate();

  // Convert number of bytes to duration of time using information about the
  // number of channels, sample rate and sample bits.
  base::TimeDelta ConvertToDuration(int bytes);

  // Estimate earliest time when current buffer can stop playing.
  void UpdateEarliestEndTime(int bytes_filled,
                             base::TimeDelta request_delay,
                             base::Time time_now);

  // Methods called on pipeline thread ----------------------------------------
  void DoPlay();
  void DoPause();
  void DoSeek();

  // media::AudioRendererSink::RenderCallback implementation.
  virtual int Render(const std::vector<float*>& audio_data,
                     int number_of_frames,
                     int audio_delay_milliseconds) OVERRIDE;
  virtual void OnRenderError() OVERRIDE;

  // Helper method that schedules an asynchronous read from the decoder and
  // increments |pending_reads_|.
  //
  // Safe to call from any thread.
  void ScheduleRead_Locked();

  // Returns true if the data in the buffer is all before
  // |seek_timestamp_|. This can only return true while
  // in the kSeeking state.
  bool IsBeforeSeekTime(const scoped_refptr<Buffer>& buffer);

  // Audio decoder.
  scoped_refptr<AudioDecoder> decoder_;

  // Algorithm for scaling audio.
  scoped_ptr<AudioRendererAlgorithm> algorithm_;

  base::Lock lock_;

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kPaused,
    kSeeking,
    kPlaying,
    kStopped,
    kUnderflow,
    kRebuffering,
  };
  State state_;

  // Keep track of our outstanding read to |decoder_|.
  bool pending_read_;

  // Keeps track of whether we received and rendered the end of stream buffer.
  bool received_end_of_stream_;
  bool rendered_end_of_stream_;

  // The timestamp of the last frame (i.e. furthest in the future) buffered as
  // well as the current time that takes current playback delay into account.
  base::TimeDelta audio_time_buffered_;
  base::TimeDelta current_time_;

  // Filter callbacks.
  base::Closure pause_cb_;
  PipelineStatusCB seek_cb_;

  base::Closure underflow_cb_;

  TimeCB time_cb_;

  base::TimeDelta seek_timestamp_;

  uint32 bytes_per_frame_;

  // Used to calculate audio delay given bytes.
  uint32 bytes_per_second_;

  // A flag that indicates this filter is called to stop.
  bool stopped_;

  // The sink (destination) for rendered audio.
  scoped_refptr<media::AudioRendererSink> sink_;

  // Set to true when OnInitialize() is called.
  bool is_initialized_;

  // We're supposed to know amount of audio data OS or hardware buffered, but
  // that is not always so -- on my Linux box
  // AudioBuffersState::hardware_delay_bytes never reaches 0.
  //
  // As a result we cannot use it to find when stream ends. If we just ignore
  // buffered data we will notify host that stream ended before it is actually
  // did so, I've seen it done ~140ms too early when playing ~150ms file.
  //
  // Instead of trying to invent OS-specific solution for each and every OS we
  // are supporting, use simple workaround: every time we fill the buffer we
  // remember when it should stop playing, and do not assume that buffer is
  // empty till that time. Workaround is not bulletproof, as we don't exactly
  // know when that particular data would start playing, but it is much better
  // than nothing.
  base::Time earliest_end_time_;

  AudioParameters audio_parameters_;

  AudioDecoder::ReadCB read_cb_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
