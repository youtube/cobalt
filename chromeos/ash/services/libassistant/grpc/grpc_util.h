// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_UTIL_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_UTIL_H_

#include <string>

#include "base/functional/callback.h"
#include "third_party/grpc/src/include/grpc/grpc_security_constants.h"
#include "third_party/grpc/src/include/grpcpp/support/byte_buffer.h"
#include "third_party/grpc/src/include/grpcpp/support/status.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ash::libassistant {

template <class Status, class Response>
using ResponseCallback =
    base::OnceCallback<void(const Status&, const Response&)>;

// Returns the local connection type for the given server address.
grpc_local_connect_type GetGrpcLocalConnectType(
    const std::string& server_address);

// Serialize src and store in *dst.
grpc::Status GrpcSerializeProto(const google::protobuf::MessageLite& src,
                                grpc::ByteBuffer* dst);

// Parse contents of src and initialize *dst with them.
bool GrpcParseProto(grpc::ByteBuffer* src, google::protobuf::MessageLite* dst);

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_GRPC_UTIL_H_
