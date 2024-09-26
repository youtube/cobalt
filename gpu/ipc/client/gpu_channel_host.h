// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_
#define GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <memory>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"
#include "gpu/ipc/client/image_decode_accelerator_proxy.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace IPC {
class ChannelMojo;
}

namespace gpu {
class ClientSharedImageInterface;
struct SyncToken;
class GpuChannelHost;
class GpuMemoryBufferManager;

using GpuChannelEstablishedCallback =
    base::OnceCallback<void(scoped_refptr<GpuChannelHost>)>;

class GPU_EXPORT GpuChannelEstablishFactory {
 public:
  virtual ~GpuChannelEstablishFactory() = default;

  virtual void EstablishGpuChannel(GpuChannelEstablishedCallback callback) = 0;
  virtual scoped_refptr<GpuChannelHost> EstablishGpuChannelSync() = 0;
  virtual GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
};

// Encapsulates an IPC channel between the client and one GPU process.
// On the GPU process side there's a corresponding GpuChannel.
// Every method can be called on any thread with a message loop, except for the
// IO thread.
class GPU_EXPORT GpuChannelHost
    : public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  GpuChannelHost(
      int channel_id,
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      mojo::ScopedMessagePipeHandle handle,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr);
  GpuChannelHost(const GpuChannelHost&) = delete;
  GpuChannelHost& operator=(const GpuChannelHost&) = delete;

  bool IsLost() const { return !connection_tracker_->is_connected(); }

  int channel_id() const { return channel_id_; }

  const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner() {
    return io_thread_;
  }

  // Virtual for testing.
  virtual mojom::GpuChannel& GetGpuChannel();

  // The GPU stats reported by the GPU process.
  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }
  const gpu::GpuFeatureInfo& gpu_feature_info() const {
    return gpu_feature_info_;
  }

  // Enqueue a deferred message for the ordering barrier and return an
  // identifier that can be used to ensure or verify the deferred message later.
  uint32_t OrderingBarrier(int32_t route_id,
                           int32_t put_offset,
                           std::vector<SyncToken> sync_token_fences);

  // Enqueues an IPC message that is deferred until the next implicit or
  // explicit flush. The IPC is also possibly gated on one or more SyncTokens
  // being released, but is handled in-order relative to other such IPCs and/or
  // OrderingBarriers. Returns a deferred message id just like OrderingBarrier.
  uint32_t EnqueueDeferredMessage(
      mojom::DeferredRequestParamsPtr params,
      std::vector<SyncToken> sync_token_fences = {});

  // Ensure that the all deferred messages prior upto |deferred_message_id| have
  // been flushed. Pass UINT32_MAX to force all pending deferred messages to be
  // flushed.
  virtual void EnsureFlush(uint32_t deferred_message_id);

  // Verify that the all deferred messages prior upto |deferred_message_id| have
  // reached the service. Pass UINT32_MAX to force all pending deferred messages
  // to be verified.
  void VerifyFlush(uint32_t deferred_message_id);

  // Destroy this channel. Must be called on the main thread, before
  // destruction.
  void DestroyChannel();

  // Reserve one unused image ID.
  int32_t ReserveImageId();

  // Generate a route ID guaranteed to be unique for this channel.
  int32_t GenerateRouteID();

  // Crashes the GPU process. This functionality is added here because
  // of instability when creating a new tab just to navigate to
  // chrome://gpucrash . This only works when running tests and is
  // otherwise ignored.
  void CrashGpuProcessForTesting();

  // Terminates the GPU process with an exit code of 0. This only works when
  // running tests and is otherwise ignored.
  void TerminateGpuProcessForTesting();

  // Virtual for testing.
  virtual std::unique_ptr<ClientSharedImageInterface>
  CreateClientSharedImageInterface();

  ImageDecodeAcceleratorProxy* image_decode_accelerator_proxy() {
    return &image_decode_accelerator_proxy_;
  }

 protected:
  friend class base::RefCountedThreadSafe<GpuChannelHost>;
  virtual ~GpuChannelHost();

 private:
  // Tracks whether we still have a working connection to the GPU process. This
  // is updated eaglerly from the IO thread if the connection is broken, but it
  // may be queried from any thread via GpuChannel::IsLost(). This is why it's a
  // RefCountedThreadSafe object.
  struct ConnectionTracker
      : public base::RefCountedThreadSafe<ConnectionTracker> {
    ConnectionTracker();

    bool is_connected() const { return is_connected_.load(); }

    void OnDisconnectedFromGpuProcess();

   private:
    friend class base::RefCountedThreadSafe<ConnectionTracker>;
    ~ConnectionTracker();

    std::atomic_bool is_connected_{true};
  };

  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop. It also maintains some shared state between
  // all the contexts.
  class GPU_EXPORT Listener : public IPC::Listener {
   public:
    Listener();
    ~Listener() override;

    // Called on the GpuChannelHost's thread.
    void Initialize(mojo::ScopedMessagePipeHandle handle,
                    mojo::PendingAssociatedReceiver<mojom::GpuChannel> receiver,
                    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

    // Called on the IO thread.
    void Close();

    // IPC::Listener implementation
    // (called on the IO thread):
    bool OnMessageReceived(const IPC::Message& msg) override;
    void OnChannelError() override;

   private:
    mutable base::Lock lock_;
    std::unique_ptr<IPC::ChannelMojo> channel_ GUARDED_BY(lock_);
  };

  struct OrderingBarrierInfo {
    OrderingBarrierInfo();
    ~OrderingBarrierInfo();
    OrderingBarrierInfo(OrderingBarrierInfo&&);
    OrderingBarrierInfo& operator=(OrderingBarrierInfo&&);

    // Route ID of the command buffer for this command buffer flush.
    int32_t route_id;
    // Client put offset. Service get offset is updated in shared memory.
    int32_t put_offset;
    // Increasing counter for the deferred message.
    uint32_t deferred_message_id;
    // Sync token dependencies of the message. These are sync tokens for which
    // waits are in the commands that are part of this command buffer flush.
    std::vector<SyncToken> sync_token_fences;
  };

  void EnqueuePendingOrderingBarrier();
  void InternalFlush(uint32_t deferred_message_id);

  // Threading notes: all fields are constant during the lifetime of |this|
  // except:
  // - |next_image_id_|, atomic type
  // - |next_route_id_|, atomic type
  // - |deferred_messages_| and |*_deferred_message_id_| protected by
  // |context_lock_|
  const scoped_refptr<base::SingleThreadTaskRunner> io_thread_;

  const int channel_id_;
  const gpu::GPUInfo gpu_info_;
  const gpu::GpuFeatureInfo gpu_feature_info_;

  // Lifetime/threading notes: Listener only operates on the IO thread, and
  // outlives |this|. It is therefore safe to PostTask calls to the IO thread
  // with base::Unretained(listener_).
  std::unique_ptr<Listener, base::OnTaskRunnerDeleter> listener_;

  // Atomically tracks whether the GPU connection has been lost. This can be
  // queried from any thread by IsLost() but is always set on the IO thread as
  // soon as disconnection is detected.
  const scoped_refptr<ConnectionTracker> connection_tracker_;

  mojo::SharedAssociatedRemote<mojom::GpuChannel> gpu_channel_;
  SharedImageInterfaceProxy shared_image_interface_;

  // A client-side helper to send image decode requests to the GPU process.
  ImageDecodeAcceleratorProxy image_decode_accelerator_proxy_;

  // Image IDs are allocated in sequence.
  base::AtomicSequenceNumber next_image_id_;

  // Route IDs are allocated in sequence.
  base::AtomicSequenceNumber next_route_id_;

  // Protects |deferred_messages_|, |pending_ordering_barrier_| and
  // |*_deferred_message_id_|.
  mutable base::Lock context_lock_;
  std::vector<mojom::DeferredRequestPtr> deferred_messages_
      GUARDED_BY(context_lock_);
  absl::optional<OrderingBarrierInfo> pending_ordering_barrier_
      GUARDED_BY(context_lock_);
  uint32_t next_deferred_message_id_ GUARDED_BY(context_lock_) = 1;
  // Highest deferred message id in |deferred_messages_|.
  uint32_t enqueued_deferred_message_id_ GUARDED_BY(context_lock_) = 0;
  // Highest deferred message id sent to the channel.
  uint32_t flushed_deferred_message_id_ GUARDED_BY(context_lock_) = 0;
  // Highest deferred message id known to have been received by the service.
  uint32_t verified_deferred_message_id_ GUARDED_BY(context_lock_) = 0;
};

}  // namespace gpu

#endif  // GPU_IPC_CLIENT_GPU_CHANNEL_HOST_H_
