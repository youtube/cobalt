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

#include "cobalt/shell/utility/shell_content_utility_client.h"

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
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cobalt/shell/common/power_monitor_test_impl.h"
#include "components/services/storage/test_api/test_api.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/pseudonymization_util.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "mojo/public/cpp/system/buffer.h"
#include "sandbox/policy/sandbox.h"

#if BUILDFLAG(IS_LINUX)
#include "content/test/sandbox_status_service.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/file_descriptor_store.h"
#endif

namespace content {

ShellContentUtilityClient::ShellContentUtilityClient(bool is_browsertest) {
  if (is_browsertest &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
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
    mojo::ServiceFactory& services) {}

}  // namespace content
