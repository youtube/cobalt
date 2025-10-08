/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <algorithm>

<<<<<<< HEAD
#include "perfetto/base/status.h"
#include "perfetto/ext/base/android_utils.h"
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/getopt.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/unix_socket.h"
=======
#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/getopt.h"
#include "perfetto/ext/base/string_utils.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/base/version.h"
#include "perfetto/ext/base/watchdog.h"
#include "perfetto/ext/traced/traced.h"
<<<<<<< HEAD
#include "perfetto/ext/tracing/core/tracing_service.h"
#include "perfetto/ext/tracing/ipc/service_ipc_host.h"
#include "perfetto/tracing/default_socket.h"
#include "src/traced/service/builtin_producer.h"

=======
#include "perfetto/ext/tracing/ipc/default_socket.h"
#include "perfetto/ext/tracing/ipc/service_ipc_host.h"
#include "src/traced/service/builtin_producer.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_LINUX) || \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#define PERFETTO_SET_SOCKET_PERMISSIONS
#include <fcntl.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
#include <sys/system_properties.h>
#endif

<<<<<<< HEAD
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
#include "src/tracing/service/zlib_compressor.h"
#endif

namespace perfetto {
namespace {
=======
namespace perfetto {
namespace {
#if defined(PERFETTO_SET_SOCKET_PERMISSIONS)
void SetSocketPermissions(const std::string& socket_name,
                          const std::string& group_name,
                          const std::string& mode_bits) {
  PERFETTO_CHECK(!socket_name.empty());
  PERFETTO_CHECK(!group_name.empty());
  struct group* socket_group = nullptr;
  // Query the group ID of |group|.
  do {
    socket_group = getgrnam(group_name.c_str());
  } while (socket_group == nullptr && errno == EINTR);
  if (socket_group == nullptr) {
    PERFETTO_FATAL("Failed to get group information of %s ",
                   group_name.c_str());
  }

  if (PERFETTO_EINTR(
          chown(socket_name.c_str(), geteuid(), socket_group->gr_gid))) {
    PERFETTO_FATAL("Failed to chown %s ", socket_name.c_str());
  }

  // |mode| accepts values like "0660" as "rw-rw----" mode bits.
  auto mode_value = base::StringToInt32(mode_bits, 8);
  if (!(mode_bits.size() == 4 && mode_value.has_value())) {
    PERFETTO_FATAL(
        "The chmod option must be a 4-digit octal number, e.g. 0660");
  }
  if (PERFETTO_EINTR(chmod(socket_name.c_str(),
                           static_cast<mode_t>(mode_value.value())))) {
    PERFETTO_FATAL("Failed to chmod %s", socket_name.c_str());
  }
}
#endif  // defined(PERFETTO_SET_SOCKET_PERMISSIONS)

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
void PrintUsage(const char* prog_name) {
  fprintf(stderr, R"(
Usage: %s [option] ...
Options and arguments
    --background : Exits immediately and continues running in the background
    --version : print the version number and exit.
    --set-socket-permissions <permissions> : sets group ownership and permission
        mode bits of the producer and consumer sockets.
        <permissions> format: <prod_group>:<prod_mode>:<cons_group>:<cons_mode>,
        where <prod_group> is the group name for chgrp the producer socket,
        <prod_mode> is the mode bits (e.g. 0660) for chmod the produce socket,
        <cons_group> is the group name for chgrp the consumer socket, and
        <cons_mode> is the mode bits (e.g. 0660) for chmod the consumer socket.
<<<<<<< HEAD
    --enable-relay-endpoint : enables the relay endpoint on producer socket(s)
        for traced_relay to communicate with traced in a multiple-machine
        tracing session.
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

Example:
    %s --set-socket-permissions traced-producer:0660:traced-consumer:0660
    starts the service and sets the group ownership of the producer and consumer
    sockets to "traced-producer" and "traced-consumer", respectively. Both
    producer and consumer sockets are chmod with 0660 (rw-rw----) mode bits.
)",
          prog_name, prog_name);
}
}  // namespace

int PERFETTO_EXPORT_ENTRYPOINT ServiceMain(int argc, char** argv) {
  enum LongOption {
    OPT_VERSION = 1000,
    OPT_SET_SOCKET_PERMISSIONS = 1001,
    OPT_BACKGROUND,
<<<<<<< HEAD
    OPT_ENABLE_RELAY_ENDPOINT
  };

  bool background = false;
  bool enable_relay_endpoint = false;
=======
  };

  bool background = false;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  static const option long_options[] = {
      {"background", no_argument, nullptr, OPT_BACKGROUND},
      {"version", no_argument, nullptr, OPT_VERSION},
      {"set-socket-permissions", required_argument, nullptr,
       OPT_SET_SOCKET_PERMISSIONS},
<<<<<<< HEAD
      {"enable-relay-endpoint", no_argument, nullptr,
       OPT_ENABLE_RELAY_ENDPOINT},
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      {nullptr, 0, nullptr, 0}};

  std::string producer_socket_group, consumer_socket_group,
      producer_socket_mode, consumer_socket_mode;

  for (;;) {
    int option = getopt_long(argc, argv, "", long_options, nullptr);
    if (option == -1)
      break;
    switch (option) {
      case OPT_BACKGROUND:
        background = true;
        break;
      case OPT_VERSION:
        printf("%s\n", base::GetVersionString());
        return 0;
      case OPT_SET_SOCKET_PERMISSIONS: {
        // Check that the socket permission argument is well formed.
        auto parts = base::SplitString(std::string(optarg), ":");
        PERFETTO_CHECK(parts.size() == 4);
        PERFETTO_CHECK(
            std::all_of(parts.cbegin(), parts.cend(),
                        [](const std::string& part) { return !part.empty(); }));
        producer_socket_group = parts[0];
        producer_socket_mode = parts[1];
        consumer_socket_group = parts[2];
        consumer_socket_mode = parts[3];
        break;
      }
<<<<<<< HEAD
      case OPT_ENABLE_RELAY_ENDPOINT:
        enable_relay_endpoint = true;
        break;
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      default:
        PrintUsage(argv[0]);
        return 1;
    }
  }

  if (background) {
    base::Daemonize([] { return 0; });
  }

  base::UnixTaskRunner task_runner;
  std::unique_ptr<ServiceIPCHost> svc;
<<<<<<< HEAD
  TracingService::InitOpts init_opts = {};
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
  init_opts.compressor_fn = &ZlibCompressFn;
#endif
  std::string relay_producer_socket;
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  relay_producer_socket = base::GetAndroidProp("traced.relay_producer_port");
  // If a guest producer port is defined, then the relay endpoint should be
  // enabled regardless. This is used in cases where perf data is passed
  // from guest machines or the hypervisor to Android.
  if (!relay_producer_socket.empty())
    init_opts.enable_relay_endpoint = true;
#endif
  if (enable_relay_endpoint)
    init_opts.enable_relay_endpoint = true;
  svc = ServiceIPCHost::CreateInstance(&task_runner, init_opts);
=======
  svc = ServiceIPCHost::CreateInstance(&task_runner);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // When built as part of the Android tree, the two socket are created and
  // bound by init and their fd number is passed in two env variables.
  // See libcutils' android_get_control_socket().
  const char* env_prod = getenv("ANDROID_SOCKET_traced_producer");
  const char* env_cons = getenv("ANDROID_SOCKET_traced_consumer");
  PERFETTO_CHECK((!env_prod && !env_cons) || (env_prod && env_cons));
  bool started;
  if (env_prod) {
#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
    PERFETTO_CHECK(false);
#else
<<<<<<< HEAD
    ListenEndpoint consumer_ep(base::ScopedFile(atoi(env_cons)));
    std::list<ListenEndpoint> producer_eps;
    producer_eps.emplace_back(ListenEndpoint(base::ScopedFile(atoi(env_prod))));
    if (!relay_producer_socket.empty()) {
      producer_eps.emplace_back(ListenEndpoint(relay_producer_socket));
    }
    started = svc->Start(std::move(producer_eps), std::move(consumer_ep));
#endif
  } else {
    std::list<ListenEndpoint> producer_eps;
    auto producer_socket_names = TokenizeProducerSockets(GetProducerSocket());
    for (const auto& producer_socket_name : producer_socket_names) {
      remove(producer_socket_name.c_str());
      producer_eps.emplace_back(ListenEndpoint(producer_socket_name));
    }
    remove(GetConsumerSocket());
    started = svc->Start(std::move(producer_eps),
                         ListenEndpoint(GetConsumerSocket()));

    if (!producer_socket_group.empty()) {
      auto status = base::OkStatus();
      for (const auto& producer_socket : producer_socket_names) {
        if (base::GetSockFamily(producer_socket.c_str()) !=
            base::SockFamily::kUnix) {
          // Socket permissions is only available to unix sockets.
          continue;
        }
        status = base::SetFilePermissions(
            producer_socket, producer_socket_group, producer_socket_mode);
        if (!status.ok()) {
          PERFETTO_ELOG("%s", status.c_message());
          return 1;
        }
      }
      status = base::SetFilePermissions(
          GetConsumerSocket(), consumer_socket_group, consumer_socket_mode);
      if (!status.ok()) {
        PERFETTO_ELOG("%s", status.c_message());
        return 1;
      }
=======
    base::ScopedFile producer_fd(atoi(env_prod));
    base::ScopedFile consumer_fd(atoi(env_cons));
    started = svc->Start(std::move(producer_fd), std::move(consumer_fd));
#endif
  } else {
    remove(GetProducerSocket());
    remove(GetConsumerSocket());
    started = svc->Start(GetProducerSocket(), GetConsumerSocket());

    if (!producer_socket_group.empty()) {
#if defined(PERFETTO_SET_SOCKET_PERMISSIONS)
      SetSocketPermissions(GetProducerSocket(), producer_socket_group,
                           producer_socket_mode);
      SetSocketPermissions(GetConsumerSocket(), consumer_socket_group,
                           consumer_socket_mode);
#else
      PERFETTO_ELOG(
          "Setting socket permissions is not supported on this platform");
      return 1;
#endif
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    }
  }

  if (!started) {
    PERFETTO_ELOG("Failed to start the traced service");
    return 1;
  }

  // Advertise builtin producers only on in-tree builds. These producers serve
  // only to dynamically start heapprofd and other services via sysprops, but
  // that can only ever happen in in-tree builds.
#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
  BuiltinProducer builtin_producer(&task_runner, /*lazy_stop_delay_ms=*/30000);
  builtin_producer.ConnectInProcess(svc->service());
#endif

  // Set the CPU limit and start the watchdog running. The memory limit will
  // be set inside the service code as it relies on the size of buffers.
  // The CPU limit is the generic one defined in watchdog.h.
  base::Watchdog* watchdog = base::Watchdog::GetInstance();
  watchdog->SetCpuLimit(base::kWatchdogDefaultCpuLimit,
                        base::kWatchdogDefaultCpuWindow);
  watchdog->Start();

  // If the TRACED_NOTIFY_FD env var is set, write 1 and close the FD. This is
  // so tools can synchronize with the point where the IPC socket has been
  // opened, without having to poll. This is used for //src/tracebox.
  const char* env_notif = getenv("TRACED_NOTIFY_FD");
  if (env_notif) {
    int notif_fd = atoi(env_notif);
    PERFETTO_CHECK(base::WriteAll(notif_fd, "1", 1) == 1);
    PERFETTO_CHECK(base::CloseFile(notif_fd) == 0);
  }

#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD) && \
    PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
  // Notify init (perfetto.rc) that traced has been started. Used only by
  // the perfetto_trace_on_boot init service.
  // This property can be set only in in-tree builds. shell.te doesn't have
  // SELinux permissions to set sys.trace.* properties.
  if (__system_property_set("sys.trace.traced_started", "1") != 0) {
    PERFETTO_PLOG("Failed to set property sys.trace.traced_started");
  }
#endif

  PERFETTO_ILOG("Started traced, listening on %s %s", GetProducerSocket(),
                GetConsumerSocket());
  task_runner.Run();
  return 0;
}

}  // namespace perfetto
