// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NACL_LISTENER_H_
#define COMPONENTS_NACL_LOADER_NACL_LISTENER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/loader/nacl_ipc_adapter.h"
#include "components/nacl/loader/nacl_trusted_listener.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class SyncChannel;
class SyncMessageFilter;
}

// The NaClListener is an IPC channel listener that waits for a
// request to start a NaCl module.
class NaClListener : public IPC::Listener {
 public:
  NaClListener();

  NaClListener(const NaClListener&) = delete;
  NaClListener& operator=(const NaClListener&) = delete;

  ~NaClListener() override;
  // Listen for a request to launch a NaCl module.
  void Listen();

  bool Send(IPC::Message* msg);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void set_prereserved_sandbox_size(size_t prereserved_sandbox_size) {
    prereserved_sandbox_size_ = prereserved_sandbox_size;
  }
#endif
#if BUILDFLAG(IS_POSIX)
  void set_number_of_cores(int number_of_cores) {
    number_of_cores_ = number_of_cores;
  }
#endif

  void* crash_info_shmem_memory() const {
    return crash_info_shmem_mapping_.memory();
  }

  NaClTrustedListener* trusted_listener() const {
    return trusted_listener_.get();
  }

  void ResolveFileToken(uint64_t token_lo,
                        uint64_t token_hi,
                        NaClIPCAdapter::ResolveFileTokenReplyCallback cb);
  void OnFileTokenResolved(uint64_t token_lo,
                           uint64_t token_hi,
                           IPC::PlatformFileForTransit ipc_fd,
                           base::FilePath file_path);

 private:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  static int MakeSharedMemorySegment(size_t length, int executable);
#endif

  bool OnMessageReceived(const IPC::Message& msg) override;

  bool OnOpenResource(const IPC::Message& msg,
                      const std::string& key,
                      NaClIPCAdapter::OpenResourceReplyCallback cb);

  void OnAddPrefetchedResource(
      const nacl::NaClResourcePrefetchResult& prefetched_resource_file);
  void OnStart(nacl::NaClStartParams params);

  // A channel back to the browser.
  std::unique_ptr<IPC::SyncChannel> channel_;

  // A filter that allows other threads to use the channel.
  scoped_refptr<IPC::SyncMessageFilter> filter_;

  base::WaitableEvent shutdown_event_;
  base::Thread io_thread_;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  size_t prereserved_sandbox_size_;
#endif
#if BUILDFLAG(IS_POSIX)
  // The outer sandbox on Linux and OSX prevents
  // sysconf(_SC_NPROCESSORS) from working; in Windows, there are no
  // problems with invoking GetSystemInfo.  Therefore, only in
  // OS_POSIX do we need to supply the number of cores into the
  // NaClChromeMainArgs object.
  int number_of_cores_;
#endif

  base::WritableSharedMemoryMapping crash_info_shmem_mapping_;

  std::unique_ptr<NaClTrustedListener> trusted_listener_;

  NaClIPCAdapter::ResolveFileTokenReplyCallback resolved_cb_;

  // Used to identify what thread we're on.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  typedef std::map<
    std::string,  // manifest key
    std::pair<IPC::PlatformFileForTransit,
              base::FilePath> > PrefetchedResourceFilesMap;
  PrefetchedResourceFilesMap prefetched_resource_files_;

  bool is_started_;
};

#endif  // COMPONENTS_NACL_LOADER_NACL_LISTENER_H_
