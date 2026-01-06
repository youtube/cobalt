// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/testing/browser_tests/renderer/shell_content_renderer_test_client.h"

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "cobalt/testing/browser_tests/common/main_frame_counter_test_impl.h"
#include "cobalt/testing/browser_tests/common/power_monitor_test_impl.h"
#include "cobalt/testing/browser_tests/common/shell_test_switches.h"
#include "cobalt/testing/browser_tests/renderer/shell_render_frame_observer.h"
#include "content/public/common/pseudonymization_util.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "sandbox/policy/sandbox.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "v8/include/v8.h"

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

  ~TestRendererServiceImpl() override = default;

 private:
  void OnConnectionError() { delete this; }

  // mojom::TestService:
  void DoSomething(DoSomethingCallback callback) override {
    // Instead of responding normally, unbind the pipe, write some garbage,
    // and go away.
    const std::string kBadMessage = "This is definitely not a valid response!";
    mojo::ScopedMessagePipeHandle pipe = receiver_.Unbind().PassPipe();
    MojoResult rv = mojo::WriteMessageRaw(pipe.get(), kBadMessage.data(),
                                          kBadMessage.size(), nullptr, 0,
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

void CreateRendererTestService(
    mojo::PendingReceiver<mojom::TestService> receiver) {
  // Owns itself.
  new TestRendererServiceImpl(std::move(receiver));
}

}  // namespace

ShellContentRendererTestClient::ShellContentRendererTestClient() = default;

ShellContentRendererTestClient::~ShellContentRendererTestClient() = default;

void ShellContentRendererTestClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  ShellContentRendererClient::ExposeInterfacesToBrowser(binders);

  binders->Add<mojom::TestService>(
      base::BindRepeating(&CreateRendererTestService),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<mojom::PowerMonitorTest>(
      base::BindRepeating(&PowerMonitorTestImpl::MakeSelfOwnedReceiver),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<mojom::MainFrameCounterTest>(
      base::BindRepeating(&MainFrameCounterTestImpl::Bind),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void ShellContentRendererTestClient::RenderFrameCreated(
    RenderFrame* render_frame) {
  // TODO(danakj): The ShellRenderFrameObserver is doing stuff only for
  // browser tests. If we only create that for browser tests then the override
  // of this method in WebTestContentRendererClient would not be needed.
  new ShellRenderFrameObserver(render_frame);
}

void ShellContentRendererTestClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::InjectInternalsObject(context);
  }
}

}  // namespace content
