// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cobalt/media/player/web_media_player_impl.h"

#include <math.h>

#include <limits>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/trace_event.h"
#include "base/float_util.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/media/base/bind_to_current_loop.h"
#include "cobalt/media/base/drm_system.h"
#include "cobalt/media/base/limits.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/filters/chunk_demuxer.h"
#include "cobalt/media/filters/shell_demuxer.h"
#include "cobalt/media/player/web_media_player_proxy.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

namespace {

// Used to ensure that there is no more than one instance of WebMediaPlayerImpl.
WebMediaPlayerImpl* s_instance;

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
const double kEndOfStreamEpsilonInSeconds = 2.;

bool IsNearTheEndOfStream(const WebMediaPlayerImpl* wmpi, double position) {
  float duration = wmpi->GetDuration();
  if (base::IsFinite(duration)) {
    // If video is very short, we always treat a position as near the end.
    if (duration <= kEndOfStreamEpsilonInSeconds) return true;
    if (position >= duration - kEndOfStreamEpsilonInSeconds) return true;
  }
  return false;
}
#endif  // defined(COBALT_SKIP_SEEK_REQUEST_NEAR_END)

base::TimeDelta ConvertSecondsToTimestamp(float seconds) {
  float microseconds = seconds * base::Time::kMicrosecondsPerSecond;
  float integer = ceilf(microseconds);
  float difference = integer - microseconds;

  // Round down if difference is large enough.
  if ((microseconds > 0 && difference > 0.5f) ||
      (microseconds <= 0 && difference >= 0.5f)) {
    integer -= 1.0f;
  }

  // Now we can safely cast to int64 microseconds.
  return base::TimeDelta::FromMicroseconds(static_cast<int64>(integer));
}

}  // namespace

#define BIND_TO_RENDER_LOOP(function) \
  BindToCurrentLoop(base::Bind(function, AsWeakPtr()))

#define BIND_TO_RENDER_LOOP_2(function, arg1, arg2) \
  BindToCurrentLoop(base::Bind(function, AsWeakPtr(), arg1, arg2))

// TODO(acolwell): Investigate whether the key_system & session_id parameters
// are really necessary.
typedef base::Callback<void(const std::string&, const std::string&,
                            scoped_array<uint8>, int)>
    OnNeedKeyCB;

WebMediaPlayerImpl::WebMediaPlayerImpl(
    PipelineWindow window, WebMediaPlayerClient* client,
    WebMediaPlayerDelegate* delegate,
    DecoderBuffer::Allocator* buffer_allocator,
    const scoped_refptr<MediaLog>& media_log)
    : pipeline_thread_("media_pipeline"),
      network_state_(WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      main_loop_(MessageLoop::current()),
      client_(client),
      delegate_(delegate),
      buffer_allocator_(buffer_allocator),
      proxy_(new WebMediaPlayerProxy(main_loop_->message_loop_proxy(), this)),
      media_log_(media_log),
      incremented_externally_allocated_memory_(false),
      is_local_source_(false),
      supports_save_(true),
      suppress_destruction_errors_(false),
      drm_system_(NULL) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::WebMediaPlayerImpl");

  video_frame_provider_ = new VideoFrameProvider();

  DLOG_IF(ERROR, s_instance)
      << "More than one WebMediaPlayerImpl has been created.";
  s_instance = this;

  DCHECK(buffer_allocator_);
  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_CREATED));

  pipeline_thread_.Start();
  pipeline_ = Pipeline::Create(window, pipeline_thread_.message_loop_proxy(),
                               media_log_, video_frame_provider_);

  // Also we want to be notified of |main_loop_| destruction.
  main_loop_->AddDestructionObserver(this);

  if (delegate_) {
    delegate_->RegisterPlayer(this);
  }
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::~WebMediaPlayerImpl");

  DCHECK(!main_loop_ || main_loop_ == MessageLoop::current());

  DLOG_IF(ERROR, s_instance != this)
      << "More than one WebMediaPlayerImpl has been created.";
  s_instance = NULL;

  if (delegate_) {
    delegate_->UnregisterPlayer(this);
  }

  Destroy();
  progressive_demuxer_.reset();
  chunk_demuxer_.reset();

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

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
  DCHECK_EQ(main_loop_, MessageLoop::current());

  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(url), kMaxURLScheme);
  DLOG(INFO) << "Start URL playback";

  // Handle any volume changes that occured before load().
  SetVolume(GetClient()->Volume());

  // TODO: Set networkState to WebMediaPlayer::kNetworkStateIdle on stop.
  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  is_local_source_ = !url.SchemeIs("http") && !url.SchemeIs("https");

  StartPipeline(url);
}

#else  // SB_HAS(PLAYER_WITH_URL)

void WebMediaPlayerImpl::LoadMediaSource() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::LoadMediaSource");
  DCHECK_EQ(main_loop_, MessageLoop::current());

  DLOG(INFO) << "Start MEDIASOURCE playback";

  // Handle any volume changes that occured before load().
  SetVolume(GetClient()->Volume());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);

  // Media source pipelines can start immediately.
  chunk_demuxer_.reset(new ChunkDemuxer(
      buffer_allocator_,
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDemuxerOpened),
      BIND_TO_RENDER_LOOP(
          &WebMediaPlayerImpl::OnEncryptedMediaInitDataEncountered),
      media_log_, true));

  supports_save_ = false;
  state_.is_media_source = true;
  StartPipeline(chunk_demuxer_.get());
}

void WebMediaPlayerImpl::LoadProgressive(
    const GURL& url, scoped_ptr<BufferedDataSource> data_source) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::LoadProgressive");
  DCHECK_EQ(main_loop_, MessageLoop::current());

  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(url), kMaxURLScheme);
  DLOG(INFO) << "Start PROGRESSIVE playback";

  // Handle any volume changes that occured before load().
  SetVolume(GetClient()->Volume());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  data_source->SetDownloadingStatusCB(
      base::Bind(&WebMediaPlayerImpl::OnDownloadingStatusChanged, AsWeakPtr()));
  proxy_->set_data_source(data_source.Pass());

  is_local_source_ = !url.SchemeIs("http") && !url.SchemeIs("https");

  progressive_demuxer_.reset(
      new ShellDemuxer(pipeline_thread_.message_loop_proxy(), buffer_allocator_,
                       proxy_->data_source(), media_log_));

  state_.is_progressive = true;
  StartPipeline(progressive_demuxer_.get());
}

#endif  // SB_HAS(PLAYER_WITH_URL)

void WebMediaPlayerImpl::CancelLoad() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
}

void WebMediaPlayerImpl::Play() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::Play");

  DCHECK_EQ(main_loop_, MessageLoop::current());
#if defined(__LB_ANDROID__)
  audio_focus_bridge_.RequestAudioFocus();
#endif  // defined(__LB_ANDROID__)

  state_.paused = false;
  pipeline_->SetPlaybackRate(state_.playback_rate);

  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::PLAY));
}

void WebMediaPlayerImpl::Pause() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
#if defined(__LB_ANDROID__)
  audio_focus_bridge_.AbandonAudioFocus();
#endif  // defined(__LB_ANDROID__)

  state_.paused = true;
  pipeline_->SetPlaybackRate(0.0f);
  state_.paused_time = pipeline_->GetMediaTime();

  media_log_->AddEvent(media_log_->CreateEvent(MediaLogEvent::PAUSE));
}

bool WebMediaPlayerImpl::SupportsFullscreen() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return true;
}

bool WebMediaPlayerImpl::SupportsSave() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return supports_save_;
}

void WebMediaPlayerImpl::Seek(float seconds) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

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

  media_log_->AddEvent(media_log_->CreateSeekEvent(seconds));

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

void WebMediaPlayerImpl::SetEndTime(float seconds) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // TODO(hclam): add method call when it has been implemented.
  return;
}

void WebMediaPlayerImpl::SetRate(float rate) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

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
  DCHECK_EQ(main_loop_, MessageLoop::current());

  pipeline_->SetVolume(volume);
}

void WebMediaPlayerImpl::SetVisible(bool visible) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // TODO(hclam): add appropriate method call when pipeline has it implemented.
  return;
}

bool WebMediaPlayerImpl::HasVideo() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->HasVideo();
}

bool WebMediaPlayerImpl::HasAudio() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->HasAudio();
}

gfx::Size WebMediaPlayerImpl::GetNaturalSize() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  gfx::Size size;
  pipeline_->GetNaturalVideoSize(&size);
  return size;
}

bool WebMediaPlayerImpl::IsPaused() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->GetPlaybackRate() == 0.0f;
}

bool WebMediaPlayerImpl::IsSeeking() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) return false;

  return state_.seeking;
}

float WebMediaPlayerImpl::GetDuration() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return std::numeric_limits<float>::quiet_NaN();

  base::TimeDelta duration = pipeline_->GetMediaDuration();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration == kInfiniteDuration)
    return std::numeric_limits<float>::infinity();

  return static_cast<float>(duration.InSecondsF());
}

#if SB_HAS(PLAYER_WITH_URL)
base::Time WebMediaPlayerImpl::GetStartDate() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return base::Time();

  base::TimeDelta start_date = pipeline_->GetMediaStartDate();

  return base::Time::FromSbTime(start_date.InMicroseconds());
}
#endif  // SB_HAS(PLAYER_WITH(URL)

float WebMediaPlayerImpl::GetCurrentTime() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  if (state_.paused) return static_cast<float>(state_.paused_time.InSecondsF());
  return static_cast<float>(pipeline_->GetMediaTime().InSecondsF());
}

int WebMediaPlayerImpl::GetDataRate() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // TODO(hclam): Add this method call if pipeline has it in the interface.
  return 0;
}

WebMediaPlayer::NetworkState WebMediaPlayerImpl::GetNetworkState() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return network_state_;
}

WebMediaPlayer::ReadyState WebMediaPlayerImpl::GetReadyState() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return ready_state_;
}

const Ranges<base::TimeDelta>& WebMediaPlayerImpl::GetBufferedTimeRanges() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  buffered_ = pipeline_->GetBufferedTimeRanges();
  return buffered_;
}

float WebMediaPlayerImpl::GetMaxTimeSeekable() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // We don't support seeking in streaming media.
  if (proxy_ && proxy_->data_source() && proxy_->data_source()->IsStreaming())
    return 0.0f;
  return static_cast<float>(pipeline_->GetMediaDuration().InSecondsF());
}

void WebMediaPlayerImpl::Suspend() { pipeline_->Suspend(); }

void WebMediaPlayerImpl::Resume() { pipeline_->Resume(); }

bool WebMediaPlayerImpl::DidLoadingProgress() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return pipeline_->DidLoadingProgress();
}

bool WebMediaPlayerImpl::HasSingleSecurityOrigin() const {
  if (proxy_) return proxy_->HasSingleOrigin();
  return true;
}

bool WebMediaPlayerImpl::DidPassCORSAccessCheck() const {
  return proxy_ && proxy_->DidPassCORSAccessCheck();
}

float WebMediaPlayerImpl::MediaTimeForTimeValue(float timeValue) const {
  return ConvertSecondsToTimestamp(timeValue).InSecondsF();
}

unsigned WebMediaPlayerImpl::GetDecodedFrameCount() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_frames_decoded;
}

unsigned WebMediaPlayerImpl::GetDroppedFrameCount() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_frames_dropped;
}

unsigned WebMediaPlayerImpl::GetAudioDecodedByteCount() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.audio_bytes_decoded;
}

unsigned WebMediaPlayerImpl::GetVideoDecodedByteCount() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  PipelineStatistics stats = pipeline_->GetStatistics();
  return stats.video_bytes_decoded;
}

scoped_refptr<VideoFrameProvider> WebMediaPlayerImpl::GetVideoFrameProvider() {
  return video_frame_provider_;
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

void WebMediaPlayerImpl::SetDrmSystem(DrmSystem* drm_system) {
  DCHECK_EQ(static_cast<DrmSystem*>(NULL), drm_system_);
  DCHECK_NE(static_cast<DrmSystem*>(NULL), drm_system);

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

void WebMediaPlayerImpl::OnPipelineSeek(PipelineStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  state_.starting = false;
  state_.seeking = false;
  if (state_.pending_seek) {
    state_.pending_seek = false;
    Seek(state_.pending_seek_seconds);
    return;
  }

  if (status != PIPELINE_OK) {
    OnPipelineError(status, "Failed pipeline seek.");
    return;
  }

  // Update our paused time.
  if (state_.paused) state_.paused_time = pipeline_->GetMediaTime();

  const bool eos_played = false;
  GetClient()->TimeChanged(eos_played);
}

void WebMediaPlayerImpl::OnPipelineEnded(PipelineStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  if (status != PIPELINE_OK) {
    OnPipelineError(status, "Failed pipeline end.");
    return;
  }

  const bool eos_played = true;
  GetClient()->TimeChanged(eos_played);
}

void WebMediaPlayerImpl::OnPipelineError(PipelineStatus error,
                                         const std::string& message) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (suppress_destruction_errors_) return;

  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(error));

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
    return;
  }

  std::string default_message;
  switch (error) {
    case PIPELINE_OK:
      NOTREACHED() << "PIPELINE_OK isn't an error!";
      break;

    case PIPELINE_ERROR_NETWORK:
      SetNetworkError(WebMediaPlayer::kNetworkStateNetworkError,
                      message.empty() ? "Pipeline network error." : message);
      break;
    case PIPELINE_ERROR_READ:
      SetNetworkError(WebMediaPlayer::kNetworkStateNetworkError,
                      message.empty() ? "Pipeline read error." : message);
      break;
    case CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateNetworkError,
          message.empty() ? "Chunk demuxer eos network error." : message);
      break;

    // TODO(vrk): Because OnPipelineInitialize() directly reports the
    // NetworkStateFormatError instead of calling OnPipelineError(), I believe
    // this block can be deleted. Should look into it! (crbug.com/126070)
    case PIPELINE_ERROR_INITIALIZATION_FAILED:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateFormatError,
          message.empty() ? "Pipeline initialization failed." : message);
      break;
    case PIPELINE_ERROR_COULD_NOT_RENDER:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Pipeline could not render." : message);
      break;
    case PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateFormatError,
          message.empty() ? "Pipeline extenrnal renderer failed." : message);
      break;
    case DEMUXER_ERROR_COULD_NOT_OPEN:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Demuxer could not open." : message);
      break;
    case DEMUXER_ERROR_COULD_NOT_PARSE:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Demuxer could not parse." : message);
      break;
    case DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateFormatError,
          message.empty() ? "Demuxer no supported streams." : message);
      break;
    case DECODER_ERROR_NOT_SUPPORTED:
      SetNetworkError(WebMediaPlayer::kNetworkStateFormatError,
                      message.empty() ? "Decoder not supported." : message);
      break;

    case PIPELINE_ERROR_DECODE:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Pipeline decode error." : message);
      break;
    case PIPELINE_ERROR_ABORT:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Pipeline abort." : message);
      break;
    case PIPELINE_ERROR_INVALID_STATE:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Pipeline invalid state." : message);
      break;
    case CHUNK_DEMUXER_ERROR_APPEND_FAILED:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateDecodeError,
          message.empty() ? "Chunk demuxer append failed." : message);
      break;
    case CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateDecodeError,
          message.empty() ? "Chunk demuxer eos decode error." : message);
      break;
    case AUDIO_RENDERER_ERROR:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Audio renderer error." : message);
      break;
    case AUDIO_RENDERER_ERROR_SPLICE_FAILED:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateDecodeError,
          message.empty() ? "Audio renderer splice failed." : message);
      break;
  }
}

void WebMediaPlayerImpl::OnPipelineBufferingState(
    Pipeline::BufferingState buffering_state) {
  DVLOG(1) << "OnPipelineBufferingState(" << buffering_state << ")";

  switch (buffering_state) {
    case Pipeline::kHaveMetadata:
      SetReadyState(WebMediaPlayer::kReadyStateHaveMetadata);
      break;
    case Pipeline::kPrerollCompleted:
      SetReadyState(WebMediaPlayer::kReadyStateHaveEnoughData);
      break;
  }
}

void WebMediaPlayerImpl::OnDemuxerOpened() {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::OnDemuxerOpened");
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DCHECK(chunk_demuxer_);

  GetClient()->SourceOpened(chunk_demuxer_.get());
}

void WebMediaPlayerImpl::SetOpaque(bool opaque) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->SetOpaque(opaque);
}

void WebMediaPlayerImpl::OnDownloadingStatusChanged(bool is_downloading) {
  if (!is_downloading && network_state_ == WebMediaPlayer::kNetworkStateLoading)
    SetNetworkState(WebMediaPlayer::kNetworkStateIdle);
  else if (is_downloading &&
           network_state_ == WebMediaPlayer::kNetworkStateIdle)
    SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  media_log_->AddEvent(
      media_log_->CreateBooleanEvent(MediaLogEvent::NETWORK_ACTIVITY_SET,
                                     "is_downloading_data", is_downloading));
}

#if SB_HAS(PLAYER_WITH_URL)
void WebMediaPlayerImpl::StartPipeline(const GURL& url) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::StartPipeline");

  state_.starting = true;

  pipeline_->SetDecodeToTextureOutputMode(client_->PreferDecodeToTexture());
  pipeline_->Start(
      NULL, BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::SetDrmSystemReadyCB),
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
#else  // SB_HAS(PLAYER_WITH_URL)
void WebMediaPlayerImpl::StartPipeline(Demuxer* demuxer) {
  TRACE_EVENT0("cobalt::media", "WebMediaPlayerImpl::StartPipeline");

  state_.starting = true;

  pipeline_->SetDecodeToTextureOutputMode(client_->PreferDecodeToTexture());
  pipeline_->Start(
      demuxer, BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::SetDrmSystemReadyCB),
      // SB_HAS(PLAYER_WITH_URL)
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineEnded),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineError),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineBufferingState),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDurationChanged),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnOutputModeChanged),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnContentSizeChanged));
}
#endif  // SB_HAS(PLAYER_WITH_URL)

void WebMediaPlayerImpl::SetNetworkState(WebMediaPlayer::NetworkState state) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DVLOG(1) << "SetNetworkState: " << state;
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->NetworkStateChanged();
}

void WebMediaPlayerImpl::SetNetworkError(WebMediaPlayer::NetworkState state,
                                         const std::string& message) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DVLOG(1) << "SetNetworkError: " << state << " message: " << message;
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->NetworkError(message);
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DVLOG(1) << "SetReadyState: " << state;

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing &&
      state >= WebMediaPlayer::kReadyStateHaveMetadata) {
    if (!HasVideo()) GetClient()->DisableAcceleratedCompositing();
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

  DCHECK(!main_loop_ || main_loop_ == MessageLoop::current());

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
  base::WaitableEvent waiter(false, false);
  DLOG(INFO) << "Trying to stop media pipeline.";
  pipeline_->Stop(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
  waiter.Wait();
  DLOG(INFO) << "Media pipeline stopped.";

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
    EmeInitDataType init_data_type, const std::vector<uint8_t>& init_data) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->EncryptedMediaInitDataEncountered(init_data_type, &init_data[0],
                                                 init_data.size());
}

WebMediaPlayerClient* WebMediaPlayerImpl::GetClient() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
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
