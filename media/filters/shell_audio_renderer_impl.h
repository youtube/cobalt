/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_FILTERS_SHELL_AUDIO_RENDERER_IMPL_H_
#define MEDIA_FILTERS_SHELL_AUDIO_RENDERER_IMPL_H_

#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/audio_decoder.h"
#include "media/filters/shell_audio_renderer.h"

namespace media {

class AudioDecoderSelector;
class DecryptingDemuxerStream;

class MEDIA_EXPORT ShellAudioRendererImpl : public ShellAudioRenderer {
 public:
  explicit ShellAudioRendererImpl(
      AudioRendererSink* sink,
      const SetDecryptorReadyCB& set_decryptor_ready_cb,
      const scoped_refptr<base::MessageLoopProxy>& message_loop);

  // ======== AudioRenderer Implementation

  // Initialize a AudioRenderer with the given AudioDecoder, executing the
  // |init_cb| upon completion.
  //
  // |statistics_cb| is executed periodically with audio rendering stats.
  //
  // |underflow_cb| is executed when the renderer runs out of data to pass to
  // the audio card during playback. ResumeAfterUnderflow() must be called
  // to resume playback. Pause(), Preroll(), or Stop() cancels the underflow
  // condition.
  //
  // |time_cb| is executed whenever time has advanced by way of audio rendering.
  //
  // |ended_cb| is executed when audio rendering has reached the end of stream.
  //
  // |disabled_cb| is executed when audio rendering has been disabled due to
  // external factors (i.e., device was removed). |time_cb| will no longer be
  // executed.
  //
  // |error_cb| is executed if an error was encountered.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const AudioDecoderList& decoders,
                          const PipelineStatusCB& init_cb,
                          const StatisticsCB& statistics_cb,
                          const base::Closure& underflow_cb,
                          const TimeCB& time_cb,
                          const base::Closure& ended_cb,
                          const base::Closure& disabled_cb,
                          const PipelineStatusCB& error_cb) override;

  // Start prerolling audio data for samples starting at |time|, executing
  // |callback| when completed.
  //
  // Only valid to call after a successful Initialize() or Flush().
  virtual void Preroll(base::TimeDelta time,
                       const PipelineStatusCB& callback) override;

  // Sets the output volume.
  virtual void SetVolume(float volume) override;

  // Resumes playback after underflow occurs.
  //
  // |buffer_more_audio| is set to true if you want to increase the size of the
  // decoded audio buffer.
  virtual void ResumeAfterUnderflow(bool buffer_more_audio) override;

  // The pipeline has resumed playback.  Filters can continue requesting reads.
  // Filters may implement this method if they need to respond to this call.
  virtual void Play(const base::Closure& callback) override;

  // The pipeline has paused playback.  Filters should stop buffer exchange.
  // Filters may implement this method if they need to respond to this call.
  virtual void Pause(const base::Closure& callback) override;

  // The pipeline has been flushed.  Filters should return buffer to owners.
  // Filters may implement this method if they need to respond to this call.
  virtual void Flush(const base::Closure& callback) override;

  // The pipeline is being stopped either as a result of an error or because
  // the client called Stop().
  virtual void Stop(const base::Closure& callback) override;

  // The pipeline playback rate has been changed.  Filters may implement this
  // method if they need to respond to this call.
  virtual void SetPlaybackRate(float playback_rate) override;

  // Carry out any actions required to seek to the given time, executing the
  // callback upon completion.
  // virtual void Seek(base::TimeDelta time,
  //                   const PipelineStatusCB& callback) override;

 protected:
  virtual ~ShellAudioRendererImpl();

  void DoPlay();
  void DoPreroll(base::TimeDelta time, const PipelineStatusCB& callback);
  void DoUnderflow();
  void DoResumeAfterUnderflow(bool buffer_more_audio);
  void DoSinkFull();

  // AudioRendererSink::RenderCallback implementation.
  // Attempts to completely fill all channels of |dest|, returns actual
  // number of frames filled.
  // Render() is run on the system audio thread
  virtual int Render(AudioBus* dest, int audio_delay_milliseconds) override;
  virtual void OnRenderError() override;
  virtual void SinkFull() override;
  virtual void SinkUnderflow() override;

  void DecodedAudioReady(AudioDecoder::Status status,
                         const AudioDecoder::Buffers& buffers);

  void OnDecoderSelected(
      scoped_ptr<AudioDecoderSelector> decoder_selector,
      const scoped_refptr<AudioDecoder>& selected_decoder,
      const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream);

  void OnDecoderReset(const base::Closure& callback);

  enum State {
    kUninitialized,
    kPaused,
    kPrerolling,
    kPlaying,
    kUnderflow,
    kRebuffering,
    kStopping,
    kStopped,
  };

  bool ShouldQueueRead(State state);

  // objects communicating with the rest of the filter graph
  scoped_refptr<AudioRendererSink> sink_;
  SetDecryptorReadyCB set_decryptor_ready_cb_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  scoped_refptr<AudioDecoder> decoder_;
  base::Closure underflow_cb_;
  TimeCB time_cb_;
  base::Closure ended_cb_;
  PipelineStatusCB preroll_cb_;
  base::TimeDelta preroll_timestamp_;
  PipelineStatusCB init_cb_;
  PipelineStatusCB error_cb_;

  State state_;

  enum EOSState {
    kWaitingForEOS,
    kReceivedEOS,
    kRenderedEOS,
    kPlayedEOS,
  };
  EOSState end_of_stream_state_;
  float playback_rate_;
  AudioParameters audio_parameters_;

  AudioBus* decoded_audio_bus_;
  AudioDecoder::Status decode_status_;
  bool decode_complete_;

  // Callback that is called from the H/W renderer thread when it is finished
  // requesting audio samples
  base::Lock renderer_cb_lock_;
  base::Closure renderer_idle_cb_;

  // modified only by MediaPipeline thread while decode_complete is false
  // Can be read by any thread when it is true
  base::TimeDelta buffered_timestamp_;
  // Read/written only in Render callback
  base::TimeDelta rendered_timestamp_;
  // Read/written only in Render callback. Indicates how much silence has been
  // written to the sink after EOS was encountered
  base::TimeDelta silence_rendered_;

  // Store the DecryptingDemuxerStream so we can reset it when Stop is
  // requested. As the top level demuxer has no knowledge of the decrypting
  // demuxer stream, the pipeline will not be able to reset the streams during
  // stopping. So we have to reset it.
  scoped_refptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_AUDIO_RENDERER_IMPL_H_
