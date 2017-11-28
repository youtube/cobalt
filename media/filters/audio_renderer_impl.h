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
//    Play/Pause/Preroll() also happens here.
// 3. Audio thread created by the AudioRendererSink.
//    Render() is called here where audio data is decoded into raw PCM data.
//
// AudioRendererImpl talks to an AudioRendererAlgorithm that takes care of
// queueing audio data and stretching/shrinking audio data when playback rate !=
// 1.0 or 0.0.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_

#include <deque>

#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_renderer.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/buffers.h"
#include "media/base/decryptor.h"
#include "media/filters/audio_renderer_algorithm.h"

namespace media {

class AudioDecoderSelector;
class AudioSplicer;
class DecryptingDemuxerStream;

class MEDIA_EXPORT AudioRendererImpl
    : public AudioRenderer,
      NON_EXPORTED_BASE(public AudioRendererSink::RenderCallback) {
 public:
  // Methods called on Render thread ------------------------------------------
  // An AudioRendererSink is used as the destination for the rendered audio.
  AudioRendererImpl(AudioRendererSink* sink,
                    const SetDecryptorReadyCB& set_decryptor_ready_cb);

  // Methods called on pipeline thread ----------------------------------------
  // AudioRenderer implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const AudioDecoderList& decoders,
                          const PipelineStatusCB& init_cb,
                          const StatisticsCB& statistics_cb,
                          const base::Closure& underflow_cb,
                          const TimeCB& time_cb,
                          const base::Closure& ended_cb,
                          const base::Closure& disabled_cb,
                          const PipelineStatusCB& error_cb) override;
  virtual void Play(const base::Closure& callback) override;
  virtual void Pause(const base::Closure& callback) override;
  virtual void Flush(const base::Closure& callback) override;
  virtual void Stop(const base::Closure& callback) override;
  virtual void SetPlaybackRate(float rate) override;
  virtual void Preroll(base::TimeDelta time,
                       const PipelineStatusCB& cb) override;
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) override;
  virtual void SetVolume(float volume) override;

  // Disables underflow support.  When used, |state_| will never transition to
  // kUnderflow resulting in Render calls that underflow returning 0 frames
  // instead of some number of silence frames.  Must be called prior to
  // Initialize().
  void DisableUnderflowForTesting();

 protected:
  virtual ~AudioRendererImpl();

 private:
  friend class AudioRendererImplTest;
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, EndOfStream);
  FRIEND_TEST_ALL_PREFIXES(AudioRendererImplTest, Underflow_EndOfStream);

  // Callback from the audio decoder delivering decoded audio samples.
  void DecodedAudioReady(AudioDecoder::Status status,
                         const scoped_refptr<Buffer>& buffer);

  // Handles buffers that come out of |splicer_|.
  // Returns true if more buffers are needed.
  bool HandleSplicerBuffer(const scoped_refptr<Buffer>& buffer);

  // Helper functions for AudioDecoder::Status values passed to
  // DecodedAudioReady().
  void HandleAbortedReadOrDecodeError(bool is_decode_error);

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
  // should the filled buffer be played.
  //
  // Safe to call on any thread.
  uint32 FillBuffer(uint8* dest,
                    uint32 requested_frames,
                    int audio_delay_milliseconds);

  // Estimate earliest time when current buffer can stop playing.
  void UpdateEarliestEndTime_Locked(int frames_filled,
                                    float playback_rate,
                                    base::TimeDelta playback_delay,
                                    base::Time time_now);

  // Methods called on pipeline thread ----------------------------------------
  void DoPlay();
  void DoPause();

  // media::AudioRendererSink::RenderCallback implementation.  Called on the
  // AudioDevice thread.
  virtual int Render(AudioBus* audio_bus,
                     int audio_delay_milliseconds) override;
  virtual void OnRenderError() override;

  // Helper method that schedules an asynchronous read from the decoder and
  // increments |pending_reads_|.
  //
  // Safe to call from any thread.
  void ScheduleRead_Locked();

  // Returns true if the data in the buffer is all before
  // |preroll_timestamp_|. This can only return true while
  // in the kPrerolling state.
  bool IsBeforePrerollTime(const scoped_refptr<Buffer>& buffer);

  // Called when |decoder_selector_| selected the |selected_decoder|.
  // |decrypting_demuxer_stream| was also populated if a DecryptingDemuxerStream
  // created to help decrypt the encrypted stream.
  // Note: |decoder_selector| is passed here to keep the AudioDecoderSelector
  // alive until OnDecoderSelected() finishes.
  void OnDecoderSelected(
      scoped_ptr<AudioDecoderSelector> decoder_selector,
      const scoped_refptr<AudioDecoder>& selected_decoder,
      const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream);

  void ResetDecoder(const base::Closure& callback);

  scoped_ptr<AudioSplicer> splicer_;

  // The sink (destination) for rendered audio.  |sink_| must only be accessed
  // on the pipeline thread (verify with |pipeline_thread_checker_|).  |sink_|
  // must never be called under |lock_| or the 3-way thread bridge between the
  // audio, pipeline, and decoder threads may deadlock.
  scoped_refptr<media::AudioRendererSink> sink_;

  SetDecryptorReadyCB set_decryptor_ready_cb_;

  // These two will be set by AudioDecoderSelector::SelectAudioDecoder().
  scoped_refptr<AudioDecoder> decoder_;
  scoped_refptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  // Ensures certain methods are always called on the pipeline thread.
  base::ThreadChecker pipeline_thread_checker_;

  // AudioParameters constructed during Initialize() based on |decoder_|.
  AudioParameters audio_parameters_;

  // Callbacks provided during Initialize().
  PipelineStatusCB init_cb_;
  StatisticsCB statistics_cb_;
  base::Closure underflow_cb_;
  TimeCB time_cb_;
  base::Closure ended_cb_;
  base::Closure disabled_cb_;
  PipelineStatusCB error_cb_;

  // Callback provided to Pause().
  base::Closure pause_cb_;

  // Callback provided to Preroll().
  PipelineStatusCB preroll_cb_;

  // After Initialize() has completed, all variables below must be accessed
  // under |lock_|. ------------------------------------------------------------
  base::Lock lock_;

  // Algorithm for scaling audio.
  scoped_ptr<AudioRendererAlgorithm> algorithm_;

  // Simple state tracking variable.
  enum State {
    kUninitialized,
    kPaused,
    kPrerolling,
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

  base::TimeDelta preroll_timestamp_;

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

  bool underflow_disabled_;

  // True if the renderer receives a buffer with kAborted status during preroll,
  // false otherwise. This flag is cleared on the next Preroll() call.
  bool preroll_aborted_;

  // End variables which must be accessed under |lock_|. ----------------------

  // Variables used only on the audio thread. ---------------------------------
  int actual_frames_per_buffer_;
  scoped_array<uint8> audio_buffer_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
