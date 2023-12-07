// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cobalt/media/player/web_media_player_impl.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/instance_counter.h"
#include "cobalt/media/base/drm_system.h"
#include "cobalt/media/base/sbplayer_pipeline.h"
#include "cobalt/media/player/web_media_player_proxy.h"
#include "cobalt/media/progressive/data_source_reader.h"
#include "cobalt/media/progressive/demuxer_extension_wrapper.h"
#include "cobalt/media/progressive/progressive_demuxer.h"
#include "starboard/system.h"
#include "starboard/types.h"
#include "third_party/chromium/media/base/bind_to_current_loop.h"
#include "third_party/chromium/media/base/limits.h"
#include "third_party/chromium/media/base/timestamp_constants.h"
#include "third_party/chromium/media/cobalt/ui/gfx/geometry/rect.h"
#include "third_party/chromium/media/cobalt/ui/gfx/geometry/size.h"
#include "third_party/chromium/media/filters/chunk_demuxer.h"

namespace cobalt {
namespace media {

namespace {

// Limits the range of playback rate.
//
// TODO(kylep): Revisit these.
//
// Vista has substantially lower performance than XP or Windows7.  If you speed
// up a video too much, it can't keep up, and rendering stops updating except on
// the time bar. For really high speeds, audio becomes a bottleneck and we just
// use up the data we have, which may not achieve the speed requested, but will
// not crash the tab.
//
// A very slow speed, ie 0.00000001x, causes the machine to lock up. (It seems
// like a busy loop). It gets unresponsive, although its not completely dead.
//
// Also our timers are not very accurate (especially for ogg), which becomes
// evident at low speeds and on Vista. Since other speeds are risky and outside
// the norms, we think 1/16x to 16x is a safe and useful range for now.
const float kMinRate = 0.0625f;
const float kMaxRate = 16.0f;

// Prefix for histograms related to Encrypted Media Extensions.
const char* kMediaEme = "Media.EME.";

#if defined(COBALT_SKIP_SEEK_REQUEST_NEAR_END)
// On some platforms, the underlying media player can hang if we keep seeking to
// a position that is near the end of the video. So we ignore any seeks near the
// end of stream position when the current playback position is also near the
// end of the stream. In this case, "near the end of stream" means "position
// greater than or equal to duration() - kEndOfStreamEpsilonInSeconds".
const double kEndOfStreamEpsilonInSeconds = 2.0;

DECLARE_INSTANCE_COUNTER(WebMediaPlayerImpl);

bool IsNearTheEndOfStream(const WebMediaPlayerImpl* wmpi, double position) {
  const double duration = wmpi->GetDuration();
  if (std::isfinite(duration)) {
    // If video is very short, we always treat a position as near the end.
    if (duration <= kEndOfStreamEpsilonInSeconds) return true;
    if (position >= duration - kEndOfStreamEpsilonInSeconds) return true;
  }
  return false;
}
#endif  // defined(COBALT_SKIP_SEEK_REQUEST_NEAR_END)

base::TimeDelta ConvertSecondsToTimestamp(double seconds) {
  return base::TimeDelta::FromMicrosecondsD(std::round(
      seconds * static_cast<double>(base::Time::kMicrosecondsPerSecond)));
}

}  // namespace

#define BIND_TO_RENDER_LOOP(function) \
  ::media::BindToCurrentLoop(base::Bind(function, AsWeakPtr()))

#define BIND_TO_RENDER_LOOP_2(function, arg1, arg2) \
  ::media::BindToCurrentLoop(base::Bind(function, AsWeakPtr(), arg1, arg2))

// TODO(acolwell): Investigate whether the key_system & session_id parameters
// are really necessary.
typedef base::Callback<void(const std::string&, const std::string&,
                            std::unique_ptr<uint8[]>, int)>
    OnNeedKeyCB;

WebMediaPlayerImpl::WebMediaPlayerImpl(
    SbPlayerInterface* interface, PipelineWindow window,
    const Pipeline::GetDecodeTargetGraphicsContextProviderFunc&
        get_decode_target_graphics_context_provider_func,
    WebMediaPlayerClient* client, WebMediaPlayerDelegate* delegate,
    bool allow_resume_after_suspend, bool allow_batched_sample_write,
    bool force_punch_out_by_default,
#if SB_API_VERSION >= 15
    SbTime audio_write_duration_local, SbTime audio_write_duration_remote,
#endif  // SB_API_VERSION >= 15
    ::media::MediaLog* const media_log)
    : pipeline_thread_("media_pipeline"),
      network_state_(WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      main_loop_(base::MessageLoop::current()),
      client_(client),
      delegate_(delegate),
      allow_resume_after_suspend_(allow_resume_after_suspend),
      allow_batched_sample_write_(allow_batched_sample_write),
      force_punch_out_by_default_(force_punch_out_by_default),
      proxy_(new WebMediaPlayerProxy(main_loop_->task_runner(), this)),
      media_log_(media_log),
      is_local_source_(false),
      suppress_destruction_errors_(false),
      drm_system_(NULL),
      window_(window) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::WebMediaPlayerImpl");

  ON_INSTANCE_CREATED(WebMediaPlayerImpl);

  decode_target_provider_ = new DecodeTargetProvider();

  media_log_->AddEvent<::media::MediaLogEvent::kWebMediaPlayerCreated>();

  pipeline_thread_.Start();
  pipeline_ = new SbPlayerPipeline(
      interface, window, pipeline_thread_.task_runner(),
      get_decode_target_graphics_context_provider_func,
      allow_resume_after_suspend_, allow_batched_sample_write_,
      force_punch_out_by_default_,
#if SB_API_VERSION >= 15
      audio_write_duration_local, audio_write_duration_remote,
#endif  // SB_API_VERSION >= 15
      media_log_, &media_metrics_provider_, decode_target_provider_.get());

  // Also we want to be notified of |main_loop_| destruction.
  main_loop_->AddDestructionObserver(this);

  if (delegate_) {
    delegate_->RegisterPlayer(this);
  }
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::~WebMediaPlayerImpl");

  DCHECK(!main_loop_ || main_loop_ == base::MessageLoop::current());

  ON_INSTANCE_RELEASED(WebMediaPlayerImpl);

  if (delegate_) {
    delegate_->UnregisterPlayer(this);
  }

  Destroy();
  progressive_demuxer_.reset();
  chunk_demuxer_.reset();

  media_log_->AddEvent<::media::MediaLogEvent::kWebMediaPlayerDestroyed>();

  // Finally tell the |main_loop_| we don't want to be notified of destruction
  // event.
  if (main_loop_) {
    main_loop_->RemoveDestructionObserver(this);
  }
  pipeline_thread_.Stop();
}

namespace {

// Helper enum for reporting scheme histograms.
enum URLSchemeForHistogram {
  kUnknownURLScheme,
  kMissingURLScheme,
  kHttpURLScheme,
  kHttpsURLScheme,
  kFtpURLScheme,
  kChromeExtensionURLScheme,
  kJavascriptURLScheme,
  kFileURLScheme,
  kBlobURLScheme,
  kDataURLScheme,
  kFileSystemScheme,
  kMaxURLScheme = kFileSystemScheme  // Must be equal to highest enum value.
};

URLSchemeForHistogram URLScheme(const GURL& url) {
  if (!url.has_scheme()) return kMissingURLScheme;
  if (url.SchemeIs("http")) return kHttpURLScheme;
  if (url.SchemeIs("https")) return kHttpsURLScheme;
  if (url.SchemeIs("ftp")) return kFtpURLScheme;
  if (url.SchemeIs("chrome-extension")) return kChromeExtensionURLScheme;
  if (url.SchemeIs("javascript")) return kJavascriptURLScheme;
  if (url.SchemeIs("file")) return kFileURLScheme;
  if (url.SchemeIs("blob")) return kBlobURLScheme;
  if (url.SchemeIs("data")) return kDataURLScheme;
  if (url.SchemeIs("filesystem")) return kFileSystemScheme;
  return kUnknownURLScheme;
}

}  // anonymous namespace

#if SB_HAS(PLAYER_WITH_URL)
void WebMediaPlayerImpl::LoadUrl(const GURL& url) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::LoadUrl");
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(url), kMaxURLScheme);
  LOG(INFO) << "Start URL playback";

  // Handle any volume changes that occurred before load().
  SetVolume(GetClient()->Volume());

  // TODO: Set networkState to WebMediaPlayer::kNetworkStateIdle on stop.
  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  media_log_->AddEvent<::media::MediaLogEvent::kLoad>(url.spec());

  is_local_source_ = !url.SchemeIs("http") && !url.SchemeIs("https");

  StartPipeline(url);
  media_metrics_provider_.Initialize(false);
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void WebMediaPlayerImpl::LoadMediaSource() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::LoadMediaSource");
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  LOG(INFO) << "Start MEDIASOURCE playback";

  // Handle any volume changes that occurred before load().
  SetVolume(GetClient()->Volume());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);

  // Media source pipelines can start immediately.
  chunk_demuxer_.reset(new ::media::ChunkDemuxer(
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDemuxerOpened),
      // TODO(b/230878852): Handle progress callback.
      base::BindRepeating([]() {}),
      BIND_TO_RENDER_LOOP(
          &WebMediaPlayerImpl::OnEncryptedMediaInitDataEncounteredWrapper),
      media_log_));

  state_.is_media_source = true;
  StartPipeline(chunk_demuxer_.get());
  media_metrics_provider_.Initialize(true);
}

void WebMediaPlayerImpl::LoadProgressive(
    const GURL& url, std::unique_ptr<DataSource> data_source) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::LoadProgressive");
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(url), kMaxURLScheme);
  LOG(INFO) << "Start PROGRESSIVE playback";

  // Handle any volume changes that occurred before load().
  SetVolume(GetClient()->Volume());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  media_log_->AddEvent<::media::MediaLogEvent::kLoad>(url.spec());

  data_source->SetDownloadingStatusCB(
      base::Bind(&WebMediaPlayerImpl::OnDownloadingStatusChanged, AsWeakPtr()));
  proxy_->set_data_source(std::move(data_source));

  is_local_source_ = !url.SchemeIs("http") && !url.SchemeIs("https");

  // Attempt to use the demuxer provided via Cobalt Extension, if available.
  progressive_demuxer_ = DemuxerExtensionWrapper::Create(
      proxy_->data_source(), pipeline_thread_.task_runner());

  if (progressive_demuxer_) {
    LOG(INFO) << "Using DemuxerExtensionWrapper.";
  } else {
    // Either the demuxer Cobalt extension was not provided, or it failed to
    // create a demuxer; fall back to the ProgressiveDemuxer.
    LOG(INFO) << "Using ProgressiveDemuxer.";
    progressive_demuxer_.reset(new ProgressiveDemuxer(
        pipeline_thread_.task_runner(), proxy_->data_source(), media_log_));
  }

  state_.is_progressive = true;
  StartPipeline(progressive_demuxer_.get());
  media_metrics_provider_.Initialize(false);
}

void WebMediaPlayerImpl::CancelLoad() {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
}

void WebMediaPlayerImpl::Play() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::Play");

  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  state_.paused = false;
  pipeline_->SetPlaybackRate(state_.playback_rate);

  media_log_->AddEvent<::media::MediaLogEvent::kPlay>();
  media_metrics_provider_.SetHasPlayed();
}

void WebMediaPlayerImpl::Pause() {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  state_.paused = true;
  pipeline_->SetPlaybackRate(0.0f);
  state_.paused_time = pipeline_->GetMediaTime();

  media_log_->AddEvent<::media::MediaLogEvent::kPause>();
}

void WebMediaPlayerImpl::Seek(double seconds) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

#if defined(COBALT_SKIP_SEEK_REQUEST_NEAR_END)
  // Ignore any seek request that is near the end of the stream when the
  // current playback position is also near the end of the stream to avoid
  // a hang in the MediaEngine.
  if (IsNearTheEndOfStream(this, GetCurrentTime()) &&
      IsNearTheEndOfStream(this, seconds)) {
    return;
  }
#endif  // defined(COBALT_SKIP_SEEK_REQUEST_NEAR_END)

  if (state_.starting || state_.seeking) {
    state_.pending_seek = true;
    state_.pending_seek_seconds = seconds;
    if (chunk_demuxer_) {
      chunk_demuxer_->CancelPendingSeek(ConvertSecondsToTimestamp(seconds));
    }
    return;
  }

  media_log_->AddEvent<::media::MediaLogEvent::kSeek>(seconds);

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);

  // Update our paused time.
  if (state_.paused) state_.paused_time = seek_time;

  state_.seeking = true;

  if (chunk_demuxer_) {
    chunk_demuxer_->StartWaitingForSeek(seek_time);
  }

  // Kick off the asynchronous seek!
  pipeline_->Seek(seek_time,
                  BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek));
}

void WebMediaPlayerImpl::SetRate(float rate) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  // TODO(kylep): Remove when support for negatives is added. Also, modify the
  // following checks so rewind uses reasonable values also.
  if (rate < 0.0f) return;

  // Limit rates to reasonable values by clamping.
  if (rate != 0.0f) {
    if (rate < kMinRate)
      rate = kMinRate;
    else if (rate > kMaxRate)
      rate = kMaxRate;
  }

  state_.playback_rate = rate;
  if (!state_.paused) {
    pipeline_->SetPlaybackRate(rate);
  }
}

void WebMediaPlayerImpl::SetVolume(float volume) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  pipeline_->SetVolume(volume);
}

void WebMediaPlayerImpl::SetVisible(bool visible) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  // TODO(hclam): add appropriate method call when pipeline has it implemented.
  return;
}

bool WebMediaPlayerImpl::HasVideo() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  return pipeline_->HasVideo();
}

bool WebMediaPlayerImpl::HasAudio() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  return pipeline_->HasAudio();
}

int WebMediaPlayerImpl::GetNaturalWidth() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  gfx::Size size;
  pipeline_->GetNaturalVideoSize(&size);
  return size.width();
}

int WebMediaPlayerImpl::GetNaturalHeight() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  gfx::Size size;
  pipeline_->GetNaturalVideoSize(&size);
  return size.height();
}

std::vector<std::string> WebMediaPlayerImpl::GetAudioConnectors() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  return pipeline_->GetAudioConnectors();
}

bool WebMediaPlayerImpl::IsPaused() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  return pipeline_->GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::IsSeeking() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) return false;

  return state_.seeking;
}

double WebMediaPlayerImpl::GetDuration() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return std::numeric_limits<double>::quiet_NaN();

  base::TimeDelta duration = pipeline_->GetMediaDuration();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration == ::media::kInfiniteDuration)
    return std::numeric_limits<double>::infinity();

  return duration.InSecondsF();
}

#if SB_HAS(PLAYER_WITH_URL)
base::Time WebMediaPlayerImpl::GetStartDate() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return base::Time();

  base::TimeDelta start_date = pipeline_->GetMediaStartDate();

  return base::Time::FromSbTime(start_date.InMicroseconds());
}
#endif  // SB_HAS(PLAYER_WITH_URL)

double WebMediaPlayerImpl::GetCurrentTime() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  if (state_.paused) {
    return state_.paused_time.InSecondsF();
  }
  return pipeline_->GetMediaTime().InSecondsF();
}

float WebMediaPlayerImpl::GetPlaybackRate() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  if (state_.paused) {
    return 0.0f;
  }
  return state_.playback_rate;
}

int WebMediaPlayerImpl::GetDataRate() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return 0;
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::GetNetworkState() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::GetReadyState() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  return ready_state_;
}

void WebMediaPlayerImpl::UpdateBufferedTimeRanges(
    const AddRangeCB& add_range_cb) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  DCHECK(add_range_cb);

  auto buffered = pipeline_->GetBufferedTimeRanges();

  for (int i = 0; i < static_cast<int>(buffered.size()); ++i) {
    add_range_cb(buffered.start(i).InSecondsF(), buffered.end(i).InSecondsF());
  }
}

double WebMediaPlayerImpl::GetMaxTimeSeekable() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  return pipeline_->GetMediaDuration().InSecondsF();
}

void WebMediaPlayerImpl::Suspend() { pipeline_->Suspend(); }

void WebMediaPlayerImpl::Resume(PipelineWindow window) {
  if (!window_ && window) {
    is_resuming_from_background_mode_ = true;
  }
  window_ = window;
  pipeline_->Resume(window);
}

bool WebMediaPlayerImpl::DidLoadingProgress() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  return pipeline_->DidLoadingProgress();
}

double WebMediaPlayerImpl::MediaTimeForTimeValue(double timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

WebMediaPlayer::PlayerStatistics WebMediaPlayerImpl::GetStatistics() const {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  PlayerStatistics statistics;
  ::media::PipelineStatistics pipeline_stats = pipeline_->GetStatistics();
  statistics.audio_bytes_decoded = pipeline_stats.audio_bytes_decoded;
  statistics.video_bytes_decoded = pipeline_stats.video_bytes_decoded;
  statistics.video_frames_decoded = pipeline_stats.video_frames_decoded;
  statistics.video_frames_dropped = pipeline_stats.video_frames_dropped;
  return statistics;
}

scoped_refptr<DecodeTargetProvider>
WebMediaPlayerImpl::GetDecodeTargetProvider() {
  return decode_target_provider_;
}

WebMediaPlayerImpl::SetBoundsCB WebMediaPlayerImpl::GetSetBoundsCB() {
  // |pipeline_| is always valid during WebMediaPlayerImpl's life time.  It is
  // also reference counted so it lives after WebMediaPlayerImpl is destroyed.
  return pipeline_->GetSetBoundsCB();
}

void WebMediaPlayerImpl::WillDestroyCurrentMessageLoop() {
  Destroy();
  main_loop_ = NULL;
}

bool WebMediaPlayerImpl::GetDebugReportDataAddress(void** out_address,
                                                   size_t* out_size) {
  *out_address = &state_;
  *out_size = sizeof(state_);
  return true;
}

void WebMediaPlayerImpl::SetDrmSystem(
    const scoped_refptr<media::DrmSystem>& drm_system) {
  DCHECK_EQ(static_cast<DrmSystem*>(NULL), drm_system_.get());
  DCHECK_NE(static_cast<DrmSystem*>(NULL), drm_system.get());

  drm_system_ = drm_system;
  if (!drm_system_ready_cb_.is_null()) {
    drm_system_ready_cb_.Run(drm_system_->wrapped_drm_system());
  }
}

void WebMediaPlayerImpl::SetDrmSystemReadyCB(
    const DrmSystemReadyCB& drm_system_ready_cb) {
  drm_system_ready_cb_ = drm_system_ready_cb;
  if (drm_system_) {
    drm_system_ready_cb_.Run(drm_system_->wrapped_drm_system());
  }
}

void WebMediaPlayerImpl::OnPipelineSeek(::media::PipelineStatus status,
                                        bool is_initial_preroll,
                                        const std::string& error_message) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  state_.starting = false;
  state_.seeking = false;
  if (state_.pending_seek) {
    state_.pending_seek = false;
    Seek(state_.pending_seek_seconds);
    return;
  }

  if (status != ::media::PIPELINE_OK) {
    OnPipelineError(status,
                    "Failed pipeline seek with error: " + error_message + ".");
    return;
  }

  // Update our paused time.
  if (state_.paused) state_.paused_time = pipeline_->GetMediaTime();

  if (is_initial_preroll) {
    const bool kEosPlayed = false;
    GetClient()->TimeChanged(kEosPlayed);
  }
}

void WebMediaPlayerImpl::OnPipelineEnded(::media::PipelineStatus status) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  if (status != ::media::PIPELINE_OK) {
    OnPipelineError(status, "Failed pipeline end.");
    return;
  }

  const bool kEosPlayed = true;
  GetClient()->TimeChanged(kEosPlayed);
}

void WebMediaPlayerImpl::OnPipelineError(::media::PipelineStatus error,
                                         const std::string& message) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  if (suppress_destruction_errors_) return;

  media_log_->NotifyError(error);
  media_metrics_provider_.OnError(error);

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkError(
        WebMediaPlayer::kNetworkStateFormatError,
        message.empty()
            ? base::StringPrintf("Ready state have nothing. Error: (%d)",
                                 static_cast<int>(error))
            : base::StringPrintf(
                  "Ready state have nothing: Error: (%d), Message: %s",
                  static_cast<int>(error), message.c_str()));
    return;
  }

  std::string default_message;
  switch (error) {
    case ::media::PIPELINE_OK:
      NOTREACHED() << "PIPELINE_OK isn't an error!";
      break;

    case ::media::PIPELINE_ERROR_NETWORK:
      SetNetworkError(WebMediaPlayer::kNetworkStateNetworkError,
                      message.empty() ? "Pipeline network error." : message);
      break;
    case ::media::PIPELINE_ERROR_READ:
      SetNetworkError(WebMediaPlayer::kNetworkStateNetworkError,
                      message.empty() ? "Pipeline read error." : message);
      break;
    case ::media::CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateNetworkError,
          message.empty() ? "Chunk demuxer eos network error." : message);
      break;

    // TODO(vrk): Because OnPipelineInitialize() directly reports the
    // NetworkStateFormatError instead of calling OnPipelineError(), I believe
    // this block can be deleted. Should look into it! (crbug.com/126070)
    case ::media::PIPELINE_ERROR_INITIALIZATION_FAILED:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateFormatError,
          message.empty() ? "Pipeline initialization failed." : message);
      break;
    case ::media::PIPELINE_ERROR_COULD_NOT_RENDER:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Pipeline could not render." : message);
      break;
    case ::media::PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateFormatError,
          message.empty() ? "Pipeline external renderer failed." : message);
      break;
    case ::media::DEMUXER_ERROR_COULD_NOT_OPEN:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Demuxer could not open." : message);
      break;
    case ::media::DEMUXER_ERROR_COULD_NOT_PARSE:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Demuxer could not parse." : message);
      break;
    case ::media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateFormatError,
          message.empty() ? "Demuxer no supported streams." : message);
      break;
    case ::media::DECODER_ERROR_NOT_SUPPORTED:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Decoder not supported." : message);
      break;
    case ::media::DEMUXER_ERROR_DETECTED_HLS:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Detected HLS format." : message);
      break;

    case ::media::PIPELINE_ERROR_DECODE:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Pipeline decode error." : message);
      break;
    case ::media::PIPELINE_ERROR_ABORT:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Pipeline abort." : message);
      break;
    case ::media::PIPELINE_ERROR_INVALID_STATE:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Pipeline invalid state." : message);
      break;
    case ::media::CHUNK_DEMUXER_ERROR_APPEND_FAILED:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateDecodeError,
          message.empty() ? "Chunk demuxer append failed." : message);
      break;
    case ::media::CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateDecodeError,
          message.empty() ? "Chunk demuxer eos decode error." : message);
      break;
    case ::media::AUDIO_RENDERER_ERROR:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Audio renderer error." : message);
      break;
    case ::media::PIPELINE_ERROR_HARDWARE_CONTEXT_RESET:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Hardware context reset." : message);
      break;

    case ::media::PLAYBACK_CAPABILITY_CHANGED:
      SetNetworkError(WebMediaPlayer::kNetworkStateCapabilityChangedError,
                      message.empty() ? "Capability changed." : message);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void WebMediaPlayerImpl::OnPipelineBufferingState(
    Pipeline::BufferingState buffering_state) {
  DVLOG(1) << "OnPipelineBufferingState(" << buffering_state << ")";

  // If |is_resuming_from_background_mode_| is true, we are exiting background
  // mode and must seek.
  if (is_resuming_from_background_mode_) {
    Seek(pipeline_->GetMediaTime().InSecondsF());
    is_resuming_from_background_mode_ = false;
  }

  switch (buffering_state) {
    case Pipeline::kHaveMetadata:
      SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
      break;
    case Pipeline::kPrerollCompleted:
      SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
      media_metrics_provider_.SetHaveEnough();
      break;
  }
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::OnDemuxerOpened");
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  DCHECK(chunk_demuxer_);

  GetClient()->SourceOpened(chunk_demuxer_.get());
}

void WebMediaPlayerImpl::OnDownloadingStatusChanged(bool is_downloading) {
  if (!is_downloading && network_state_ == WebMediaPlayer::kNetworkStateLoading)
    SetNetworkState(WebMediaPlayer::kNetworkStateIdle);
  else if (is_downloading &&
           network_state_ == WebMediaPlayer::kNetworkStateIdle)
    SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
}

#if SB_HAS(PLAYER_WITH_URL)
void WebMediaPlayerImpl::StartPipeline(const GURL& url) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::StartPipeline");

  state_.starting = true;

  if (client_->PreferDecodeToTexture()) {
    pipeline_->SetPreferredOutputModeToDecodeToTexture();
  }
  pipeline_->Start(
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::SetDrmSystemReadyCB),
      BIND_TO_RENDER_LOOP(
          &WebMediaPlayerImpl::OnEncryptedMediaInitDataEncountered),
      url.spec(), BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineEnded),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineError),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineBufferingState),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDurationChanged),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnOutputModeChanged),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnContentSizeChanged));
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void WebMediaPlayerImpl::StartPipeline(::media::Demuxer* demuxer) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::StartPipeline");

  state_.starting = true;

  if (client_->PreferDecodeToTexture()) {
    pipeline_->SetPreferredOutputModeToDecodeToTexture();
  }
  pipeline_->Start(
      demuxer, BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::SetDrmSystemReadyCB),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineEnded),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineError),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineBufferingState),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDurationChanged),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnOutputModeChanged),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnContentSizeChanged),
      GetClient()->MaxVideoCapabilities());
}

void WebMediaPlayerImpl::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  DVLOG(1) << "SetNetworkState: " << state;
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->NetworkStateChanged();
}

void WebMediaPlayerImpl::SetNetworkError(WebMediaPlayer::NetworkState state,
                                         const std::string& message) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  DVLOG(1) << "SetNetworkError: " << state << " message: " << message;
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->NetworkError(message);
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  DVLOG(1) << "SetReadyState: " << state;

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing &&
      state >= WebMediaPlayer::kReadyStateHaveMetadata) {
  } else if (state == WebMediaPlayer::kReadyStateHaveEnoughData) {
    if (is_local_source_ &&
        network_state_ == WebMediaPlayer::kNetworkStateLoading) {
      SetNetworkState(WebMediaPlayer::kNetworkStateLoaded);
    }
  }

  ready_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->ReadyStateChanged();
}

void WebMediaPlayerImpl::Destroy() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::Destroy");

  DCHECK(!main_loop_ || main_loop_ == base::MessageLoop::current());

  // If |main_loop_| has already stopped, do nothing here.
  if (!main_loop_) {
    // This may happen if this function was already called by the
    // DestructionObserver override when the thread running this player was
    // stopped. The pipeline should have been shut down.
    DCHECK(!proxy_);
    return;
  }

  // Tell the data source to abort any pending reads so that the pipeline is
  // not blocked when issuing stop commands to the other filters.
  suppress_destruction_errors_ = true;
  if (proxy_) {
    proxy_->AbortDataSource();
  }

  // Make sure to kill the pipeline so there's no more media threads running.
  // Note: stopping the pipeline might block for a long time.
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  LOG(INFO) << "Trying to stop media pipeline.";
  pipeline_->Stop(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
  waiter.Wait();
  LOG(INFO) << "Media pipeline stopped.";

  // And then detach the proxy, it may live on the render thread for a little
  // longer until all the tasks are finished.
  if (proxy_) {
    proxy_->Detach();
    proxy_ = NULL;
  }
}

void WebMediaPlayerImpl::GetMediaTimeAndSeekingState(
    base::TimeDelta* media_time, bool* is_seeking) const {
  DCHECK(media_time);
  DCHECK(is_seeking);
  *media_time = pipeline_->GetMediaTime();
  *is_seeking = state_.seeking;
}

void WebMediaPlayerImpl::OnEncryptedMediaInitDataEncountered(
    const char* init_data_type, const std::vector<uint8_t>& init_data) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  GetClient()->EncryptedMediaInitDataEncountered(init_data_type, &init_data[0],
                                                 init_data.size());
}

void WebMediaPlayerImpl::OnEncryptedMediaInitDataEncounteredWrapper(
    ::media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());

  // Initialization data types are defined in
  // https://www.w3.org/TR/eme-initdata-registry/#registry.
  switch (init_data_type) {
    case ::media::EmeInitDataType::UNKNOWN:
      LOG(WARNING) << "Unknown EME initialization data type.";
      OnEncryptedMediaInitDataEncountered("", init_data);
      break;
    case ::media::EmeInitDataType::WEBM:
      OnEncryptedMediaInitDataEncountered("webm", init_data);
      break;
    case ::media::EmeInitDataType::CENC:
      OnEncryptedMediaInitDataEncountered("cenc", init_data);
      break;
    case ::media::EmeInitDataType::KEYIDS:
      OnEncryptedMediaInitDataEncountered("keyids", init_data);
      break;
    default:
      NOTREACHED();
      break;
  }
  media_metrics_provider_.SetIsEME();
}

WebMediaPlayerClient* WebMediaPlayerImpl::GetClient() {
  DCHECK_EQ(main_loop_, base::MessageLoop::current());
  DCHECK(client_);
  return client_;
}

void WebMediaPlayerImpl::OnDurationChanged() {
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) return;

  GetClient()->DurationChanged();
}

void WebMediaPlayerImpl::OnOutputModeChanged() {
  GetClient()->OutputModeChanged();
}

void WebMediaPlayerImpl::OnContentSizeChanged() {
  GetClient()->ContentSizeChanged();
}

}  // namespace media
}  // namespace cobalt
