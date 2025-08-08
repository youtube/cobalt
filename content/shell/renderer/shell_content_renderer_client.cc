// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_content_renderer_client.h"

#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cdm/renderer/external_clear_key_key_system_info.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/pseudonymization_util.h"
#include "content/public/common/web_identity.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/test/test_service.mojom.h"
#include "content/shell/common/main_frame_counter_test_impl.h"
#include "content/shell/common/power_monitor_test_impl.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_render_frame_observer.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/policy/sandbox.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/web/modules/credentialmanagement/throttle_helper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#endif

namespace content {

namespace {

// A test service which can be driven by browser tests for various reasons.
class TestRendererServiceImpl : public mojom::TestService {
 public:
  explicit TestRendererServiceImpl(
      mojo::PendingReceiver<mojom::TestService> receiver)
      : receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestRendererServiceImpl::OnConnectionError, base::Unretained(this)));
  }

  TestRendererServiceImpl(const TestRendererServiceImpl&) = delete;
  TestRendererServiceImpl& operator=(const TestRendererServiceImpl&) = delete;

  ~TestRendererServiceImpl() override {}

 private:
  void OnConnectionError() { delete this; }

  // mojom::TestService:
  void DoSomething(DoSomethingCallback callback) override {
    // Instead of responding normally, unbind the pipe, write some garbage,
    // and go away.
    const std::string kBadMessage = "This is definitely not a valid response!";
    mojo::ScopedMessagePipeHandle pipe = receiver_.Unbind().PassPipe();
    MojoResult rv = mojo::WriteMessageRaw(
        pipe.get(), kBadMessage.data(), kBadMessage.size(), nullptr, 0,
        MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(rv, MOJO_RESULT_OK);

    // Deletes this.
    OnConnectionError();
  }

  void DoTerminateProcess(DoTerminateProcessCallback callback) override {
    NOTREACHED();
  }

  void DoCrashImmediately(DoCrashImmediatelyCallback callback) override {
    // This intentionally crashes the process and needs to be fatal regardless
    // of DCHECK level. It's intended to get called. This is unlike the other
    // NOTREACHED()s which are not expected to get called at all.
    CHECK(false);
  }

  void CreateFolder(CreateFolderCallback callback) override { NOTREACHED(); }

  void GetRequestorName(GetRequestorNameCallback callback) override {
    std::move(callback).Run("Not implemented.");
  }

  void CreateReadOnlySharedMemoryRegion(
      const std::string& message,
      CreateReadOnlySharedMemoryRegionCallback callback) override {
    NOTREACHED();
  }

  void CreateWritableSharedMemoryRegion(
      const std::string& message,
      CreateWritableSharedMemoryRegionCallback callback) override {
    NOTREACHED();
  }

  void CreateUnsafeSharedMemoryRegion(
      const std::string& message,
      CreateUnsafeSharedMemoryRegionCallback callback) override {
    NOTREACHED();
  }

  void CloneSharedMemoryContents(
      base::ReadOnlySharedMemoryRegion region,
      CloneSharedMemoryContentsCallback callback) override {
    NOTREACHED();
  }

  void IsProcessSandboxed(IsProcessSandboxedCallback callback) override {
    std::move(callback).Run(sandbox::policy::Sandbox::IsProcessSandboxed());
  }

  void PseudonymizeString(const std::string& value,
                          PseudonymizeStringCallback callback) override {
    std::move(callback).Run(
        PseudonymizationUtil::PseudonymizeStringForTesting(value));
  }

  void PassWriteableFile(base::File file,
                         PassWriteableFileCallback callback) override {
    std::move(callback).Run();
  }

  void WriteToPreloadedPipe() override { NOTREACHED(); }

  mojo::Receiver<mojom::TestService> receiver_;
};

class ShellContentRendererUrlLoaderThrottleProvider
    : public blink::URLLoaderThrottleProvider {
 public:
  std::unique_ptr<URLLoaderThrottleProvider> Clone() override {
    return std::make_unique<ShellContentRendererUrlLoaderThrottleProvider>();
  }

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override {
    blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    // Workers can call us on a background thread. We don't care about such
    // requests because we purposefully only look at resources from frames
    // that the user can interact with.`
    content::RenderFrame* frame =
        RenderThread::IsMainThread()
            ? content::RenderFrame::FromRoutingID(render_frame_id)
            : nullptr;
    if (frame) {
      auto throttle = content::MaybeCreateIdentityUrlLoaderThrottle(
          base::BindRepeating(blink::SetIdpSigninStatus, frame->GetWebFrame()));
      if (throttle)
        throttles.push_back(std::move(throttle));
    }

    return throttles;
  }

  void SetOnline(bool is_online) override {}
};

void CreateRendererTestService(
    mojo::PendingReceiver<mojom::TestService> receiver) {
  // Owns itself.
  new TestRendererServiceImpl(std::move(receiver));
}

}  // namespace

ShellContentRendererClient::ShellContentRendererClient() {}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();
}

void ShellContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  binders->Add<mojom::TestService>(
      base::BindRepeating(&CreateRendererTestService),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<mojom::PowerMonitorTest>(
      base::BindRepeating(&PowerMonitorTestImpl::MakeSelfOwnedReceiver),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<mojom::MainFrameCounterTest>(
      base::BindRepeating(&MainFrameCounterTestImpl::Bind),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<web_cache::mojom::WebCache>(
      base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                          base::Unretained(web_cache_impl_.get())),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void ShellContentRendererClient::RenderFrameCreated(RenderFrame* render_frame) {
  // TODO(danakj): The ShellRenderFrameObserver is doing stuff only for
  // browser tests. If we only create that for browser tests then the override
  // of this method in WebTestContentRendererClient would not be needed.
  new ShellRenderFrameObserver(render_frame);
}

void ShellContentRendererClient::PrepareErrorPage(
    RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (error_html && error_html->empty()) {
    *error_html =
        "<head><title>Error</title></head><body>Could not load the requested "
        "resource.<br/>Error code: " +
        base::NumberToString(error.reason()) +
        (error.reason() < 0 ? " (" + net::ErrorToString(error.reason()) + ")"
                            : "") +
        "</body>";
  }
}

void ShellContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    int http_status,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (error_html) {
    *error_html =
        "<head><title>Error</title></head><body>Server returned HTTP status " +
        base::NumberToString(http_status) + "</body>";
  }
}

void ShellContentRendererClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::InjectInternalsObject(context);
  }
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
ShellContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<ShellContentRendererUrlLoaderThrottleProvider>();
}

#if BUILDFLAG(ENABLE_MOJO_CDM)
void ShellContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  media::KeySystemInfos key_systems;
  if (base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting))
    key_systems.push_back(
        std::make_unique<cdm::ExternalClearKeyKeySystemInfo>());
  std::move(cb).Run(std::move(key_systems));
}
#endif

std::unique_ptr<blink::WebPrescientNetworking>
ShellContentRendererClient::CreatePrescientNetworking(
    RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

}  // namespace content
