// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_
#define COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "components/named_mojo_ipc_server/endpoint_options.h"
#include "components/named_mojo_ipc_server/ipc_server.h"
#include "components/named_mojo_ipc_server/named_mojo_server_endpoint_connector.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
class IsolatedConnection;
}

namespace named_mojo_ipc_server {
// Template-less base class to keep implementations in the .cc file. For usage,
// see MojoIpcServer.
class NamedMojoIpcServerBase : public IpcServer {
 public:
  // Internal use only.
  struct PendingConnection;

  void StartServer() override;
  void StopServer() override;
  void Close(mojo::ReceiverId id) override;

  // Sets a callback to be run when an invitation is sent. Used by unit tests
  // only.
  void set_on_server_endpoint_created_callback_for_testing(
      const base::RepeatingClosure& callback) {
    on_server_endpoint_created_callback_for_testing_ = callback;
  }

 protected:
  NamedMojoIpcServerBase(
      const EndpointOptions& options,
      base::RepeatingCallback<void*(std::unique_ptr<ConnectionInfo>)>
          impl_provider);
  ~NamedMojoIpcServerBase() override;

  void OnIpcDisconnected();

  virtual mojo::ReceiverId TrackMessagePipe(
      mojo::ScopedMessagePipeHandle message_pipe,
      void* impl,
      base::ProcessId peer_pid) = 0;

  virtual void UntrackMessagePipe(mojo::ReceiverId id) = 0;

  virtual void UntrackAllMessagePipes() = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::RepeatingClosure disconnect_handler_;

 private:
  class DelegateProxy;

  void OnEndpointConnectorStarted(
      base::SequenceBound<NamedMojoServerEndpointConnector> endpoint_connector);
  void OnClientConnected(mojo::PlatformChannelEndpoint endpoint,
                         std::unique_ptr<ConnectionInfo> info);
  void OnServerEndpointCreated();

  using ActiveConnectionMap =
      base::flat_map<mojo::ReceiverId,
                     std::unique_ptr<mojo::IsolatedConnection>>;

  EndpointOptions options_;
  base::RepeatingCallback<void*(std::unique_ptr<ConnectionInfo>)>
      impl_provider_;

  bool server_started_ = false;

  // A task runner to run blocking jobs.
  scoped_refptr<base::SequencedTaskRunner> io_sequence_;

  base::SequenceBound<NamedMojoServerEndpointConnector> endpoint_connector_;

  // This is only populated if the server uses isolated connections.
  ActiveConnectionMap active_connections_;

  base::OneShotTimer restart_endpoint_timer_;

  base::RepeatingClosure on_server_endpoint_created_callback_for_testing_;

  base::WeakPtrFactory<NamedMojoIpcServerBase> weak_factory_{this};
};

// A helper that uses a NamedPlatformChannel to send out mojo invitations and
// maintains multiple concurrent IPCs. It keeps one outgoing invitation at a
// time and will send a new invitation whenever the previous one has been
// accepted by the client. Please see README.md for the example usage.
template <typename Interface>
class NamedMojoIpcServer final : public NamedMojoIpcServerBase {
 public:
  // options: Options to start the server endpoint.
  // impl_provider: A function that returns a pointer to an implementation,
  //     or nullptr if the connecting endpoint should be rejected.
  NamedMojoIpcServer(
      const EndpointOptions& options,
      base::RepeatingCallback<Interface*(std::unique_ptr<ConnectionInfo>)>
          impl_provider)
      : NamedMojoIpcServerBase(
            options,
            impl_provider.Then(base::BindRepeating([](Interface* impl) {
              // Opacify the type for the base class, which takes no template
              // parameters.
              return reinterpret_cast<void*>(impl);
            }))) {
    receiver_set_.set_disconnect_handler(base::BindRepeating(
        &NamedMojoIpcServer::OnIpcDisconnected, base::Unretained(this)));
  }

  ~NamedMojoIpcServer() override = default;

  NamedMojoIpcServer(const NamedMojoIpcServer&) = delete;
  NamedMojoIpcServer& operator=(const NamedMojoIpcServer&) = delete;

  void set_disconnect_handler(base::RepeatingClosure handler) override {
    disconnect_handler_ = handler;
  }

  mojo::ReceiverId current_receiver() const override {
    return receiver_set_.current_receiver();
  }

  base::ProcessId current_peer_pid() const override {
    return receiver_set_.current_context();
  }

  size_t GetNumberOfActiveConnectionsForTesting() const {
    return receiver_set_.size();
  }

 private:
  // NamedMojoIpcServerBase implementation.
  mojo::ReceiverId TrackMessagePipe(mojo::ScopedMessagePipeHandle message_pipe,
                                    void* impl,
                                    base::ProcessId peer_pid) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return receiver_set_.Add(
        reinterpret_cast<Interface*>(impl),
        mojo::PendingReceiver<Interface>(std::move(message_pipe)), peer_pid);
  }

  void UntrackMessagePipe(mojo::ReceiverId id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    receiver_set_.Remove(id);
  }

  void UntrackAllMessagePipes() override { receiver_set_.Clear(); }

  mojo::ReceiverSet<Interface, base::ProcessId> receiver_set_;
};

}  // namespace named_mojo_ipc_server

#endif  // COMPONENTS_NAMED_MOJO_IPC_SERVER_NAMED_MOJO_IPC_SERVER_H_
