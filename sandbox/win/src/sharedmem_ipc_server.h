// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_SHAREDMEM_IPC_SERVER_H_
#define SANDBOX_WIN_SRC_SHAREDMEM_IPC_SERVER_H_

#include <stdint.h>

#include <list>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/win/scoped_handle.h"
#include "sandbox/win/src/crosscall_params.h"
#include "sandbox/win/src/crosscall_server.h"
#include "sandbox/win/src/sharedmem_ipc_client.h"
#include "sandbox/win/src/threadpool.h"

// IPC transport implementation that uses shared memory.
// This is the server side
//
// The server side has knowledge about the layout of the shared memory
// and the state transitions. Both are explained in sharedmem_ipc_client.h
//
// As opposed to SharedMemIPClient, the Server object should be one for the
// entire lifetime of the target process. The server is in charge of creating
// the events (ping, pong) both for the client and for the target that are used
// to signal the IPC and also in charge of setting the initial state of the
// channels.
//
// When an IPC is ready, the server relies on being called by on the
// ThreadPingEventReady callback. The IPC server then retrieves the buffer,
// marshals it into a CrossCallParam object and calls the Dispatcher, who is in
// charge of fulfilling the IPC request.
namespace sandbox {

// the shared memory implementation of the IPC server. There should be one
// of these objects per target (IPC client) process
class SharedMemIPCServer {
 public:
  // Creates the IPC server.
  // target_process: handle to the target process. It must be suspended. It is
  // unfortunate to receive a raw handle (and store it inside this object) as
  // that dilutes ownership of the process, but in practice a SharedMemIPCServer
  // is owned by TargetProcess, which calls this method, and owns the handle, so
  // everything is safe. If that changes, we should break this dependency and
  // duplicate the handle instead.
  // target_process_id: process id of the target process.
  // thread_pool: a thread pool object.
  // dispatcher: an object that can service IPC calls.
  SharedMemIPCServer(HANDLE target_process,
                     DWORD target_process_id,
                     ThreadPool* thread_pool,
                     Dispatcher* dispatcher);

  SharedMemIPCServer(const SharedMemIPCServer&) = delete;
  SharedMemIPCServer& operator=(const SharedMemIPCServer&) = delete;

  ~SharedMemIPCServer();

  // Initializes the server structures, shared memory structures and
  // creates the kernels events used to signal the IPC.
  bool Init(void* shared_mem, uint32_t shared_size, uint32_t channel_size);

 private:
  // Allow tests to be marked DISABLED_. Note that FLAKY_ and FAILS_ prefixes
  // do not work with sandbox tests.
  FRIEND_TEST_ALL_PREFIXES(IPCTest, SharedMemServerTests);
  // When an event fires (IPC request). A thread from the ThreadPool
  // will call this function. The context parameter should be the same as
  // provided when ThreadPool::RegisterWait was called.
  static void __stdcall ThreadPingEventReady(void* context, unsigned char);

  // Makes the client and server events. This function is called once
  // per channel.
  bool MakeEvents(base::win::ScopedHandle* server_ping,
                  base::win::ScopedHandle* server_pong,
                  HANDLE* client_ping,
                  HANDLE* client_pong);

  // A copy this structure is maintained per channel.
  // Note that a lot of the fields are just the same of what we have in the IPC
  // object itself. It is better to have the copies since we can dispatch in the
  // static method without worrying about converting back to a member function
  // call or about threading issues.
  struct ServerControl {
    ServerControl();
    ~ServerControl();

    // This channel server ping event.
    base::win::ScopedHandle ping_event;
    // This channel server pong event.
    base::win::ScopedHandle pong_event;
    // The size of this channel.
    uint32_t channel_size;
    // The pointer to the actual channel data.
    raw_ptr<char, AllowPtrArithmetic> channel_buffer;
    // The pointer to the base of the shared memory.
    raw_ptr<char, AllowPtrArithmetic> shared_base;
    // A pointer to this channel's client-side control structure this structure
    // lives in the shared memory.
    raw_ptr<ChannelControl> channel;
    // the IPC dispatcher associated with this channel.
    raw_ptr<Dispatcher> dispatcher;
    // The target process information associated with this channel.
    ClientInfo target_info;
  };

  // Looks for the appropriate handler for this IPC and invokes it.
  static bool InvokeCallback(const ServerControl* service_context,
                             void* ipc_buffer,
                             CrossCallReturn* call_result);

  // Points to the shared memory channel control which lives at
  // the start of the shared section.
  //
  // `client_control_` is not a raw_ptr<IPCControl>, because reinterpret_cast of
  // uninitialized memory to raw_ptr can cause ref-counting mismatch.
  RAW_PTR_EXCLUSION IPCControl* client_control_;

  // Keeps track of the server side objects that are used to answer an IPC.
  std::list<std::unique_ptr<ServerControl>> server_contexts_;

  // The thread pool provides the threads that call back into this object
  // when the IPC events fire.
  raw_ptr<ThreadPool> thread_pool_;

  // The IPC object is associated with a target process.
  HANDLE target_process_;

  // The target process id associated with the IPC object.
  DWORD target_process_id_;

  // The dispatcher handles 'ready' IPC calls.
  //
  // `call_dispatcher_` is not a raw_ptr<Dispatcher>, because reinterpret_cast
  // of uninitialized memory to raw_ptr can cause ref-counting mismatch.
  RAW_PTR_EXCLUSION Dispatcher* call_dispatcher_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_SHAREDMEM_IPC_SERVER_H_
