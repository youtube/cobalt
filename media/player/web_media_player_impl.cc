// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/player/web_media_player_impl.h"

#include <math.h>

#include <limits>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/float_util.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "media/audio/shell_audio_sink.h"
#include "media/base/bind_to_loop.h"
#include "media/base/filter_collection.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/shell_audio_renderer.h"
#include "media/filters/shell_demuxer.h"
#include "media/filters/video_renderer_base.h"
#include "media/player/web_media_player_proxy.h"

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

#define BIND_TO_RENDER_LOOP(function)          \
  BindToLoop(main_loop_->message_loop_proxy(), \
             base::Bind(function, AsWeakPtr()))

#define BIND_TO_RENDER_LOOP_2(function, arg1, arg2) \
  BindToLoop(main_loop_->message_loop_proxy(),      \
             base::Bind(function, AsWeakPtr(), arg1, arg2))

// TODO(acolwell): Investigate whether the key_system & session_id parameters
// are really necessary.
typedef base::Callback<
    void(const std::string&, const std::string&, scoped_array<uint8>, int)>
    OnNeedKeyCB;

static void LogMediaSourceError(const scoped_refptr<MediaLog>& media_log,
                                const std::string& error) {
  media_log->AddEvent(media_log->CreateMediaSourceErrorEvent(error));
}

WebMediaPlayerImpl::WebMediaPlayerImpl(
    PipelineWindow window,
    WebMediaPlayerClient* client,
    WebMediaPlayerDelegate* delegate,
    const scoped_refptr<ShellVideoFrameProvider>& video_frame_provider,
    scoped_ptr<FilterCollection> collection,
    const scoped_refptr<AudioRendererSink>& audio_renderer_sink,
    scoped_ptr<MessageLoopFactory> message_loop_factory,
    const scoped_refptr<MediaLog>& media_log)
    : network_state_(WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(WebMediaPlayer::kReadyStateHaveNothing),
      main_loop_(MessageLoop::current()),
      filter_collection_(collection.Pass()),
      message_loop_factory_(message_loop_factory.Pass()),
      client_(client),
      delegate_(delegate),
      video_frame_provider_(video_frame_provider),
      proxy_(new WebMediaPlayerProxy(main_loop_->message_loop_proxy(), this)),
      media_log_(media_log),
      incremented_externally_allocated_memory_(false),
      audio_renderer_sink_(audio_renderer_sink),
      is_local_source_(false),
      supports_save_(true),
      suppress_destruction_errors_(false) {
  DLOG_IF(ERROR, s_instance)
      << "More than one WebMediaPlayerImpl has been created.";
  s_instance = this;

  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_CREATED));

  scoped_refptr<base::MessageLoopProxy> pipeline_message_loop =
      message_loop_factory_->GetMessageLoop(MessageLoopFactory::kPipeline);
  pipeline_ = Pipeline::Create(window, pipeline_message_loop, media_log_);

  // Also we want to be notified of |main_loop_| destruction.
  main_loop_->AddDestructionObserver(this);

  SetDecryptorReadyCB set_decryptor_ready_cb;

  decryptor_.reset(new ProxyDecryptor(proxy_.get()));
  set_decryptor_ready_cb = base::Bind(&ProxyDecryptor::SetDecryptorReadyCB,
                                      base::Unretained(decryptor_.get()));

  // Create default video renderer.
  scoped_refptr<VideoRendererBase> video_renderer = new VideoRendererBase(
      pipeline_message_loop, set_decryptor_ready_cb,
      base::Bind(base::DoNothing),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::SetOpaque), true);
  filter_collection_->AddVideoRenderer(video_renderer);
  proxy_->set_frame_provider(video_renderer);

  if (audio_renderer_sink) {
    filter_collection_->AddAudioRenderer(ShellAudioRenderer::Create(
        audio_renderer_sink, set_decryptor_ready_cb, pipeline_message_loop));
  }

  if (video_frame_provider_) {
    media_time_and_seeking_state_cb_ =
        base::Bind(&WebMediaPlayerImpl::GetMediaTimeAndSeekingState,
                   base::Unretained(this));
    video_frame_provider_->RegisterMediaTimeAndSeekingStateCB(
        media_time_and_seeking_state_cb_);
  }
  if (delegate_) {
    delegate_->RegisterPlayer(this);
  }
}

WebMediaPlayerImpl::~WebMediaPlayerImpl() {
  DCHECK(!main_loop_ || main_loop_ == MessageLoop::current());

  DLOG_IF(ERROR, s_instance != this)
      << "More than one WebMediaPlayerImpl has been created.";
  s_instance = NULL;

  if (delegate_) {
    delegate_->UnregisterPlayer(this);
  }

  if (video_frame_provider_) {
    DCHECK(!media_time_and_seeking_state_cb_.is_null());
    video_frame_provider_->UnregisterMediaTimeAndSeekingStateCB(
        media_time_and_seeking_state_cb_);
    media_time_and_seeking_state_cb_.Reset();
  }

#if defined(__LB_ANDROID__)
  audio_focus_bridge_.AbandonAudioFocus();
#endif  // defined(__LB_ANDROID__)

  decryptor_->DestroySoon();
  Destroy();
  media_log_->AddEvent(
      media_log_->CreateEvent(MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));

  // Finally tell the |main_loop_| we don't want to be notified of destruction
  // event.
  if (main_loop_) {
    main_loop_->RemoveDestructionObserver(this);
  }
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
  if (!url.has_scheme())
    return kMissingURLScheme;
  if (url.SchemeIs("http"))
    return kHttpURLScheme;
  if (url.SchemeIs("https"))
    return kHttpsURLScheme;
  if (url.SchemeIs("ftp"))
    return kFtpURLScheme;
  if (url.SchemeIs("chrome-extension"))
    return kChromeExtensionURLScheme;
  if (url.SchemeIs("javascript"))
    return kJavascriptURLScheme;
  if (url.SchemeIs("file"))
    return kFileURLScheme;
  if (url.SchemeIs("blob"))
    return kBlobURLScheme;
  if (url.SchemeIs("data"))
    return kDataURLScheme;
  if (url.SchemeIs("filesystem"))
    return kFileSystemScheme;
  return kUnknownURLScheme;
}

}  // anonymous namespace

void WebMediaPlayerImpl::LoadMediaSource() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DCHECK(filter_collection_);

  // Handle any volume changes that occured before load().
  SetVolume(GetClient()->Volume());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);

  scoped_refptr<base::MessageLoopProxy> message_loop =
      message_loop_factory_->GetMessageLoop(MessageLoopFactory::kPipeline);

  // Media source pipelines can start immediately.
  chunk_demuxer_ = new ChunkDemuxer(
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDemuxerOpened),
      BIND_TO_RENDER_LOOP_2(&WebMediaPlayerImpl::OnNeedKey, "", ""),
      base::Bind(&LogMediaSourceError, media_log_));

  filter_collection_->SetDemuxer(chunk_demuxer_);
  supports_save_ = false;
  state_.is_media_source = true;
  StartPipeline();
}

void WebMediaPlayerImpl::LoadProgressive(
    const GURL& url,
    scoped_ptr<BufferedDataSource> data_source) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DCHECK(filter_collection_);

  UMA_HISTOGRAM_ENUMERATION("Media.URLScheme", URLScheme(url), kMaxURLScheme);

  // Handle any volume changes that occured before load().
  SetVolume(GetClient()->Volume());

  SetNetworkState(WebMediaPlayer::kNetworkStateLoading);
  SetReadyState(WebMediaPlayer::kReadyStateHaveNothing);
  media_log_->AddEvent(media_log_->CreateLoadEvent(url.spec()));

  scoped_refptr<base::MessageLoopProxy> message_loop =
      message_loop_factory_->GetMessageLoop(MessageLoopFactory::kPipeline);

  data_source->SetDownloadingStatusCB(
      base::Bind(&WebMediaPlayerImpl::OnDownloadingStatusChanged, AsWeakPtr()));
  proxy_->set_data_source(data_source.Pass());

  is_local_source_ = !url.SchemeIs("http") && !url.SchemeIs("https");

  filter_collection_->SetDemuxer(
      new ShellDemuxer(message_loop, proxy_->data_source()));

  state_.is_progressive = true;
  StartPipeline();
}

void WebMediaPlayerImpl::CancelLoad() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
}

void WebMediaPlayerImpl::Play() {
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

  if (state_.starting || state_.seeking) {
    state_.pending_seek = true;
    state_.pending_seek_seconds = seconds;
    if (chunk_demuxer_) {
      chunk_demuxer_->CancelPendingSeek();
      decryptor_->CancelDecrypt(Decryptor::kAudio);
      decryptor_->CancelDecrypt(Decryptor::kVideo);
    }
    return;
  }

  media_log_->AddEvent(media_log_->CreateSeekEvent(seconds));

  base::TimeDelta seek_time = ConvertSecondsToTimestamp(seconds);

  // Update our paused time.
  if (state_.paused)
    state_.paused_time = seek_time;

  state_.seeking = true;

  if (chunk_demuxer_) {
    chunk_demuxer_->StartWaitingForSeek();
    decryptor_->CancelDecrypt(Decryptor::kAudio);
    decryptor_->CancelDecrypt(Decryptor::kVideo);
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
  if (rate < 0.0f)
    return;

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

bool WebMediaPlayerImpl::GetTotalBytesKnown() {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->GetTotalBytes() != 0;
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

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return false;

  return state_.seeking;
}

float WebMediaPlayerImpl::GetDuration() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return std::numeric_limits<float>::quiet_NaN();

  base::TimeDelta duration = pipeline_->GetMediaDuration();

  // Return positive infinity if the resource is unbounded.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/video.html#dom-media-duration
  if (duration == kInfiniteDuration())
    return std::numeric_limits<float>::infinity();

  return static_cast<float>(duration.InSecondsF());
}

float WebMediaPlayerImpl::GetCurrentTime() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  if (state_.paused)
    return static_cast<float>(state_.paused_time.InSecondsF());
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

void WebMediaPlayerImpl::Suspend() {
  pipeline_->Suspend();
}

void WebMediaPlayerImpl::Resume() {
  pipeline_->Resume();
}

bool WebMediaPlayerImpl::DidLoadingProgress() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  return pipeline_->DidLoadingProgress();
}

unsigned long long WebMediaPlayerImpl::GetTotalBytes() const {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  return pipeline_->GetTotalBytes();
}

bool WebMediaPlayerImpl::HasSingleSecurityOrigin() const {
  if (proxy_)
    return proxy_->HasSingleOrigin();
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

scoped_refptr<ShellVideoFrameProvider>
WebMediaPlayerImpl::GetVideoFrameProvider() {
  return video_frame_provider_;
}

scoped_refptr<VideoFrame> WebMediaPlayerImpl::GetCurrentFrame() {
  if (video_frame_provider_) {
    return video_frame_provider_->GetCurrentFrame();
  }
  return NULL;
}

void WebMediaPlayerImpl::PutCurrentFrame(
    const scoped_refptr<VideoFrame>& video_frame) {
  if (video_frame) {
    proxy_->PutCurrentFrame(video_frame);
  } else {
    proxy_->PutCurrentFrame(NULL);
  }
}

// TODO: Eliminate the duplicated enums.
#define COMPILE_ASSERT_MATCHING_STATUS_ENUM(player_name, chromium_name) \
  COMPILE_ASSERT(static_cast<int>(WebMediaPlayer::player_name) ==       \
                     static_cast<int>(ChunkDemuxer::chromium_name),     \
                 mismatching_status_enums)
COMPILE_ASSERT_MATCHING_STATUS_ENUM(kAddIdStatusOk, kOk);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(kAddIdStatusNotSupported, kNotSupported);
COMPILE_ASSERT_MATCHING_STATUS_ENUM(kAddIdStatusReachedIdLimit,
                                    kReachedIdLimit);
#undef COMPILE_ASSERT_MATCHING_ENUM

WebMediaPlayer::AddIdStatus WebMediaPlayerImpl::SourceAddId(
    const std::string& id,
    const std::string& type,
    const std::vector<std::string>& codecs) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  std::vector<std::string> new_codecs(codecs.size());
  for (size_t i = 0; i < codecs.size(); ++i)
    new_codecs[i] = codecs[i];

  return static_cast<WebMediaPlayer::AddIdStatus>(
      chunk_demuxer_->AddId(id, type, new_codecs));
}

bool WebMediaPlayerImpl::SourceRemoveId(const std::string& id) {
  DCHECK(!id.empty());
  chunk_demuxer_->RemoveId(id);
  return true;
}

Ranges<base::TimeDelta> WebMediaPlayerImpl::SourceBuffered(
    const std::string& id) {
  return chunk_demuxer_->GetBufferedRanges(id);
}

bool WebMediaPlayerImpl::SourceAppend(const std::string& id,
                                      const unsigned char* data,
                                      unsigned length) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  float old_duration = GetDuration();
  if (!chunk_demuxer_->AppendData(id, data, length))
    return false;

  if (old_duration != GetDuration())
    GetClient()->DurationChanged();

  return true;
}

bool WebMediaPlayerImpl::SourceAbort(const std::string& id) {
  chunk_demuxer_->Abort(id);
  return true;
}

double WebMediaPlayerImpl::SourceGetDuration() const {
  DCHECK(chunk_demuxer_);
  return chunk_demuxer_->GetDuration();
}

void WebMediaPlayerImpl::SourceSetDuration(double new_duration) {
  DCHECK_GE(new_duration, 0);
  DCHECK(chunk_demuxer_);
  chunk_demuxer_->SetDuration(new_duration);
}

void WebMediaPlayerImpl::SourceEndOfStream(
    WebMediaPlayer::EndOfStreamStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  PipelineStatus pipeline_status = PIPELINE_OK;

  switch (status) {
    case WebMediaPlayer::kEndOfStreamStatusNoError:
      break;
    case WebMediaPlayer::kEndOfStreamStatusNetworkError:
      pipeline_status = PIPELINE_ERROR_NETWORK;
      break;
    case WebMediaPlayer::kEndOfStreamStatusDecodeError:
      pipeline_status = PIPELINE_ERROR_DECODE;
      break;
    default:
      NOTIMPLEMENTED();
  }

  float old_duration = GetDuration();
  if (!chunk_demuxer_->EndOfStream(pipeline_status))
    DVLOG(1) << "EndOfStream call failed.";

  if (old_duration != GetDuration())
    GetClient()->DurationChanged();
}

bool WebMediaPlayerImpl::SourceSetTimestampOffset(const std::string& id,
                                                  double offset) {
  base::TimeDelta time_offset = base::TimeDelta::FromMicroseconds(
      offset * base::Time::kMicrosecondsPerSecond);
  return chunk_demuxer_->SetTimestampOffset(id, time_offset);
}

// Helper enum for reporting generateKeyRequest/addKey histograms.
enum MediaKeyException {
  kUnknownResultId,
  kSuccess,
  kKeySystemNotSupported,
  kInvalidPlayerState,
  kMaxMediaKeyException
};

static MediaKeyException MediaKeyExceptionForUMA(
    WebMediaPlayer::MediaKeyException e) {
  switch (e) {
    case WebMediaPlayer::kMediaKeyExceptionKeySystemNotSupported:
      return kKeySystemNotSupported;
    case WebMediaPlayer::kMediaKeyExceptionInvalidPlayerState:
      return kInvalidPlayerState;
    case WebMediaPlayer::kMediaKeyExceptionNoError:
      return kSuccess;
    default:
      return kUnknownResultId;
  }
}

// Helper for converting |key_system| name and exception |e| to a pair of enum
// values from above, for reporting to UMA.
static void ReportMediaKeyExceptionToUMA(const std::string& method,
                                         const std::string& key_system,
                                         WebMediaPlayer::MediaKeyException e) {
  MediaKeyException result_id = MediaKeyExceptionForUMA(e);
  DCHECK_NE(result_id, kUnknownResultId) << e;
  base::LinearHistogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + "." + method, 1,
      kMaxMediaKeyException, kMaxMediaKeyException + 1,
      base::Histogram::kUmaTargetedHistogramFlag)
      ->Add(result_id);
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::GenerateKeyRequest(
    const std::string& key_system,
    const unsigned char* init_data,
    unsigned init_data_length) {
  WebMediaPlayer::MediaKeyException e =
      GenerateKeyRequestInternal(key_system, init_data, init_data_length);
  ReportMediaKeyExceptionToUMA("generateKeyRequest", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException
WebMediaPlayerImpl::GenerateKeyRequestInternal(const std::string& key_system,
                                               const unsigned char* init_data,
                                               unsigned init_data_length) {
  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::kMediaKeyExceptionKeySystemNotSupported;

  // We do not support run-time switching between key systems for now.
  if (current_key_system_.empty())
    current_key_system_ = key_system;
  else if (key_system != current_key_system_)
    return WebMediaPlayer::kMediaKeyExceptionInvalidPlayerState;

  DVLOG(1) << "generateKeyRequest: " << key_system << ": "
           << std::string(reinterpret_cast<const char*>(init_data),
                          static_cast<size_t>(init_data_length));

  // TODO(xhwang): We assume all streams are from the same container (thus have
  // the same "type") for now. In the future, the "type" should be passed down
  // from the application.
  if (!decryptor_->GenerateKeyRequest(key_system, init_data_type_, init_data,
                                      init_data_length)) {
    current_key_system_.clear();
    return WebMediaPlayer::kMediaKeyExceptionKeySystemNotSupported;
  }

  return WebMediaPlayer::kMediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::AddKey(
    const std::string& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const std::string& session_id) {
  WebMediaPlayer::MediaKeyException e = AddKeyInternal(
      key_system, key, key_length, init_data, init_data_length, session_id);
  ReportMediaKeyExceptionToUMA("addKey", key_system, e);
  return e;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::AddKeyInternal(
    const std::string& key_system,
    const unsigned char* key,
    unsigned key_length,
    const unsigned char* init_data,
    unsigned init_data_length,
    const std::string& session_id) {
  DCHECK(key);
  DCHECK_GT(key_length, 0u);

  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::kMediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.empty() || key_system != current_key_system_)
    return WebMediaPlayer::kMediaKeyExceptionInvalidPlayerState;

  DVLOG(1) << "addKey: " << key_system << ": "
           << base::HexEncode(key, static_cast<size_t>(key_length)) << ", "
           << base::HexEncode(init_data, static_cast<size_t>(init_data_length))
           << " [" << session_id << "]";

  decryptor_->AddKey(key_system, key, key_length, init_data, init_data_length,
                     session_id);
  return WebMediaPlayer::kMediaKeyExceptionNoError;
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::CancelKeyRequest(
    const std::string& key_system,
    const std::string& session_id) {
  WebMediaPlayer::MediaKeyException e =
      CancelKeyRequestInternal(key_system, session_id);
  ReportMediaKeyExceptionToUMA("cancelKeyRequest", key_system, e);
  return e;
}

WebMediaPlayerImpl::SetBoundsCB WebMediaPlayerImpl::GetSetBoundsCB() {
  // |pipeline_| is always valid during WebMediaPlayerImpl's life time.  It is
  // also reference counted so it lives after WebMediaPlayerImpl is destroyed.
  return pipeline_->GetSetBoundsCB();
}

WebMediaPlayer::MediaKeyException WebMediaPlayerImpl::CancelKeyRequestInternal(
    const std::string& key_system,
    const std::string& session_id) {
  if (!IsSupportedKeySystem(key_system))
    return WebMediaPlayer::kMediaKeyExceptionKeySystemNotSupported;

  if (current_key_system_.empty() || key_system != current_key_system_)
    return WebMediaPlayer::kMediaKeyExceptionInvalidPlayerState;

  decryptor_->CancelKeyRequest(key_system, session_id);
  return WebMediaPlayer::kMediaKeyExceptionNoError;
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
  if (state_.paused)
    state_.paused_time = pipeline_->GetMediaTime();

  const bool eos_played = false;
  GetClient()->TimeChanged(eos_played);
}

void WebMediaPlayerImpl::OnPipelineEnded(PipelineStatus status) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  if (status != PIPELINE_OK) {
    OnPipelineError(status, "Failed pipeline end. ");
    return;
  }
  const bool eos_played = true;
  GetClient()->TimeChanged(eos_played);
}

void WebMediaPlayerImpl::OnPipelineError(PipelineStatus error,
                                         const std::string& message) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  if (suppress_destruction_errors_)
    return;

  media_log_->AddEvent(media_log_->CreatePipelineErrorEvent(error));

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing) {
    // Any error that occurs before reaching ReadyStateHaveMetadata should
    // be considered a format error.
    SetNetworkState(WebMediaPlayer::kNetworkStateFormatError);
    return;
  }

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
    case PIPELINE_ERROR_URL_NOT_FOUND:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateFormatError,
          message.empty() ? "Pipeline error URL not found." : message);
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
    case PIPELINE_ERROR_OPERATION_PENDING:
      SetNetworkError(
          WebMediaPlayer::kNetworkStateDecodeError,
          message.empty() ? "Pipeline operation pending." : message);
      break;
    case PIPELINE_ERROR_INVALID_STATE:
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Pipeline invalid state." : message);
      break;

    case PIPELINE_ERROR_DECRYPT:
      // Decrypt error.
      base::Histogram::FactoryGet(
          (kMediaEme + KeySystemNameForUMA(current_key_system_) +
           ".DecryptError"),
          1, 1000000, 50, base::Histogram::kUmaTargetedHistogramFlag)
          ->Add(1);
      // TODO(xhwang): Change to use NetworkStateDecryptError once it's added in
      // Webkit (see http://crbug.com/124486).
      SetNetworkError(WebMediaPlayer::kNetworkStateDecodeError,
                      message.empty() ? "Pipeline decrypt error." : message);
      break;

    case PIPELINE_STATUS_MAX:
      NOTREACHED() << "PIPELINE_STATUS_MAX isn't a real error!";
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
  DCHECK_EQ(main_loop_, MessageLoop::current());

  DCHECK(chunk_demuxer_);
  GetClient()->SourceOpened(chunk_demuxer_);
}

void WebMediaPlayerImpl::OnKeyAdded(const std::string& key_system,
                                    const std::string& session_id) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  base::Histogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + ".KeyAdded", 1, 1000000, 50,
      base::Histogram::kUmaTargetedHistogramFlag)
      ->Add(1);

  GetClient()->KeyAdded(key_system, session_id);
}

void WebMediaPlayerImpl::OnNeedKey(const std::string& key_system,
                                   const std::string& session_id,
                                   const std::string& type,
                                   scoped_array<uint8> init_data,
                                   int init_data_size) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  // Do not fire NeedKey event if encrypted media is not enabled.
  if (!decryptor_)
    return;

  UMA_HISTOGRAM_COUNTS(kMediaEme + std::string("NeedKey"), 1);

#if !defined(__LB_SHELL__) && !defined(COBALT)
  DCHECK(init_data_type_.empty() || type.empty() || type == init_data_type_);
#endif  // !defined(__LB_SHELL__) && !defined(COBALT)

  if (init_data_type_.empty())
    init_data_type_ = type;

  GetClient()->KeyNeeded(key_system, session_id, init_data.get(),
                         init_data_size);
}

// TODO: Eliminate the duplicated enums.
#define COMPILE_ASSERT_MATCHING_ENUM(name)                       \
  COMPILE_ASSERT(static_cast<int>(WebMediaPlayerClient::name) == \
                     static_cast<int>(Decryptor::name),          \
                 mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(kUnknownError);
COMPILE_ASSERT_MATCHING_ENUM(kClientError);
COMPILE_ASSERT_MATCHING_ENUM(kServiceError);
COMPILE_ASSERT_MATCHING_ENUM(kOutputError);
COMPILE_ASSERT_MATCHING_ENUM(kHardwareChangeError);
COMPILE_ASSERT_MATCHING_ENUM(kDomainError);
#undef COMPILE_ASSERT_MATCHING_ENUM

void WebMediaPlayerImpl::OnKeyError(const std::string& key_system,
                                    const std::string& session_id,
                                    Decryptor::KeyError error_code,
                                    int system_code) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  base::LinearHistogram::FactoryGet(
      kMediaEme + KeySystemNameForUMA(key_system) + ".KeyError", 1,
      Decryptor::kMaxKeyError, Decryptor::kMaxKeyError + 1,
      base::Histogram::kUmaTargetedHistogramFlag)
      ->Add(error_code);

  GetClient()->KeyError(
      key_system, session_id,
      static_cast<WebMediaPlayerClient::MediaKeyErrorCode>(error_code),
      system_code);
}

void WebMediaPlayerImpl::OnKeyMessage(const std::string& key_system,
                                      const std::string& session_id,
                                      const std::string& message,
                                      const GURL& default_url) {
  DCHECK_EQ(main_loop_, MessageLoop::current());

  GetClient()->KeyMessage(key_system, session_id,
                          reinterpret_cast<const uint8*>(message.data()),
                          message.size(), default_url.spec());
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

void WebMediaPlayerImpl::StartPipeline() {
  state_.starting = true;

  SetDecryptorReadyCB set_decryptor_ready_cb;
  if (decryptor_) {
    set_decryptor_ready_cb = base::Bind(&ProxyDecryptor::SetDecryptorReadyCB,
                                        base::Unretained(decryptor_.get()));
  }

  pipeline_->SetDecodeToTextureOutputMode(client_->PreferDecodeToTexture());
  pipeline_->Start(
      filter_collection_.Pass(), set_decryptor_ready_cb,
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineEnded),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineError),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineSeek),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnPipelineBufferingState),
      BIND_TO_RENDER_LOOP(&WebMediaPlayerImpl::OnDurationChanged));
}

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
  DVLOG(1) << "SetNetworkState: " << state << " message: " << message;
  network_state_ = state;
  // Always notify to ensure client has the latest value.
  GetClient()->NetworkError(message);
}

void WebMediaPlayerImpl::SetReadyState(WebMediaPlayer::ReadyState state) {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DVLOG(1) << "SetReadyState: " << state;

  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing &&
      state >= WebMediaPlayer::kReadyStateHaveMetadata) {
    if (!HasVideo())
      GetClient()->DisableAcceleratedCompositing();
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
  DCHECK(!main_loop_ || main_loop_ == MessageLoop::current());

  // If |main_loop_| has already stopped, do nothing here.
  if (!main_loop_) {
    // This may happen if this function was already called by the
    // DestructionObserver override when the thread running this player was
    // stopped. The pipeline should have been shut down.
    DCHECK(!chunk_demuxer_);
    DCHECK(!message_loop_factory_);
    DCHECK(!proxy_);
    return;
  }

  // Tell the data source to abort any pending reads so that the pipeline is
  // not blocked when issuing stop commands to the other filters.
  suppress_destruction_errors_ = true;
  if (proxy_) {
    proxy_->AbortDataSource();
    if (chunk_demuxer_) {
      chunk_demuxer_->Shutdown();
      chunk_demuxer_ = NULL;
    }
  }

  // Make sure to kill the pipeline so there's no more media threads running.
  // Note: stopping the pipeline might block for a long time.
  base::WaitableEvent waiter(false, false);
  DLOG(INFO) << "Trying to stop media pipeline.";
  pipeline_->Stop(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
  waiter.Wait();
  DLOG(INFO) << "Media pipeline stopped.";

  message_loop_factory_.reset();

  // And then detach the proxy, it may live on the render thread for a little
  // longer until all the tasks are finished.
  if (proxy_) {
    proxy_->Detach();
    proxy_ = NULL;
  }
}

void WebMediaPlayerImpl::GetMediaTimeAndSeekingState(
    base::TimeDelta* media_time,
    bool* is_seeking) const {
  DCHECK(media_time);
  DCHECK(is_seeking);
  *media_time = pipeline_->GetMediaTime();
  *is_seeking = state_.seeking;
}

WebMediaPlayerClient* WebMediaPlayerImpl::GetClient() {
  DCHECK_EQ(main_loop_, MessageLoop::current());
  DCHECK(client_);
  return client_;
}

void WebMediaPlayerImpl::OnDurationChanged() {
  if (ready_state_ == WebMediaPlayer::kReadyStateHaveNothing)
    return;

  GetClient()->DurationChanged();
}

}  // namespace media
