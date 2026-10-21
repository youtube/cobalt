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

#include "cobalt/testing/browser_tests/utility/shell_content_utility_client.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cobalt/testing/browser_tests/common/power_monitor_test_impl.h"
#include "components/services/storage/test_api/test_api.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pseudonymization_util.h"
#include "content/public/test/test_service.mojom.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "mojo/public/cpp/system/buffer.h"
#include "sandbox/policy/sandbox.h"
#include "services/test/echo/echo_service.h"

#if BUILDFLAG(IS_LINUX)
#include "content/test/sandbox_status_service.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/file_descriptor_store.h"
#endif

namespace content {

namespace {

class TestUtilityServiceImpl : public mojom::TestService {
 public:
  explicit TestUtilityServiceImpl(
      mojo::PendingReceiver<mojom::TestService> receiver)
      : receiver_(this, std::move(receiver)) {}

  TestUtilityServiceImpl(const TestUtilityServiceImpl&) = delete;
  TestUtilityServiceImpl& operator=(const TestUtilityServiceImpl&) = delete;

  ~TestUtilityServiceImpl() override = default;

  // mojom::TestService implementation:
  void DoSomething(DoSomethingCallback callback) override {
    std::move(callback).Run();
  }

  void DoTerminateProcess(DoTerminateProcessCallback callback) override {
    base::Process::TerminateCurrentProcessImmediately(0);
  }

  void DoCrashImmediately(DoCrashImmediatelyCallback callback) override {
    base::ImmediateCrash();
  }

  void CreateFolder(CreateFolderCallback callback) override {
    // Note: This is used to check if the sandbox is disabled or not since
    //       creating a folder is forbidden when it is enabled.
    std::move(callback).Run(base::ScopedTempDir().CreateUniqueTempDir());
  }

  void GetRequestorName(GetRequestorNameCallback callback) override {
    NOTREACHED();
  }

  void CreateReadOnlySharedMemoryRegion(
      const std::string& message,
      CreateReadOnlySharedMemoryRegionCallback callback) override {
    base::MappedReadOnlyRegion map_and_region =
        base::ReadOnlySharedMemoryRegion::Create(message.size());
    CHECK(map_and_region.IsValid());
    std::ranges::copy(message,
                      map_and_region.mapping.GetMemoryAsSpan<char>().begin());
    std::move(callback).Run(std::move(map_and_region.region));
  }

  void CreateWritableSharedMemoryRegion(
      const std::string& message,
      CreateWritableSharedMemoryRegionCallback callback) override {
    auto region = base::WritableSharedMemoryRegion::Create(message.size());
    CHECK(region.IsValid());
    base::WritableSharedMemoryMapping mapping = region.Map();
    CHECK(mapping.IsValid());
    std::ranges::copy(message, mapping.GetMemoryAsSpan<char>().begin());
    std::move(callback).Run(std::move(region));
  }

  void CreateUnsafeSharedMemoryRegion(
      const std::string& message,
      CreateUnsafeSharedMemoryRegionCallback callback) override {
    auto region = base::UnsafeSharedMemoryRegion::Create(message.size());
    CHECK(region.IsValid());
    base::WritableSharedMemoryMapping mapping = region.Map();
    CHECK(mapping.IsValid());
    std::ranges::copy(message, mapping.GetMemoryAsSpan<char>().begin());
    std::move(callback).Run(std::move(region));
  }

  void CloneSharedMemoryContents(
      base::ReadOnlySharedMemoryRegion region,
      CloneSharedMemoryContentsCallback callback) override {
    auto mapping = region.Map();
    auto new_region = base::UnsafeSharedMemoryRegion::Create(region.GetSize());
    auto new_mapping = new_region.Map();
    memcpy(new_mapping.memory(), mapping.memory(), region.GetSize());
    std::move(callback).Run(std::move(new_region));
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

  void WriteToPreloadedPipe() override {
#if BUILDFLAG(IS_POSIX)
    base::MemoryMappedFile::Region region;
    base::ScopedFD write_pipe = base::FileDescriptorStore::GetInstance().TakeFD(
        mojom::kTestPipeKey, &region);
    CHECK(write_pipe.is_valid());
    CHECK(region == base::MemoryMappedFile::Region::kWholeFile);
    CHECK(base::WriteFileDescriptor(write_pipe.get(), "test"));
#else
    NOTREACHED();
#endif
  }

 private:
  mojo::Receiver<mojom::TestService> receiver_;
};

auto RunTestService(mojo::PendingReceiver<mojom::TestService> receiver) {
  return std::make_unique<TestUtilityServiceImpl>(std::move(receiver));
}

auto RunEchoService(mojo::PendingReceiver<echo::mojom::EchoService> receiver) {
  return std::make_unique<echo::EchoService>(std::move(receiver));
}

}  // namespace

ShellContentUtilityClient::ShellContentUtilityClient() {
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType) == switches::kUtilityProcess) {
    network_service_test_helper_ = NetworkServiceTestHelper::Create();
    audio_service_test_helper_ = std::make_unique<AudioServiceTestHelper>();
    storage::InjectTestApiImplementation();
    register_sandbox_status_helper_ = true;
  }
}

ShellContentUtilityClient::~ShellContentUtilityClient() = default;

void ShellContentUtilityClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  binders->Add<mojom::PowerMonitorTest>(
      base::BindRepeating(&PowerMonitorTestImpl::MakeSelfOwnedReceiver),
      base::SingleThreadTaskRunner::GetCurrentDefault());
#if BUILDFLAG(IS_LINUX)
  if (register_sandbox_status_helper_) {
    binders->Add<content::mojom::SandboxStatusService>(
        base::BindRepeating(
            &content::SandboxStatusService::MakeSelfOwnedReceiver),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
#endif
}

void ShellContentUtilityClient::RegisterIOThreadServices(
    mojo::ServiceFactory& services) {
  services.Add(RunTestService);
  services.Add(RunEchoService);
}

}  // namespace content
