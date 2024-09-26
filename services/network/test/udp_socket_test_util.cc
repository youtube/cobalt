// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/udp_socket_test_util.h"

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network::test {

UDPSocketTestHelper::UDPSocketTestHelper(mojo::Remote<mojom::UDPSocket>* socket)
    : socket_(std::make_unique<mojom::UDPSocketAsyncWaiter>(socket->get())) {}

UDPSocketTestHelper::~UDPSocketTestHelper() = default;

int UDPSocketTestHelper::ConnectSync(const net::IPEndPoint& remote_addr,
                                     mojom::UDPSocketOptionsPtr options,
                                     net::IPEndPoint* local_addr_out) {
  int32_t net_error;
  absl::optional<net::IPEndPoint> local_addr_result;
  socket_->Connect(remote_addr, std::move(options), &net_error,
                   &local_addr_result);
  if (local_addr_result) {
    *local_addr_out = *local_addr_result;
  }
  return net_error;
}

int UDPSocketTestHelper::BindSync(const net::IPEndPoint& local_addr,
                                  mojom::UDPSocketOptionsPtr options,
                                  net::IPEndPoint* local_addr_out) {
  int32_t net_error;
  absl::optional<net::IPEndPoint> local_addr_result;
  socket_->Bind(local_addr, std::move(options), &net_error, &local_addr_result);
  if (local_addr_result) {
    *local_addr_out = *local_addr_result;
  }
  return net_error;
}

int UDPSocketTestHelper::SendToSync(const net::IPEndPoint& remote_addr,
                                    base::span<const uint8_t> data) {
  int32_t net_error;
  socket_->SendTo(
      remote_addr, data,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      &net_error);
  return net_error;
}

int UDPSocketTestHelper::SendSync(base::span<const uint8_t> data) {
  int32_t net_error;
  socket_->Send(
      data,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      &net_error);
  return net_error;
}

int UDPSocketTestHelper::SetBroadcastSync(bool broadcast) {
  int32_t net_error;
  socket_->SetBroadcast(broadcast, &net_error);
  return net_error;
}

int UDPSocketTestHelper::SetSendBufferSizeSync(int send_buffer_size) {
  int32_t net_error;
  socket_->SetSendBufferSize(send_buffer_size, &net_error);
  return net_error;
}

int UDPSocketTestHelper::SetReceiveBufferSizeSync(int receive_buffer_size) {
  int32_t net_error;
  socket_->SetReceiveBufferSize(receive_buffer_size, &net_error);
  return net_error;
}

int UDPSocketTestHelper::JoinGroupSync(const net::IPAddress& group_address) {
  int32_t net_error;
  socket_->JoinGroup(group_address, &net_error);
  return net_error;
}

int UDPSocketTestHelper::LeaveGroupSync(const net::IPAddress& group_address) {
  int32_t net_error;
  socket_->LeaveGroup(group_address, &net_error);
  return net_error;
}

UDPSocketListenerImpl::ReceivedResult::ReceivedResult(
    int net_error_arg,
    const absl::optional<net::IPEndPoint>& src_addr_arg,
    absl::optional<std::vector<uint8_t>> data_arg)
    : net_error(net_error_arg),
      src_addr(src_addr_arg),
      data(std::move(data_arg)) {}

UDPSocketListenerImpl::ReceivedResult::ReceivedResult(
    const ReceivedResult& other) = default;

UDPSocketListenerImpl::ReceivedResult::~ReceivedResult() {}

UDPSocketListenerImpl::UDPSocketListenerImpl()
    : run_loop_(std::make_unique<base::RunLoop>()),
      expected_receive_count_(0) {}

UDPSocketListenerImpl::~UDPSocketListenerImpl() {}

void UDPSocketListenerImpl::WaitForReceivedResults(size_t count) {
  DCHECK_LE(results_.size(), count);
  DCHECK_EQ(0u, expected_receive_count_);

  if (results_.size() == count)
    return;

  expected_receive_count_ = count;
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void UDPSocketListenerImpl::OnReceived(
    int32_t result,
    const absl::optional<net::IPEndPoint>& src_addr,
    absl::optional<base::span<const uint8_t>> data) {
  // OnReceive() API contracts specifies that this method will not be called
  // with a |result| that is > 0.
  DCHECK_GE(0, result);
  DCHECK(result < 0 || data);

  results_.emplace_back(result, src_addr,
                        data ? absl::make_optional(std::vector<uint8_t>(
                                   data.value().begin(), data.value().end()))
                             : absl::nullopt);
  if (results_.size() == expected_receive_count_) {
    expected_receive_count_ = 0;
    run_loop_->Quit();
  }
}

}  // namespace network::test
