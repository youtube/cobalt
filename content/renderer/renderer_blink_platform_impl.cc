// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_blink_platform_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/time/time_delta_from_string.h"
#include "build/build_config.h"
#include "cc/trees/raster_context_provider_wrapper.h"
#include "components/url_formatter/url_formatter.h"
#include "components/viz/common/features.h"
#include "content/child/child_process.h"
#include "content/common/android/sync_compositor_statics.h"
#include "content/common/content_constants_internal.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/audio_decoder.h"
#include "content/renderer/media/batching_media_log.h"
#include "content/renderer/media/inspector_media_event_handler.h"
#include "content/renderer/media/render_media_event_handler.h"
#include "content/renderer/media/renderer_webaudiodevice_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "content/renderer/variations_render_thread_observer.h"
#include "content/renderer/webgraphicscontext3d_provider_impl.h"
#include "content/renderer/worker/dedicated_worker_host_factory_client.h"
#include "content/renderer/worker/worker_thread_registry.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "gin/array_buffer.h"  // TODO(crbug.com/1321521) remove import once resolved.
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/audio/audio_output_device.h"
#include "media/base/media_permission.h"
#include "media/base/media_switches.h"
#include "media/filters/stream_parser_factory.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/webrtc/webrtc_features.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/modules/video_capture/web_video_capture_impl_manager.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_v8_value_converter.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/media/audio/audio_device_factory.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_media_inspector.h"
#include "third_party/blink/public/web/web_user_level_memory_pressure_signal_generator.h"
#include "third_party/sqlite/sqlite3.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "content/child/child_process_sandbox_support_impl_mac.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/child/child_process_sandbox_support_impl_linux.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

using blink::Platform;
using blink::WebAudioDevice;
using blink::WebAudioLatencyHint;
using blink::WebAudioSinkDescriptor;
using blink::WebMediaStreamTrack;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

namespace {

// TODO(https://crbug.com/787252): Move this method and its callers to Blink.
media::AudioParameters GetAudioHardwareParams() {
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  RenderFrame* const render_frame = RenderFrame::FromWebFrame(web_frame);
  if (!render_frame)
    return media::AudioParameters::UnavailableDeviceParams();

  return blink::AudioDeviceFactory::GetInstance()
      ->GetOutputDeviceInfo(render_frame->GetWebFrame()->GetLocalFrameToken(),
                            std::string())
      .output_params();
}

gpu::ContextType ToGpuContextType(blink::Platform::ContextType type) {
  switch (type) {
    case blink::Platform::kWebGL1ContextType:
      return gpu::CONTEXT_TYPE_WEBGL1;
    case blink::Platform::kWebGL2ContextType:
      return gpu::CONTEXT_TYPE_WEBGL2;
    case blink::Platform::kGLES2ContextType:
      return gpu::CONTEXT_TYPE_OPENGLES2;
    case blink::Platform::kGLES3ContextType:
      return gpu::CONTEXT_TYPE_OPENGLES3;
    case blink::Platform::kWebGPUContextType:
      return gpu::CONTEXT_TYPE_WEBGPU;
  }
  NOTREACHED();
  return gpu::CONTEXT_TYPE_OPENGLES2;
}

}  // namespace

//------------------------------------------------------------------------------

RendererBlinkPlatformImpl::RendererBlinkPlatformImpl(
    blink::scheduler::WebThreadScheduler* main_thread_scheduler)
    : BlinkPlatformImpl(RenderThreadImpl::current()
                            ? RenderThreadImpl::current()->GetIOTaskRunner()
                            : nullptr),
      sudden_termination_disables_(0),
      is_locked_to_site_(false),
      main_thread_scheduler_(main_thread_scheduler) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  sk_sp<font_service::FontLoader> font_loader;
#endif

  // RenderThread may not exist in some tests.
  if (RenderThreadImpl::current()) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    mojo::PendingRemote<font_service::mojom::FontService> font_service;
    RenderThreadImpl::current()->BindHostReceiver(
        font_service.InitWithNewPipeAndPassReceiver());
    font_loader = sk_make_sp<font_service::FontLoader>(std::move(font_service));
    SkFontConfigInterface::SetGlobal(font_loader);
#endif
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  if (sandboxEnabled()) {
#if BUILDFLAG(IS_MAC)
    sandbox_support_ = std::make_unique<WebSandboxSupportMac>();
#else
    sandbox_support_ = std::make_unique<WebSandboxSupportLinux>(font_loader);
#endif
  } else {
    DVLOG(1) << "Disabling sandbox support for testing.";
  }
#endif

  auto io_task_runner = GetIOTaskRunner();
  if (io_task_runner) {
    io_task_runner->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::PlatformThreadId* id,
                          base::WaitableEvent* io_thread_id_ready_event) {
                         *id = base::PlatformThread::CurrentId();
                         io_thread_id_ready_event->Signal();
                       },
                       &io_thread_id_, &io_thread_id_ready_event_));
  } else {
    // Match the `Wait` in destructor even if there is no IO runner.
    io_thread_id_ready_event_.Signal();
  }
}

RendererBlinkPlatformImpl::~RendererBlinkPlatformImpl() {
  base::ScopedAllowBaseSyncPrimitives allow;
  // Ensure task posted to IO thread is finished because it contains
  // pointers to fields of `this`.
  io_thread_id_ready_event_.Wait();
}

void RendererBlinkPlatformImpl::Shutdown() {}

//------------------------------------------------------------------------------

std::string RendererBlinkPlatformImpl::GetNameForHistogram(const char* name) {
  RenderThreadImpl* render_thread_impl = RenderThreadImpl::current();
  // render_thread_impl can be null in tests.
  return render_thread_impl ? render_thread_impl->histogram_customizer()
                                  ->ConvertToCustomHistogramName(name)
                            : std::string{name};
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void RendererBlinkPlatformImpl::SetThreadType(base::PlatformThreadId thread_id,
                                              base::ThreadType thread_type) {
  if (RenderThreadImpl* render_thread = RenderThreadImpl::current()) {
    render_thread->render_message_filter()->SetThreadType(thread_id,
                                                          thread_type);
  }
}
#endif

blink::WebSandboxSupport* RendererBlinkPlatformImpl::GetSandboxSupport() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  return sandbox_support_.get();
#else
  // These platforms do not require sandbox support.
  return nullptr;
#endif
}

bool RendererBlinkPlatformImpl::sandboxEnabled() {
  // As explained in Platform.h, this function is used to decide
  // whether to allow file system operations to come out of WebKit or not.
  // Even if the sandbox is disabled, there's no reason why the code should
  // act any differently...unless we're in single process mode.  In which
  // case, we have no other choice.  Platform.h discourages using
  // this switch unless absolutely necessary, so hopefully we won't end up
  // with too many code paths being different in single-process mode.
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

uint64_t RendererBlinkPlatformImpl::VisitedLinkHash(const char* canonical_url,
                                                    size_t length) {
  return GetContentClient()->renderer()->VisitedLinkHash(canonical_url, length);
}

bool RendererBlinkPlatformImpl::IsLinkVisited(uint64_t link_hash) {
  return GetContentClient()->renderer()->IsLinkVisited(link_hash);
}

blink::WebString RendererBlinkPlatformImpl::UserAgent() {
  auto* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl is null in some tests.
  if (!render_thread)
    return WebString();
  return render_thread->GetUserAgent();
}

blink::WebString RendererBlinkPlatformImpl::FullUserAgent() {
  auto* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl is null in some tests.
  if (!render_thread)
    return WebString();
  return render_thread->GetFullUserAgent();
}

blink::WebString RendererBlinkPlatformImpl::ReducedUserAgent() {
  auto* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl is null in some tests.
  if (!render_thread)
    return WebString();
  return render_thread->GetReducedUserAgent();
}

blink::UserAgentMetadata RendererBlinkPlatformImpl::UserAgentMetadata() {
  auto* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl is null in some tests.
  if (!render_thread)
    return blink::UserAgentMetadata();
  return render_thread->GetUserAgentMetadata();
}

bool RendererBlinkPlatformImpl::IsRedirectSafe(const GURL& from_url,
                                               const GURL& to_url) {
  return IsSafeRedirectTarget(from_url, to_url) &&
         (!GetContentClient()->renderer() ||  // null in unit tests.
          GetContentClient()->renderer()->IsSafeRedirectTarget(from_url,
                                                               to_url));
}

void RendererBlinkPlatformImpl::AppendVariationsThrottles(
    const url::Origin& top_origin,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>>* throttles) {
  VariationsRenderThreadObserver::AppendThrottleIfNeeded(top_origin, throttles);
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
RendererBlinkPlatformImpl::CreateURLLoaderThrottleProviderForWorker(
    blink::URLLoaderThrottleProviderType provider_type) {
  return GetContentClient()->renderer()->CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType::kWorker);
}

std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
RendererBlinkPlatformImpl::CreateWebSocketHandshakeThrottleProvider() {
  return GetContentClient()
      ->renderer()
      ->CreateWebSocketHandshakeThrottleProvider();
}

WebString RendererBlinkPlatformImpl::DefaultLocale() {
  return WebString::FromASCII(RenderThread::Get()->GetLocale());
}

void RendererBlinkPlatformImpl::SuddenTerminationChanged(bool enabled) {
  if (enabled) {
    // We should not get more enables than disables, but we want it to be a
    // non-fatal error if it does happen.
    DCHECK_GT(sudden_termination_disables_, 0);
    sudden_termination_disables_ =
        std::max(sudden_termination_disables_ - 1, 0);
    if (sudden_termination_disables_ != 0)
      return;
  } else {
    sudden_termination_disables_++;
    if (sudden_termination_disables_ != 1)
      return;
  }

  RenderThreadImpl* thread = RenderThreadImpl::current();
  if (thread)  // NULL in unittests.
    thread->GetRendererHost()->SuddenTerminationChanged(enabled);
}

//------------------------------------------------------------------------------

viz::FrameSinkId RendererBlinkPlatformImpl::GenerateFrameSinkId() {
  return viz::FrameSinkId(RenderThread::Get()->GetClientId(),
                          RenderThread::Get()->GenerateRoutingID());
}

bool RendererBlinkPlatformImpl::IsLockedToSite() const {
  return is_locked_to_site_;
}

void RendererBlinkPlatformImpl::SetIsLockedToSite() {
  is_locked_to_site_ = true;
}

bool RendererBlinkPlatformImpl::IsGpuCompositingDisabled() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  RenderThreadImpl* thread = RenderThreadImpl::current();
  // |thread| can be NULL in tests.
  return !thread || thread->IsGpuCompositingDisabled();
}

#if BUILDFLAG(IS_ANDROID)
bool RendererBlinkPlatformImpl::
    IsSynchronousCompositingEnabledForAndroidWebView() {
  return GetContentClient()->UsingSynchronousCompositing();
}

bool RendererBlinkPlatformImpl::
    IsZeroCopySynchronousSwDrawEnabledForAndroidWebView() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

SkCanvas*
RendererBlinkPlatformImpl::SynchronousCompositorGetSkCanvasForAndroidWebView() {
  return content::SynchronousCompositorGetSkCanvas();
}
#endif

bool RendererBlinkPlatformImpl::IsLcdTextEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsLcdTextEnabled() : false;
}

bool RendererBlinkPlatformImpl::IsElasticOverscrollEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsElasticOverscrollEnabled() : false;
}

bool RendererBlinkPlatformImpl::IsScrollAnimatorEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsScrollAnimatorEnabled() : false;
}

bool RendererBlinkPlatformImpl::IsThreadedAnimationEnabled() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->IsThreadedAnimationEnabled() : true;
}

double RendererBlinkPlatformImpl::AudioHardwareSampleRate() {
  return GetAudioHardwareParams().sample_rate();
}

size_t RendererBlinkPlatformImpl::AudioHardwareBufferSize() {
  return GetAudioHardwareParams().frames_per_buffer();
}

unsigned RendererBlinkPlatformImpl::AudioHardwareOutputChannels() {
  return GetAudioHardwareParams().channels();
}

base::TimeDelta RendererBlinkPlatformImpl::GetHungRendererDelay() {
  return kHungRendererDelay;
}

std::unique_ptr<WebAudioDevice> RendererBlinkPlatformImpl::CreateAudioDevice(
    const WebAudioSinkDescriptor& sink_descriptor,
    unsigned number_of_output_channels,
    const blink::WebAudioLatencyHint& latency_hint,
    media::AudioRendererSink::RenderCallback* callback) {
  // The `number_of_output_channels` does not manifest the actual channel
  // layout of the audio output device. We use the best guess to the channel
  // layout based on the number of channels.
  media::ChannelLayout layout =
      media::GuessChannelLayout(number_of_output_channels);

  // Use "discrete" channel layout when the best guess was not successful.
  if (layout == media::CHANNEL_LAYOUT_UNSUPPORTED) {
    layout = media::CHANNEL_LAYOUT_DISCRETE;
  }

  return RendererWebAudioDeviceImpl::Create(sink_descriptor, layout,
                                            number_of_output_channels,
                                            latency_hint, callback);
}

bool RendererBlinkPlatformImpl::DecodeAudioFileData(
    blink::WebAudioBus* destination_bus,
    const char* audio_file_data,
    size_t data_size) {
  return content::DecodeAudioFileData(destination_bus, audio_file_data,
                                      data_size);
}

//------------------------------------------------------------------------------

scoped_refptr<media::AudioCapturerSource>
RendererBlinkPlatformImpl::NewAudioCapturerSource(
    blink::WebLocalFrame* web_frame,
    const media::AudioSourceParameters& params) {
  return blink::AudioDeviceFactory::GetInstance()->NewAudioCapturerSource(
      web_frame, params);
}

scoped_refptr<viz::RasterContextProvider>
RendererBlinkPlatformImpl::SharedMainThreadContextProvider() {
  return RenderThreadImpl::current()->SharedMainThreadContextProvider();
}

scoped_refptr<cc::RasterContextProviderWrapper>
RendererBlinkPlatformImpl::SharedCompositorWorkerContextProvider(
    cc::RasterDarkModeFilter* dark_mode_filter) {
  return RenderThreadImpl::current()->SharedCompositorWorkerContextProvider(
      dark_mode_filter);
}

scoped_refptr<gpu::GpuChannelHost>
RendererBlinkPlatformImpl::EstablishGpuChannelSync() {
  return RenderThreadImpl::current()->EstablishGpuChannelSync();
}

void RendererBlinkPlatformImpl::EstablishGpuChannel(
    EstablishGpuChannelCallback callback) {
  RenderThreadImpl::current()->EstablishGpuChannel(std::move(callback));
}

bool RendererBlinkPlatformImpl::RTCSmoothnessAlgorithmEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableRTCSmoothnessAlgorithm);
}

//------------------------------------------------------------------------------

absl::optional<double>
RendererBlinkPlatformImpl::GetWebRtcMaxCaptureFrameRate() {
  const std::string max_fps_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWebRtcMaxCaptureFramerate);
  if (!max_fps_str.empty()) {
    double value;
    if (base::StringToDouble(max_fps_str, &value) && value >= 0.0)
      return value;
  }
  return absl::nullopt;
}

scoped_refptr<media::AudioRendererSink>
RendererBlinkPlatformImpl::NewAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    blink::WebLocalFrame* web_frame,
    const media::AudioSinkParameters& params) {
  return blink::AudioDeviceFactory::GetInstance()->NewAudioRendererSink(
      source_type, web_frame->GetLocalFrameToken(), params);
}

media::AudioLatency::LatencyType
RendererBlinkPlatformImpl::GetAudioSourceLatencyType(
    blink::WebAudioDeviceSourceType source_type) {
  return blink::AudioDeviceFactory::GetSourceLatencyType(source_type);
}

bool RendererBlinkPlatformImpl::ShouldEnforceWebRTCRoutingPreferences() {
  return GetContentClient()
      ->renderer()
      ->ShouldEnforceWebRTCRoutingPreferences();
}

bool RendererBlinkPlatformImpl::UsesFakeCodecForPeerConnection() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeCodecForPeerConnection);
}

bool RendererBlinkPlatformImpl::IsWebRtcEncryptionEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebRtcEncryption);
}

media::MediaPermission* RendererBlinkPlatformImpl::GetWebRTCMediaPermission(
    blink::WebLocalFrame* web_frame) {
  DCHECK(ShouldEnforceWebRTCRoutingPreferences());

  media::MediaPermission* media_permission = nullptr;
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(web_frame);
  if (render_frame)
    media_permission = render_frame->GetMediaPermission();
  DCHECK(media_permission);
  return media_permission;
}

void RendererBlinkPlatformImpl::GetWebRTCRendererPreferences(
    blink::WebLocalFrame* web_frame,
    blink::WebString* ip_handling_policy,
    uint16_t* udp_min_port,
    uint16_t* udp_max_port,
    bool* allow_mdns_obfuscation) {
  DCHECK(ip_handling_policy);
  DCHECK(udp_min_port);
  DCHECK(udp_max_port);
  DCHECK(allow_mdns_obfuscation);

  auto* render_frame = RenderFrameImpl::FromWebFrame(web_frame);
  if (!render_frame)
    return;

  *ip_handling_policy = blink::WebString::FromUTF8(
      render_frame->GetRendererPreferences().webrtc_ip_handling_policy);
  *udp_min_port = render_frame->GetRendererPreferences().webrtc_udp_min_port;
  *udp_max_port = render_frame->GetRendererPreferences().webrtc_udp_max_port;
  const std::vector<std::string>& allowed_urls =
      render_frame->GetRendererPreferences().webrtc_local_ips_allowed_urls;
  const std::string url(web_frame->GetSecurityOrigin().ToString().Utf8());
  for (const auto& allowed_url : allowed_urls) {
    if (base::MatchPattern(url, allowed_url)) {
      *allow_mdns_obfuscation = false;
      return;
    }
  }
  *allow_mdns_obfuscation = true;
}

bool RendererBlinkPlatformImpl::IsWebRtcHWEncodingEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebRtcHWEncoding);
}

bool RendererBlinkPlatformImpl::IsWebRtcHWDecodingEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebRtcHWDecoding);
}

bool RendererBlinkPlatformImpl::IsWebRtcSrtpAesGcmEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebRtcSrtpAesGcm);
}

bool RendererBlinkPlatformImpl::IsWebRtcSrtpEncryptedHeadersEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebRtcSrtpEncryptedHeaders);
}

bool RendererBlinkPlatformImpl::AllowsLoopbackInPeerConnection() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAllowLoopbackInPeerConnection);
}

blink::WebVideoCaptureImplManager*
RendererBlinkPlatformImpl::GetVideoCaptureImplManager() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->video_capture_impl_manager() : nullptr;
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::Collect3DContextInformation(
    blink::Platform::GraphicsInfo* gl_info,
    const gpu::GPUInfo& gpu_info) const {
  DCHECK(gl_info);
  const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
  gl_info->vendor_id = active_gpu.vendor_id;
  gl_info->device_id = active_gpu.device_id;
  gl_info->renderer_info = WebString::FromUTF8(gpu_info.gl_renderer);
  gl_info->vendor_info = WebString::FromUTF8(gpu_info.gl_vendor);
  gl_info->driver_version = WebString::FromUTF8(active_gpu.driver_version);
  gl_info->reset_notification_strategy =
      gpu_info.gl_reset_notification_strategy;
  gl_info->sandboxed = gpu_info.sandboxed;
  gl_info->amd_switchable = gpu_info.amd_switchable;
  gl_info->optimus = gpu_info.optimus;
  gl_info->using_gpu_compositing = !IsGpuCompositingDisabled();
  gl_info->using_passthrough_command_decoder = gpu_info.passthrough_cmd_decoder;
}

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateOffscreenGraphicsContext3DProvider(
    const blink::Platform::ContextAttributes& web_attributes,
    const blink::WebURL& top_document_web_url,
    blink::Platform::GraphicsInfo* gl_info) {
  DCHECK(gl_info);
  if (!RenderThreadImpl::current()) {
    std::string error_message("Failed to run in Current RenderThreadImpl");
    gl_info->error_message = WebString::FromUTF8(error_message);
    return nullptr;
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    std::string error_message(
        "OffscreenContext Creation failed, GpuChannelHost creation failed");
    gl_info->error_message = WebString::FromUTF8(error_message);
    return nullptr;
  }
  Collect3DContextInformation(gl_info, gpu_channel_host->gpu_info());

  // This is an offscreen context. Generally it won't use the default
  // frame buffer, in that case don't request any alpha, depth, stencil,
  // antialiasing. But we do need those attributes for the "own
  // offscreen surface" optimization which supports directly drawing
  // to a custom surface backed frame buffer.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = web_attributes.support_alpha ? 8 : -1;
  attributes.depth_size = web_attributes.support_depth ? 24 : 0;
  attributes.stencil_size = web_attributes.support_stencil ? 8 : 0;
  attributes.samples = web_attributes.support_antialias ? 4 : 0;
  attributes.own_offscreen_surface =
      web_attributes.support_alpha || web_attributes.support_depth ||
      web_attributes.support_stencil || web_attributes.support_antialias;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.enable_raster_interface = web_attributes.enable_raster_interface;
  attributes.enable_oop_rasterization =
      attributes.enable_raster_interface &&
      gpu_channel_host->gpu_feature_info()
              .status_values[gpu::GPU_FEATURE_TYPE_CANVAS_OOP_RASTERIZATION] ==
          gpu::kGpuFeatureStatusEnabled;
  attributes.enable_gles2_interface = !attributes.enable_oop_rasterization;

  attributes.gpu_preference = web_attributes.prefer_low_power_gpu
                                  ? gl::GpuPreference::kLowPower
                                  : gl::GpuPreference::kHighPerformance;

  attributes.fail_if_major_perf_caveat =
      web_attributes.fail_if_major_performance_caveat;

  attributes.context_type = ToGpuContextType(web_attributes.context_type);

  constexpr bool automatic_flushes = true;
  constexpr bool support_locking = false;
  bool use_grcontext =
      !attributes.enable_oop_rasterization && web_attributes.support_grcontext;

  scoped_refptr<viz::ContextProviderCommandBuffer> provider(
      new viz::ContextProviderCommandBuffer(
          std::move(gpu_channel_host),
          RenderThreadImpl::current()->GetGpuMemoryBufferManager(),
          kGpuStreamIdDefault, kGpuStreamPriorityDefault,
          gpu::kNullSurfaceHandle, GURL(top_document_web_url),
          automatic_flushes, support_locking, use_grcontext,
          gpu::SharedMemoryLimits(), attributes,
          viz::command_buffer_metrics::ContextType::WEBGL));
  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      std::move(provider));
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateSharedOffscreenGraphicsContext3DProvider() {
  auto* thread = RenderThreadImpl::current();

  scoped_refptr<viz::ContextProviderCommandBuffer> provider =
      thread->SharedMainThreadContextProvider();
  if (!provider)
    return nullptr;

  scoped_refptr<gpu::GpuChannelHost> host = thread->EstablishGpuChannelSync();
  // This shouldn't normally fail because we just got |provider|. But the
  // channel can become lost on the IO thread since then. It is important that
  // this happens after getting |provider|. In the case that this GpuChannelHost
  // is not the same one backing |provider|, the context behind the |provider|
  // will be already lost/dead on arrival.
  if (!host)
    return nullptr;

  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      std::move(provider));
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebGraphicsContext3DProvider>
RendererBlinkPlatformImpl::CreateWebGPUGraphicsContext3DProvider(
    const blink::WebURL& top_document_web_url) {
#if !BUILDFLAG(USE_DAWN)
  return nullptr;
#else
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host(
      RenderThreadImpl::current()->EstablishGpuChannelSync());
  if (!gpu_channel_host) {
    // TODO(crbug.com/973017): Collect GPU info and surface context creation
    // error.
    return nullptr;
  }

  gpu::ContextCreationAttribs attributes;
  // TODO(kainino): It's not clear yet how GPU preferences work for WebGPU.
  attributes.gpu_preference = gl::GpuPreference::kHighPerformance;
  attributes.enable_gles2_interface = false;
  attributes.context_type = gpu::CONTEXT_TYPE_WEBGPU;

  constexpr bool automatic_flushes = true;
  constexpr bool support_locking = false;
  constexpr bool support_grcontext = true;

  // WebGPU GPUBuffers, which are backed by shared memory transfer buffers, may
  // be accessed as ArrayBuffers from JavaScript. As such, the underlying
  // buffers need to be mapped using the ArrayBuffer shared memory mapper. As
  // there is currently no way of specifying a custom mapper per buffer, we
  // have to map all buffers created by this provider using the custom mapper.
  // TODO(crbug.com/1321521) instead of mapping all buffers created by this
  // provider with the array buffer mapper, only map those that will actually
  // be used as ArrayBuffers and remove this per-provider mapper again.
  base::SharedMemoryMapper* buffer_mapper =
      gin::GetSharedMemoryMapperForArrayBuffers();

  scoped_refptr<viz::ContextProviderCommandBuffer> provider(
      new viz::ContextProviderCommandBuffer(
          std::move(gpu_channel_host),
          RenderThreadImpl::current()->GetGpuMemoryBufferManager(),
          kGpuStreamIdDefault, kGpuStreamPriorityDefault,
          gpu::kNullSurfaceHandle, GURL(top_document_web_url),
          automatic_flushes, support_locking, support_grcontext,
          gpu::SharedMemoryLimits::ForWebGPUContext(), attributes,
          viz::command_buffer_metrics::ContextType::WEBGPU, buffer_mapper));
  return std::make_unique<WebGraphicsContext3DProviderImpl>(
      std::move(provider));
#endif
}

//------------------------------------------------------------------------------

gpu::GpuMemoryBufferManager*
RendererBlinkPlatformImpl::GetGpuMemoryBufferManager() {
  RenderThreadImpl* thread = RenderThreadImpl::current();
  return thread ? thread->GetGpuMemoryBufferManager() : nullptr;
}

//------------------------------------------------------------------------------

blink::WebString RendererBlinkPlatformImpl::ConvertIDNToUnicode(
    const blink::WebString& host) {
  return WebString::FromUTF16(url_formatter::IDNToUnicode(host.Ascii()));
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebDedicatedWorkerHostFactoryClient>
RendererBlinkPlatformImpl::CreateDedicatedWorkerHostFactoryClient(
    blink::WebDedicatedWorker* worker,
    const blink::BrowserInterfaceBrokerProxy& interface_broker) {
  return std::make_unique<DedicatedWorkerHostFactoryClient>(worker,
                                                            interface_broker);
}

void RendererBlinkPlatformImpl::DidStartWorkerThread() {
  WorkerThreadRegistry::Instance()->DidStartCurrentWorkerThread();
}

void RendererBlinkPlatformImpl::WillStopWorkerThread() {
  WorkerThreadRegistry::Instance()->WillStopCurrentWorkerThread();
}

void RendererBlinkPlatformImpl::WorkerContextCreated(
    const v8::Local<v8::Context>& worker) {
  GetContentClient()->renderer()->DidInitializeWorkerContextOnWorkerThread(
      worker);
}

bool RendererBlinkPlatformImpl::AllowScriptExtensionForServiceWorker(
    const blink::WebSecurityOrigin& script_origin) {
  return GetContentClient()->renderer()->AllowScriptExtensionForServiceWorker(
      script_origin);
}

blink::ProtocolHandlerSecurityLevel
RendererBlinkPlatformImpl::GetProtocolHandlerSecurityLevel() {
  return GetContentClient()->renderer()->GetProtocolHandlerSecurityLevel();
}

bool RendererBlinkPlatformImpl::OriginCanAccessServiceWorkers(
    const blink::WebURL& url) {
  return content::OriginCanAccessServiceWorkers(url);
}

std::tuple<blink::CrossVariantMojoRemote<
               blink::mojom::ServiceWorkerContainerHostInterfaceBase>,
           blink::CrossVariantMojoRemote<
               blink::mojom::ServiceWorkerContainerHostInterfaceBase>>
RendererBlinkPlatformImpl::CloneServiceWorkerContainerHost(
    blink::CrossVariantMojoRemote<
        blink::mojom::ServiceWorkerContainerHostInterfaceBase>
        service_worker_container_host) {
  mojo::Remote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host_remote(
          std::move(service_worker_container_host));
  mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
      service_worker_container_host_pending_remote;

  service_worker_container_host_remote->CloneContainerHost(
      service_worker_container_host_pending_remote
          .InitWithNewPipeAndPassReceiver());
  return std::make_tuple(
      service_worker_container_host_remote.Unbind(),
      std::move(service_worker_container_host_pending_remote));
}

void RendererBlinkPlatformImpl::CreateServiceWorkerSubresourceLoaderFactory(
    blink::CrossVariantMojoRemote<
        blink::mojom::ServiceWorkerContainerHostInterfaceBase>
        service_worker_container_host,
    const blink::WebString& client_id,
    blink::mojom::ServiceWorkerFetchHandlerBypassOption
        fetch_handler_bypass_option,
    std::unique_ptr<network::PendingSharedURLLoaderFactory> fallback_factory,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  ServiceWorkerSubresourceLoaderFactory::Create(
      base::MakeRefCounted<ControllerServiceWorkerConnector>(
          std::move(service_worker_container_host),
          /*remote_controller=*/mojo::NullRemote(), client_id.Utf8(),
          fetch_handler_bypass_option),
      network::SharedURLLoaderFactory::Create(std::move(fallback_factory)),
      std::move(receiver), std::move(task_runner));
}

//------------------------------------------------------------------------------

// The returned BatchingMediaLog can be used on any thread, but must be
// destroyed on |owner_task_runner|. The aggregated MediaLogRecords will be
// sent back to the Browser via Mojo objects bound to |owner_task_runner|.
std::unique_ptr<media::MediaLog> RendererBlinkPlatformImpl::GetMediaLog(
    blink::MediaInspectorContext* inspector_context,
    scoped_refptr<base::SingleThreadTaskRunner> owner_task_runner,
    bool is_on_worker) {
  std::vector<std::unique_ptr<BatchingMediaLog::EventHandler>> handlers;

  // For chrome://media-internals.
  // This should only be created in the main Window context, and not from
  // a worker context.
  if (!is_on_worker)
    handlers.push_back(std::make_unique<RenderMediaEventHandler>(
        media::GetNextMediaPlayerLoggingID()));

  // For devtools' media tab.
  handlers.push_back(
      std::make_unique<InspectorMediaEventHandler>(inspector_context));

  return std::make_unique<BatchingMediaLog>(owner_task_runner,
                                            std::move(handlers));
}

//------------------------------------------------------------------------------
media::GpuVideoAcceleratorFactories*
RendererBlinkPlatformImpl::GetGpuFactories() {
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return nullptr;

  return render_thread->GetGpuFactories();
}

scoped_refptr<base::SequencedTaskRunner>
RendererBlinkPlatformImpl::MediaThreadTaskRunner() {
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return nullptr;

  return render_thread->GetMediaSequencedTaskRunner();
}

base::WeakPtr<media::DecoderFactory>
RendererBlinkPlatformImpl::GetMediaDecoderFactory() {
  blink::WebLocalFrame* const web_frame =
      blink::WebLocalFrame::FrameForCurrentContext();
  RenderFrameImpl* render_frame = RenderFrameImpl::FromWebFrame(web_frame);
  return render_frame->GetMediaDecoderFactory();
}

void RendererBlinkPlatformImpl::SetRenderingColorSpace(
    const gfx::ColorSpace& color_space) {
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return;

  render_thread->SetRenderingColorSpace(color_space);
}

gfx::ColorSpace RendererBlinkPlatformImpl::GetRenderingColorSpace() const {
  auto* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return {};

  return render_thread->GetRenderingColorSpace();
}

//------------------------------------------------------------------------------

void RendererBlinkPlatformImpl::SetActiveURL(const blink::WebURL& url,
                                             const blink::WebString& top_url) {
  GetContentClient()->SetActiveURL(url, top_url.Utf8());
}

//------------------------------------------------------------------------------

SkBitmap* RendererBlinkPlatformImpl::GetSadPageBitmap() {
  return GetContentClient()->renderer()->GetSadWebViewBitmap();
}

//------------------------------------------------------------------------------

std::unique_ptr<blink::WebV8ValueConverter>
RendererBlinkPlatformImpl::CreateWebV8ValueConverter() {
  return std::make_unique<V8ValueConverterImpl>();
}

void RendererBlinkPlatformImpl::AppendContentSecurityPolicy(
    const blink::WebURL& url,
    blink::WebVector<blink::WebContentSecurityPolicyHeader>* csp) {
  GetContentClient()->renderer()->AppendContentSecurityPolicy(url, csp);
}

base::PlatformThreadId RendererBlinkPlatformImpl::GetIOThreadId() const {
  auto io_task_runner = GetIOTaskRunner();
  if (!io_task_runner)
    return base::kInvalidThreadId;
  // Cannot be called from IO thread due to potential deadlock.
  CHECK(!io_task_runner->BelongsToCurrentThread());
  {
    base::ScopedAllowBaseSyncPrimitives allow;
    io_thread_id_ready_event_.Wait();
  }
  return io_thread_id_;
}

network::mojom::AttributionSupport
RendererBlinkPlatformImpl::GetAttributionReportingSupport() {
  auto* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl is null in some tests.
  if (!render_thread)
    return network::mojom::AttributionSupport::kWeb;
  return render_thread->GetAttributionReportingSupport();
}

scoped_refptr<base::SingleThreadTaskRunner>
RendererBlinkPlatformImpl::VideoFrameCompositorTaskRunner() {
  auto compositor_task_runner = CompositorThreadTaskRunner();
  if (features::UseSurfaceLayerForVideo() || !compositor_task_runner) {
    if (!video_frame_compositor_thread_) {
      // All of Chromium's GPU code must know which thread it's running on, and
      // be the same thread on which the rendering context was initialized. This
      // is why this must be a SingleThreadTaskRunner instead of a
      // SequencedTaskRunner.
      video_frame_compositor_thread_ =
          std::make_unique<base::Thread>("VideoFrameCompositor");
      video_frame_compositor_thread_->StartWithOptions(
          base::Thread::Options(base::ThreadType::kCompositing));
    }

    return video_frame_compositor_thread_->task_runner();
  }
  return compositor_task_runner;
}

#if BUILDFLAG(IS_ANDROID)
void RendererBlinkPlatformImpl::SetPrivateMemoryFootprint(
    uint64_t private_memory_footprint_bytes) {
  auto* render_thread = RenderThreadImpl::current();
  CHECK(render_thread);
  render_thread->SetPrivateMemoryFootprint(private_memory_footprint_bytes);
}

bool RendererBlinkPlatformImpl::IsUserLevelMemoryPressureSignalEnabled() {
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUserLevelMemoryPressureSignalParams);
  return enabled;
}

std::pair<base::TimeDelta, base::TimeDelta> RendererBlinkPlatformImpl::
    InertAndMinimumIntervalOfUserLevelMemoryPressureSignal() {
  constexpr std::pair<base::TimeDelta, base::TimeDelta>
      kDefaultInertAndMinInterval =
          std::make_pair(base::TimeDelta::Min(), base::Minutes(10));

  if (!IsUserLevelMemoryPressureSignalEnabled()) {
    return kDefaultInertAndMinInterval;
  }

  std::vector<base::StringPiece> parameters = base::SplitStringPiece(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUserLevelMemoryPressureSignalParams),
      ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parameters.size() != 2) {
    return kDefaultInertAndMinInterval;
  }

  absl::optional<base::TimeDelta> inert_interval =
      base::TimeDeltaFromString(parameters.at(0));
  absl::optional<base::TimeDelta> minimum_interval =
      base::TimeDeltaFromString(parameters.at(1));
  if (!inert_interval.has_value() || !minimum_interval.has_value()) {
    return kDefaultInertAndMinInterval;
  }

  return std::make_pair(inert_interval.value(), minimum_interval.value());
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
