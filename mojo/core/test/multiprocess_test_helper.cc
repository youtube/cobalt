// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/test/multiprocess_test_helper.h"

#include <functional>
#include <set>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "mojo/public/cpp/platform/named_platform_channel.h"
#endif

namespace mojo {
namespace core {
namespace test {

namespace {

#if !BUILDFLAG(IS_FUCHSIA)
const char kNamedPipeName[] = "named-pipe-name";
#endif
const char kRunAsBrokerClient[] = "run-as-broker-client";
const char kAcceptInvitationAsync[] = "accept-invitation-async";
const char kTestChildMessagePipeName[] = "test_pipe";
const char kDisableAllCapabilities[] = "disable-all-capabilities";

// For use (and only valid) in a test child process:
base::LazyInstance<IsolatedConnection>::Leaky g_child_isolated_connection;

int RunClientFunction(base::OnceCallback<int(MojoHandle)> handler,
                      bool pass_pipe_ownership_to_main) {
  CHECK(MultiprocessTestHelper::primordial_pipe.is_valid());
  ScopedMessagePipeHandle pipe =
      std::move(MultiprocessTestHelper::primordial_pipe);
  MessagePipeHandle pipe_handle =
      pass_pipe_ownership_to_main ? pipe.release() : pipe.get();
  return std::move(handler).Run(pipe_handle.value());
}

}  // namespace

MultiprocessTestHelper::MultiprocessTestHelper() = default;

MultiprocessTestHelper::~MultiprocessTestHelper() {
  CHECK(!test_child_.IsValid());
}

ScopedMessagePipeHandle MultiprocessTestHelper::StartChild(
    const std::string& test_child_name,
    LaunchType launch_type) {
  return StartChildWithExtraSwitch(test_child_name, std::string(),
                                   std::string(), launch_type);
}

ScopedMessagePipeHandle MultiprocessTestHelper::StartChildWithExtraSwitch(
    const std::string& test_child_name,
    const std::string& switch_string,
    const std::string& switch_value,
    LaunchType launch_type) {
  CHECK(!test_child_name.empty());
  CHECK(!test_child_.IsValid());

  std::string test_child_main = test_child_name + "TestChildMain";

  // Manually construct the new child's commandline to avoid copying unwanted
  // values.
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine().GetProgram());

  std::set<std::string> uninherited_args;
  uninherited_args.insert("mojo-platform-channel-handle");
  uninherited_args.insert(switches::kTestChildProcess);

  std::string enable_overrides;
  std::string disable_overrides;
  base::FeatureList::GetInstance()->GetCommandLineFeatureOverrides(
      &enable_overrides, &disable_overrides);
  command_line.AppendSwitchASCII(switches::kEnableFeatures, enable_overrides);
  command_line.AppendSwitchASCII(switches::kDisableFeatures, disable_overrides);

  // Copy commandline switches from the parent process, except for the
  // multiprocess client name and mojo message pipe handle; this allows test
  // clients to spawn other test clients.
  for (const auto& entry :
       base::CommandLine::ForCurrentProcess()->GetSwitches()) {
    if (!base::Contains(uninherited_args, entry.first)) {
      command_line.AppendSwitchNative(entry.first, entry.second);
    }
  }

#if !BUILDFLAG(IS_FUCHSIA)
  NamedPlatformChannel::ServerName server_name;
#endif
  PlatformChannel channel;
  base::LaunchOptions options;
  switch (launch_type) {
    case LaunchType::CHILD:
    case LaunchType::CHILD_WITHOUT_CAPABILITIES:
    case LaunchType::PEER:
    case LaunchType::ASYNC:
      channel.PrepareToPassRemoteEndpoint(&options, &command_line);
      break;
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case LaunchType::NAMED_CHILD:
    case LaunchType::NAMED_PEER: {
#if BUILDFLAG(IS_MAC)
      server_name = NamedPlatformChannel::ServerNameFromUTF8(
          "mojo.test." + base::NumberToString(base::RandUint64()));
#elif BUILDFLAG(IS_POSIX)
      base::FilePath temp_dir;
      CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
      server_name =
          temp_dir.AppendASCII(base::NumberToString(base::RandUint64()))
              .value();
#elif BUILDFLAG(IS_WIN)
      server_name = base::NumberToWString(base::RandUint64());
#else
#error "Platform not yet supported."
#endif
      command_line.AppendSwitchNative(kNamedPipeName, server_name);
      break;
    }
#endif  // !BUILDFLAG(IS_FUCHSIA)
  }

  if (!switch_string.empty()) {
    CHECK(!command_line.HasSwitch(switch_string));
    if (!switch_value.empty())
      command_line.AppendSwitchASCII(switch_string, switch_value);
    else
      command_line.AppendSwitch(switch_string);
  }

#if BUILDFLAG(IS_WIN)
  options.start_hidden = true;
#endif

  // NOTE: In the case of named pipes, it's important that the server handle be
  // created before the child process is launched; otherwise the server binding
  // the pipe path can race with child's connection to the pipe.
  PlatformChannelEndpoint local_channel_endpoint;
  PlatformChannelServerEndpoint server_endpoint;
  switch (launch_type) {
    case LaunchType::CHILD:
    case LaunchType::CHILD_WITHOUT_CAPABILITIES:
    case LaunchType::PEER:
    case LaunchType::ASYNC:
      local_channel_endpoint = channel.TakeLocalEndpoint();
      break;
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case LaunchType::NAMED_CHILD:
    case LaunchType::NAMED_PEER: {
      NamedPlatformChannel::Options channel_options;
      channel_options.server_name = server_name;
      NamedPlatformChannel named_channel(channel_options);
      server_endpoint = named_channel.TakeServerEndpoint();
      break;
    }
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
  };

  OutgoingInvitation child_invitation;
  ScopedMessagePipeHandle pipe;
  switch (launch_type) {
    case LaunchType::CHILD_WITHOUT_CAPABILITIES:
    case LaunchType::ASYNC:
      // It's one or the other
      command_line.AppendSwitch(launch_type == LaunchType::ASYNC
                                    ? kAcceptInvitationAsync
                                    : kDisableAllCapabilities);
      [[fallthrough]];
    case LaunchType::CHILD:
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case LaunchType::NAMED_CHILD:
#endif
      pipe = child_invitation.AttachMessagePipe(kTestChildMessagePipeName);
      command_line.AppendSwitch(kRunAsBrokerClient);
      break;
    case LaunchType::PEER:
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case LaunchType::NAMED_PEER:
#endif
      isolated_connection_ = std::make_unique<IsolatedConnection>();
      if (local_channel_endpoint.is_valid()) {
        pipe = isolated_connection_->Connect(std::move(local_channel_endpoint));
      } else {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
        DCHECK(server_endpoint.is_valid());
        pipe = isolated_connection_->Connect(std::move(server_endpoint));
#else
        NOTREACHED();
#endif
      }
      break;
  }

  test_child_ =
      base::SpawnMultiProcessTestChild(test_child_main, command_line, options);

  if (launch_type == LaunchType::CHILD ||
      launch_type == LaunchType::CHILD_WITHOUT_CAPABILITIES ||
      launch_type == LaunchType::PEER || launch_type == LaunchType::ASYNC) {
    channel.RemoteProcessLaunchAttempted();
  }

  if (launch_type == LaunchType::CHILD ||
      launch_type == LaunchType::CHILD_WITHOUT_CAPABILITIES) {
    DCHECK(local_channel_endpoint.is_valid());
    OutgoingInvitation::Send(std::move(child_invitation), test_child_.Handle(),
                             std::move(local_channel_endpoint),
                             ProcessErrorCallback());
  } else if (launch_type == LaunchType::ASYNC) {
    DCHECK(local_channel_endpoint.is_valid());
    OutgoingInvitation::SendAsync(
        std::move(child_invitation), test_child_.Handle(),
        std::move(local_channel_endpoint), ProcessErrorCallback());
  }
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
  else if (launch_type == LaunchType::NAMED_CHILD) {
    DCHECK(server_endpoint.is_valid());
    OutgoingInvitation::Send(std::move(child_invitation), test_child_.Handle(),
                             std::move(server_endpoint),
                             ProcessErrorCallback());
  }
#endif  //  !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)

  CHECK(test_child_.IsValid());
  return pipe;
}

int MultiprocessTestHelper::WaitForChildShutdown() {
  CHECK(test_child_.IsValid());

  int rv = -1;
  WaitForMultiprocessTestChildExit(test_child_, TestTimeouts::action_timeout(),
                                   &rv);
  test_child_.Close();
  return rv;
}

bool MultiprocessTestHelper::WaitForChildTestShutdown() {
  return WaitForChildShutdown() == 0;
}

// static
void MultiprocessTestHelper::ChildSetup() {
  CHECK(base::CommandLine::InitializedForCurrentProcess());

  auto& command_line = *base::CommandLine::ForCurrentProcess();

  const bool run_as_broker_client = command_line.HasSwitch(kRunAsBrokerClient);
  const bool async = command_line.HasSwitch(kAcceptInvitationAsync);

  PlatformChannelEndpoint endpoint;
#if !BUILDFLAG(IS_FUCHSIA)
  NamedPlatformChannel::ServerName named_pipe(
      command_line.GetSwitchValueNative(kNamedPipeName));
  if (!named_pipe.empty()) {
    endpoint = NamedPlatformChannel::ConnectToServer(named_pipe);
  } else
#endif  // !BUILDFLAG(IS_FUCHSIA)
  {
    endpoint =
        PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line);
  }

  if (run_as_broker_client) {
    IncomingInvitation invitation;
    if (async)
      invitation = IncomingInvitation::AcceptAsync(std::move(endpoint));
    else
      invitation = IncomingInvitation::Accept(std::move(endpoint));
    primordial_pipe = invitation.ExtractMessagePipe(kTestChildMessagePipeName);
  } else {
    primordial_pipe =
        g_child_isolated_connection.Get().Connect(std::move(endpoint));
  }
}

// static
int MultiprocessTestHelper::RunClientMain(
    base::OnceCallback<int(MojoHandle)> main,
    bool pass_pipe_ownership_to_main) {
  return RunClientFunction(std::move(main), pass_pipe_ownership_to_main);
}

// static
int MultiprocessTestHelper::RunClientTestMain(
    base::OnceCallback<void(MojoHandle)> main) {
  return RunClientFunction(
      base::BindOnce(
          [](base::OnceCallback<void(MojoHandle)> main, MojoHandle handle) {
            std::move(main).Run(handle);
            return (::testing::Test::HasFatalFailure() ||
                    ::testing::Test::HasNonfatalFailure())
                       ? 1
                       : 0;
          },
          std::move(main)),
      true /* pass_pipe_ownership_to_main */);
}

ScopedMessagePipeHandle MultiprocessTestHelper::primordial_pipe;

}  // namespace test
}  // namespace core
}  // namespace mojo
