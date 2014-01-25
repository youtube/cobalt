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
#include "media/filters/shell_audio_renderer_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/buffers.h"
#include "media/base/demuxer_stream.h"
#include "media/filters/audio_decoder_selector.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/base/shell_filter_graph_log.h"
#include "media/base/shell_filter_graph_log_constants.h"

const int AACDEC_PCM_SAMPLE_SIZE = 1024;

namespace media {

// static
ShellAudioRenderer* ShellAudioRenderer::Create(
    media::AudioRendererSink* sink,
    const SetDecryptorReadyCB& set_decryptor_ready_cb,
    const scoped_refptr<base::MessageLoopProxy>& message_loop) {
  return new ShellAudioRendererImpl(
      sink, set_decryptor_ready_cb, message_loop);
}

ShellAudioRendererImpl::ShellAudioRendererImpl(
    media::AudioRendererSink* sink,
    const SetDecryptorReadyCB& set_decryptor_ready_cb,
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : message_loop_(message_loop)
    , set_decryptor_ready_cb_(set_decryptor_ready_cb)
    , sink_(sink)
    , state_(kUninitialized)
    , end_of_stream_state_(kWaitingForEOS)
    , decoded_audio_bus_(NULL)
    , decode_complete_(false) {
  DCHECK(message_loop_ != NULL);
}

ShellAudioRendererImpl::~ShellAudioRendererImpl() {
  // Stop() does the main teardown work, and should always be called
  // before the object is destroyed.
  DCHECK(state_ == kUninitialized || state_ == kStopped);
}

void ShellAudioRendererImpl::Play(const base::Closure &callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPaused);
  filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventPlay);
  state_ = kPlaying;
  callback.Run();

  DoPlay();
}

void ShellAudioRendererImpl::DoPlay() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPlaying);
  if (playback_rate_ > 0.0f) {
    sink_->Play();
  }
}

void ShellAudioRendererImpl::Pause(const base::Closure &callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == kPlaying || state_ == kUnderflow ||
         state_ == kRebuffering);
  filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventPause);
  sink_->Pause(false);

  state_ = kPaused;
  {
    base::AutoLock lock(renderer_cb_lock_);
    DCHECK(renderer_idle_cb_.is_null());
    renderer_idle_cb_ = BindToCurrentLoop(callback);
  }
}

void ShellAudioRendererImpl::Flush(const base::Closure &callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kPaused);
  DCHECK_EQ(decoded_audio_bus_, (media::AudioBus*)NULL);
  filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventFlush);

  // Pause and flush the sink. Do this only if there is not a decode pending
  // because the sink expects that it will not be flushed while a read is
  // in process
  sink_->Pause(true);

  // Reset the decoder. It will call the callback when it has finished.
  decoder_->Reset(callback);
}

void ShellAudioRendererImpl::Stop(const base::Closure &callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Called by the Decoder once it finishes resetting
  base::Closure reset_complete_cb =
      base::Bind(&ShellAudioRendererImpl::OnDecoderReset,
                 this,
                 callback);

  switch (state_) {
    case kUninitialized:
    case kStopped:
      DCHECK_EQ(decoder_, reinterpret_cast<AudioDecoder*>(NULL));
      // Just post this to the message loop, since the Sink
      // is not yet running
      message_loop_->PostTask(FROM_HERE, reset_complete_cb);
      break;
    case kPrerolling:
    case kPlaying:
    case kRebuffering:
    case kUnderflow:
    case kPaused: {
      // Set to kStopping state to stop further requests for audio, and set
      // a callback that the H/W renderer thread will call when it is idle.
      state_ = kStopping;
      sink_->Pause(false);
      {
        base::AutoLock lock(renderer_cb_lock_);
        // Called when the sink is no longer requesting audio
        renderer_idle_cb_ = BindToCurrentLoop(base::Bind(
            &AudioDecoder::Reset, decoder_, reset_complete_cb));
      }
      break;
    }
    case kStopping:
    default:
      NOTREACHED();
      error_cb_.Run(PIPELINE_ERROR_INVALID_STATE);
      break;
  }
}

void ShellAudioRendererImpl::OnDecoderReset(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(state_ == kStopping || state_ == kUninitialized);
  DCHECK_EQ(decoded_audio_bus_, (media::AudioBus*)NULL);
  DCHECK(renderer_idle_cb_.is_null());
  filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventStop);

  decoder_ = NULL;
  if (state_ != kUninitialized && state_ != kStopped) {
    DCHECK_NE(sink_, (media::AudioRendererSink*)NULL);
    sink_->Stop();
  }
  sink_ = NULL;
  decoded_audio_bus_ = NULL;
  decode_complete_ = false;
  buffered_timestamp_ = base::TimeDelta::FromSeconds(0);
  rendered_timestamp_ = base::TimeDelta::FromSeconds(0);
  silence_rendered_ = base::TimeDelta::FromSeconds(0);
  end_of_stream_state_ = kWaitingForEOS;
  playback_rate_ = 1.0f;
  state_ = kStopped;
  callback.Run();
}

void ShellAudioRendererImpl::SetPlaybackRate(float rate) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  playback_rate_ = rate;
  if (playback_rate_ == 0.0f)
    sink_->Pause(false);
  else if (state_ == kPlaying)
    DoPlay();
}

void ShellAudioRendererImpl::Initialize(
    const scoped_refptr<DemuxerStream>& stream,
    const AudioDecoderList& decoders,
    const PipelineStatusCB& init_cb,
    const StatisticsCB& statistics_cb,
    const base::Closure& underflow_cb,
    const TimeCB& time_cb,
    const base::Closure& ended_cb,
    const base::Closure& disabled_cb,
    const PipelineStatusCB& error_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  filter_graph_log_ = stream->filter_graph_log();
  filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventInitialize);
  underflow_cb_ = underflow_cb;
  time_cb_ = time_cb;
  init_cb_ = init_cb;
  ended_cb_ = ended_cb;
  error_cb_ = error_cb;

  state_ = kUninitialized;
  playback_rate_ = 1.0f;
  end_of_stream_state_ = kWaitingForEOS;
  DCHECK_EQ(decoded_audio_bus_, (media::AudioBus*)NULL);
  decoded_audio_bus_ = NULL;
  decode_complete_ = false;

  scoped_ptr<AudioDecoderSelector> decoder_selector(
      new AudioDecoderSelector(base::MessageLoopProxy::current(),
                               decoders,
                               set_decryptor_ready_cb_));
  AudioDecoderSelector* decoder_selector_ptr = decoder_selector.get();
  decoder_selector_ptr->SelectAudioDecoder(
      stream,
      statistics_cb,
      base::Bind(&ShellAudioRendererImpl::OnDecoderSelected, this,
                 base::Passed(&decoder_selector)));
}

void ShellAudioRendererImpl::OnDecoderSelected(
    scoped_ptr<AudioDecoderSelector> decoder_selector,
    const scoped_refptr<AudioDecoder>& selected_decoder,
    const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (selected_decoder == NULL) {
    DLOG(WARNING) << "NULL decoder selected";
    init_cb_.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // extract configuration information from the demuxer
  ChannelLayout channel_layout = selected_decoder->channel_layout();
  int channels = ChannelLayoutToChannelCount(channel_layout);
  // TODO(***REMOVED***) : Find a proper way to check native channel support here,
  // including proper way to determinate if 5.1 or 7.1 is present.
  if (channels != 1 && channels != 2 && channels != 6) {
    DLOG(WARNING) << "we only support 1, 2 and 6 channel audio";
    init_cb_.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  decoder_ = selected_decoder;

  // construct audio parameters for the sink
  audio_parameters_ = AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      channel_layout,
      decoder_->samples_per_second(),
      decoder_->bits_per_channel(),
      AACDEC_PCM_SAMPLE_SIZE);

  state_ = kPaused;

  DCHECK(sink_.get());
  // initialize and start the sink
  sink_->Initialize(audio_parameters_, this, filter_graph_log_);
  sink_->Start();

  // run the startup callback, we're ready to go
  init_cb_.Run(PIPELINE_OK);
}

void ShellAudioRendererImpl::Preroll(
    base::TimeDelta time,
    const PipelineStatusCB& callback) {
  message_loop_->PostTask(FROM_HERE,
      base::Bind(&ShellAudioRendererImpl::DoPreroll, this, time, callback));
}

void ShellAudioRendererImpl::DoPreroll(
    base::TimeDelta time,
    const PipelineStatusCB& callback) {
  // Start will begin filling the AudioBus with data, and the Streamer
  // will be paused until data is full
  buffered_timestamp_ = base::TimeDelta::FromMicroseconds(0);
  preroll_cb_ = callback;
  preroll_timestamp_ = time;
  state_ = kPrerolling;
}

void ShellAudioRendererImpl::DoUnderflow() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  // don't reprocess if we've already set our state, or already got EOS
  if (state_ == kUnderflow || end_of_stream_state_ >= kReceivedEOS) {
    return;
  }
  filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventUnderflow);
  DLOG(WARNING) << "audio renderer registered underflow";
  // now set the state flag
  state_ = kUnderflow;
  // pause the sink, which won't stop requests but will stop consumption
  // of audio by the rendering hardware.
  sink_->Pause(false);
  // inform the pipeline that we've underflowed
  underflow_cb_.Run();
}

// called by the Pipeline to indicate that it has processed the underflow
// we announced, and that we should begin rebuffering
void ShellAudioRendererImpl::ResumeAfterUnderflow(bool buffer_more_audio) {
  message_loop_->PostTask(FROM_HERE,
      base::Bind(&ShellAudioRendererImpl::DoResumeAfterUnderflow,
                 this,
                 buffer_more_audio));
}

void ShellAudioRendererImpl::DoResumeAfterUnderflow(bool buffer_more_audio) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventResume);
  if (state_ == kUnderflow) {
    state_ = kRebuffering;
    sink_->ResumeAfterUnderflow(buffer_more_audio);
  }
}

void ShellAudioRendererImpl::SinkFull() {
  message_loop_->PostTask(FROM_HERE,
      base::Bind(&ShellAudioRendererImpl::DoSinkFull, this));
}

void ShellAudioRendererImpl::DoSinkFull() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventOutputBufferFull);
  if (state_ == kPrerolling) {
    DCHECK(!preroll_cb_.is_null());
    // we transition from preroll to a pause
    state_ = kPaused;
    base::ResetAndReturn(&preroll_cb_).Run(PIPELINE_OK);
  } else if (state_ == kUnderflow || state_ == kRebuffering) {
    // we pause the sink during an underflow, so now resume
    state_ = kPlaying;
    DoPlay();
  }
}

void ShellAudioRendererImpl::SinkUnderflow() {
  // This will be called by the Audio Renderer thread
  message_loop_->PostTask(FROM_HERE,
      base::Bind(&ShellAudioRendererImpl::DoUnderflow, this));
}

void ShellAudioRendererImpl::SetVolume(float volume) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (sink_)
    sink_->SetVolume(volume);
}

void ShellAudioRendererImpl::DecodedAudioReady(
    AudioDecoder::Status status,
    const scoped_refptr<Buffer>& buffer) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!decode_complete_);
  DCHECK_NE(decoded_audio_bus_, (media::AudioBus*)NULL);

  if (status == AudioDecoder::kOk) {
    if (!buffer) {
      DLOG(WARNING) << "got NULL buffer from decoder";
    } else if (state_ == kStopped) {
      DLOG(WARNING) << "enqueue after stop.";
    } else {
      filter_graph_log_->LogEvent(kObjectIdAudioRenderer, kEventDataDecoded);
      if (buffer->IsEndOfStream()) {
        if (end_of_stream_state_ == kWaitingForEOS)
          end_of_stream_state_ = kReceivedEOS;
        //  if we were rebuffering transition back to playing as we
        //  will never receive additional data
        if (state_ == kUnderflow || state_ == kRebuffering) {
          state_ = kPlaying;
          DoPlay();
        }
      }
      // This is read by the renderer thread in Render(), but will only be read
      // when decode_complete_ is set to true.
      buffered_timestamp_ = buffer->GetTimestamp();
    }
  } else {
    DCHECK(status == AudioDecoder::kAborted ||
        status == AudioDecoder::kDecodeError);
    DLOG_IF(WARNING, status == AudioDecoder::kDecodeError)
        << "audio decoder returned decode error";
    DLOG_IF(WARNING, status == AudioDecoder::kAborted)
        << "audio decoder returned abort";
    // We need to call the preroll callback to unwedge the pipeline on failure
    if (state_ == kPrerolling) {
      // kAborted does not signify an error in the pipeline, so return OK for
      // that case and ERROR for all others
      state_ = kPaused;
      PipelineStatus pipeline_status = status == AudioDecoder::kAborted
          ? PIPELINE_OK : PIPELINE_ERROR_DECODE;
      base::ResetAndReturn(&preroll_cb_).Run(pipeline_status);
    }
  }

  // Signal that the data in pending_decode_ is now valid;
  decode_status_ = status;
  decode_complete_ = true;
}

// CAUTION: this method will not usually execute on the renderer thread!
// On Android, this is normally called by the system audio thread, and must
// not block or perform high-latency operations
int ShellAudioRendererImpl::Render(AudioBus* dest,
                                   int audio_delay_milliseconds) {
  // Use the previous value for rendered_timestamp_ and audio_delay_milliseconds
  // to calculate the current playback time of the audio streamer
  const base::TimeDelta audio_delay =
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds);

  // Once the delay is less than the amount of silence we have rendered, then
  // we know that we have played the last sample
  if (end_of_stream_state_ == kRenderedEOS) {
    if (silence_rendered_ > audio_delay) {
      end_of_stream_state_ = kPlayedEOS;
      message_loop_->PostTask(FROM_HERE, ended_cb_);
    }
  }

  // Once we've called the ended_cb_, we should no longer call the time_cb_
  if (end_of_stream_state_ < kPlayedEOS && state_ != kPaused) {
    // adjust the delay length to account for the amount of silence that has
    // been rendered to the sink
    const base::TimeDelta kZeroTimeDelta = base::TimeDelta::FromMilliseconds(0);
    base::TimeDelta adjusted_delay = (audio_delay - silence_rendered_);
    if (adjusted_delay < kZeroTimeDelta)
      adjusted_delay = kZeroTimeDelta;

    const base::TimeDelta current_time = rendered_timestamp_ - adjusted_delay;
    time_cb_.Run(current_time, rendered_timestamp_);
  }

  // 0 indicates the read is still pending
  int frames_rendered = 0;

  // dest is NULL, this means the sink doesn't need data and is just
  // pinging us with an updated timestamp
  if (dest != NULL) {
    // No pending decode, so fire off the Read if we are in an appropriate state
    if (ShouldQueueRead(state_) && decoded_audio_bus_ == NULL) {
      DCHECK_NE(dest, (media::AudioBus*)NULL);
      decode_complete_ = false;
      decoded_audio_bus_ = dest;
      decoder_->ReadInto(
          decoded_audio_bus_,
          base::Bind(&ShellAudioRendererImpl::DecodedAudioReady, this));
    }

    // Must be polling for the Read that is currently in progress
    if (decoded_audio_bus_ != NULL && decode_complete_) {
      DCHECK_EQ(dest, decoded_audio_bus_);
      decoded_audio_bus_ = NULL;
      if (decode_status_ != AudioDecoder::kOk) {
        // An error has occurred. Indicate this to the sink
        frames_rendered = -1;
      } else {
        if (end_of_stream_state_ == kWaitingForEOS) {
          // Normal decode event
          rendered_timestamp_ = buffered_timestamp_;
          frames_rendered = AACDEC_PCM_SAMPLE_SIZE;
        }
        if (end_of_stream_state_ == kReceivedEOS) {
          end_of_stream_state_ = kRenderedEOS;
        }
        if (end_of_stream_state_ == kRenderedEOS) {
          const int bytes_per_sample = audio_parameters_.bits_per_sample() / 8;
          DCHECK_EQ(AACDEC_PCM_SAMPLE_SIZE,
                    dest->frames() * dest->channels() * sizeof(float) /
                    bytes_per_sample / audio_parameters_.channels());
          // Write zeros (silence) to each channel
          for (int i = 0; i < dest->channels(); ++i) {
            void* channel_data = dest->channel(i);
            size_t num_bytes =
                dest->frames() * sizeof(float);  // NOLINT(runtime/sizeof)
            memset(channel_data, 0, num_bytes);
          }
          frames_rendered = AACDEC_PCM_SAMPLE_SIZE;
          uint64_t silence_ms = AACDEC_PCM_SAMPLE_SIZE * 1000 /
              audio_parameters_.sample_rate();
          silence_rendered_ += base::TimeDelta::FromMilliseconds(silence_ms);
        }
      }
    }
  }

  if (decoded_audio_bus_ == NULL) {
    // No pending decode, so we are idle. Call and clear
    // the callback if it is set.
    renderer_cb_lock_.Acquire();
    base::Closure callback = renderer_idle_cb_;
    renderer_idle_cb_.Reset();
    renderer_cb_lock_.Release();
    if (!callback.is_null())
      callback.Run();
  }

  return frames_rendered;
}

void ShellAudioRendererImpl::OnRenderError() {
  state_ = kUninitialized;
  error_cb_.Run(PIPELINE_ERROR_COULD_NOT_RENDER);
}

scoped_refptr<ShellFilterGraphLog>
ShellAudioRendererImpl::filter_graph_log() {
  return filter_graph_log_;
}

bool ShellAudioRendererImpl::ShouldQueueRead(State state) {
  switch (state) {
    case kPrerolling:
    case kPlaying:
    case kRebuffering:
      return true;
    case kUnderflow:
    case kPaused:
    case kUninitialized:
    case kStopping:
    case kStopped:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace media

