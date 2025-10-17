// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_H_
#define CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_H_

#include <grpcpp/grpcpp.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromecast/cast_core/grpc/grpc_handler.h"
#include "chromecast/cast_core/grpc/server_reactor_tracker.h"

namespace cast {
namespace utils {

// Generic gRPC server that allows seamless registration of API handlers by
// specifying the API with GrpcHandler-based classes. The GrpcServer implements
// grpc::CallbackGenericService and is registered with the ServerBuilder as
// such. The implications are following:
//  - All gRPC requests arrive to server.
//  - The APIs are distiguished by the full RPC name (ie /<class>/method).
//  - The user must register the full rpc name (as a string) with specific
//  handler.
//
// An example usage that creates a server that combines below two services in
// one:
// PROTO:
//   service Foo {
//     rpc DoOneThing(OneThingRequest) returns (OneThingResponse);
//   }
//
//   service Bar {
//     rpc DoAnotherThing(AnotherThingRequest) returns (AnotherThingResponse);
//   }
//
// C++:
//   // Create a CastCoreService.Register handler.
//   constexpr const char DoOneThingMethod[] = "DoOneThing";
//   using DoOneThingHandler =
//       GrpcUnaryHandler<Foo, OneThingRequest,
//                        OneThingResponse, DoOneThingMethod>;
//
//   constexpr const char DoAnotherThingMethod[] = "DoAnotherThing";
//   using DoAnotherThingHandler =
//       GrpcUnaryHandler<Bar, AnotherThingRequest,
//                        AnotherThingResponse, DoAnotherThingMethod>;
//
//   GrpcServer server("[::]:12345");
//   server.SetHandler<DoOneThingHandler>(
//       base::BindOnce([](OneThingRequest request,
//                             DoOneThing::Reactor* reactor) {
//         reactor->Write(OneThingResponse());
//       }));
//   server.SetHandler<DoAnotherThingHandler>(
//       base::BindOnce([](AnotherThingRequest request,
//                             DoAnotherThing::Reactor* reactor) {
//         reactor->Write(AnotherThingResponse());
//       }));
//
//   // Start the server.
//   server.Start();
//
//   // Stop the server.
//   server.Stop();
//
class GrpcServer : public grpc::CallbackGenericService {
 public:
  // Constructor.
  GrpcServer();
  // Constructor with an endpoint.
  // `endpoint` must be in the format of [unix|unix-abstract:]<address>[:<port>]
  // where `address` is the unix domain socket name or a TCP/IP address. `port`
  // is used with TCP/IP addresses only and must always be specified. A value of
  // 0 is used to let gRPC framework pick an available port.
  explicit GrpcServer(std::string_view endpoint);
  GrpcServer(GrpcServer&& server);
  GrpcServer& operator=(GrpcServer&& server);
  ~GrpcServer() override;

  // Sets the request callback for an RPC defined by |Handler| type.
  // NOTE: Every handler must check that the GrpcServer associated with it is up
  // and running before accessing the |reactor| object.
  template <typename THandler>
  void SetHandler(typename THandler::OnRequestCallback on_request_callback) {
    // The full rpc name is /<fully-qualified-service-type>/method, ie
    // /cast.core.CastCoreService/RegisterRuntime.
    DCHECK(registered_handlers_.find(THandler::rpc_name()) ==
           registered_handlers_.end())
        << "Duplicate handler: " << THandler::rpc_name();
    registered_handlers_.emplace(
        THandler::rpc_name(),
        std::make_unique<THandler>(std::move(on_request_callback),
                                   server_reactor_tracker_.get()));
    DVLOG(1) << "Request handler is set for " << THandler::rpc_name();
  }

  // Starts the gRPC server.
  [[nodiscard]] grpc::Status Start();
  [[nodiscard]] grpc::Status Start(std::string_view endpoint);

  // Stops the gRPC server synchronously. May block indefinitely if there's a
  // non-finished pending reactor created by the gRPC framework.
  // NOTE: This framework guarantees thread safety iff Stop and handler
  // callbacks are called in the same sequence!
  void Stop();

  // Stops the gRPC server and calls the callback. The process will crash in
  // case the |timeout| is reached as such case clearly points to a bug in
  // reactor handling.
  // NOTE: This framework guarantees thread safety iff Stop and handler
  // callbacks are called in the same sequence!
  void Stop(int64_t timeout_ms, base::OnceClosure server_stopped_callback);

  size_t active_reactor_count() const {
    return server_reactor_tracker_->active_reactor_count();
  }

  const std::string& endpoint() const { return endpoint_; }

  bool is_running() const { return server_ != nullptr; }

 private:
  // Implements grpc::CallbackGenericService APIs.
  // Creates a reactor for a given rpc method from the |ctx|. If a handler is
  // registered for that method, it's reactor will be created. Otherwise a
  // default (unimplemented) reactor will be returned.
  grpc::ServerGenericBidiReactor* CreateReactor(
      grpc::GenericCallbackServerContext* ctx) override;

  std::string endpoint_;
  std::unique_ptr<ServerReactorTracker> server_reactor_tracker_;

  std::unordered_map<std::string, std::unique_ptr<utils::GrpcHandler>>
      registered_handlers_;
  std::unique_ptr<grpc::Server> server_;
};

}  // namespace utils
}  // namespace cast

#endif  // CHROMECAST_CAST_CORE_GRPC_GRPC_SERVER_H_
