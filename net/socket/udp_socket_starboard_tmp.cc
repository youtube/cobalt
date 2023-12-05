// Copyright 2015 Google Inc. All Rights Reserved.
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

// Adapted from udp_socket_libevent.cc

#include "net/socket/udp_socket_starboard_tmp.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/current_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_source_type.h"
#include "net/socket/udp_net_log_parameters.h"
#include "starboard/common/socket.h"
#include "starboard/system.h"

namespace net {

UDPSocketStarboard::UDPSocketStarboard(DatagramSocket::BindType bind_type,
                                       net::NetLog* net_log,
                                       const net::NetLogSource& source)
    : write_async_watcher_(std::make_unique<WriteAsyncWatcher>(this)),
      sender_(new UDPSocketStarboardSender()),
      socket_(kSbSocketInvalid),
      socket_options_(0),
      bind_type_(bind_type),
      read_buf_len_(0),
      recv_from_address_(NULL),
      write_buf_len_(0),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::UDP_SOCKET)),
      weak_factory_(this) {
}

UDPSocketStarboard::~UDPSocketStarboard() {
}

int UDPSocketStarboard::Open(AddressFamily address_family) {
  return OK;
}

int UDPSocketStarboard::AdoptOpenedSocket(AddressFamily address_family,
                                      const SbSocket& socket) {
  return OK;
}

void UDPSocketStarboard::Close() {}

int UDPSocketStarboard::GetPeerAddress(IPEndPoint* address) const {
  return OK;
}

int UDPSocketStarboard::GetLocalAddress(IPEndPoint* address) const {
  return OK;
}

int UDPSocketStarboard::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) {
  return OK;
}

int UDPSocketStarboard::RecvFrom(IOBuffer* buf,
                                 int buf_len,
                                 IPEndPoint* address,
                                 CompletionOnceCallback callback) {
  return ERR_IO_PENDING;
}

int UDPSocketStarboard::Write(IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback,
                              const NetworkTrafficAnnotationTag&) {
  return OK;
}

int UDPSocketStarboard::SendTo(IOBuffer* buf,
                               int buf_len,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) {
  return OK;
}

int UDPSocketStarboard::SendToOrWrite(IOBuffer* buf,
                                      int buf_len,
                                      const IPEndPoint* address,
                                      CompletionOnceCallback callback) {
  return ERR_IO_PENDING;
}

int UDPSocketStarboard::Connect(const IPEndPoint& address) {
  return OK;
}

int UDPSocketStarboard::Bind(const IPEndPoint& address) {
  return OK;
}

int UDPSocketStarboard::BindToNetwork(
    handles::NetworkHandle network) {
  return ERR_NOT_IMPLEMENTED;
}

int UDPSocketStarboard::SetReceiveBufferSize(int32_t size) {
  return OK;
}

int UDPSocketStarboard::SetSendBufferSize(int32_t size) {
  return OK;
}

int UDPSocketStarboard::AllowAddressReuse() {
  return OK;
}

int UDPSocketStarboard::SetBroadcast(bool broadcast) {
  return OK;
}

int UDPSocketStarboard::AllowAddressSharingForMulticast() {
  return OK;
}

void UDPSocketStarboard::OnSocketReadyToRead(SbSocket /*socket*/) {
}

void UDPSocketStarboard::OnSocketReadyToWrite(SbSocket socket) {
}

void UDPSocketStarboard::WriteAsyncWatcher::OnSocketReadyToWrite(
    SbSocket /*socket*/) {
}

void UDPSocketStarboard::DoReadCallback(int rv) {
}

void UDPSocketStarboard::DoWriteCallback(int rv) {
}

void UDPSocketStarboard::DidCompleteRead() {
}

void UDPSocketStarboard::LogRead(int result,
                                 const char* bytes,
                                 const IPEndPoint* address) const {
}

void UDPSocketStarboard::DidCompleteWrite() {
}

void UDPSocketStarboard::LogWrite(int result,
                                  const char* bytes,
                                  const IPEndPoint* address) const {
}

int UDPSocketStarboard::InternalRecvFrom(IOBuffer* buf,
                                         int buf_len,
                                         IPEndPoint* address) {
  return OK;
}

int UDPSocketStarboard::InternalSendTo(IOBuffer* buf,
                                       int buf_len,
                                       const IPEndPoint* address) {
  return OK;
}

int UDPSocketStarboard::RandomBind(const IPAddress& address) {
  return OK;
}

int UDPSocketStarboard::JoinGroup(const IPAddress& group_address) const {
  return OK;
}

int UDPSocketStarboard::LeaveGroup(const IPAddress& group_address) const {
  return ERR_FAILED;
}

int UDPSocketStarboard::SetMulticastInterface(uint32_t interface_index) {
  return OK;
}

int UDPSocketStarboard::SetMulticastTimeToLive(int time_to_live) {
  return ERR_FAILED;
}

int UDPSocketStarboard::SetMulticastLoopbackMode(bool loopback) {
  return ERR_FAILED;
}

int UDPSocketStarboard::SetDiffServCodePoint(DiffServCodePoint dscp) {
  return OK;
}

void UDPSocketStarboard::DetachFromThread() {
}

void UDPSocketStarboard::ApplySocketTag(const SocketTag&) {
}

UDPSocketStarboardSender::UDPSocketStarboardSender() {}
UDPSocketStarboardSender::~UDPSocketStarboardSender() {}

SendResult::SendResult() : rv(0), write_count(0) {}
SendResult::~SendResult() {}
SendResult::SendResult(int _rv, int _write_count, DatagramBuffers _buffers)
    : rv(_rv), write_count(_write_count), buffers(std::move(_buffers)) {}
SendResult::SendResult(SendResult&& other) = default;

SendResult UDPSocketStarboardSender::InternalSendBuffers(
    const SbSocket& socket,
    DatagramBuffers buffers,
    SbSocketAddress address) const {
  return SendResult();
}

SendResult UDPSocketStarboardSender::SendBuffers(const SbSocket& socket,
                                                 DatagramBuffers buffers,
                                                 SbSocketAddress address) {
  return SendResult();
}

int UDPSocketStarboardSender::Send(const SbSocket& socket,
                                   const char* buf,
                                   size_t len,
                                   SbSocketAddress address) const {
  return OK;
}

int UDPSocketStarboard::WriteAsync(
    const char* buffer,
    size_t buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return OK;
}

int UDPSocketStarboard::WriteAsync(
    DatagramBuffers buffers,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return OK;
}

int UDPSocketStarboard::InternalWriteAsync(
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return OK;
}

void UDPSocketStarboard::FlushPending() {
}

void UDPSocketStarboard::OnWriteAsyncTimerFired() {
}

void UDPSocketStarboard::LocalSendBuffers() {
}

void UDPSocketStarboard::PostSendBuffers() {
}

void UDPSocketStarboard::DidSendBuffers(SendResult send_result) {
}

int UDPSocketStarboard::SetDoNotFragment() {
  return ERR_NOT_IMPLEMENTED;
}

void UDPSocketStarboard::SetMsgConfirm(bool confirm) {
}

bool UDPSocketStarboard::WatchSocket() {
  return true;
}

void UDPSocketStarboard::StopWatchingSocket() {
}

bool UDPSocketStarboard::InternalWatchSocket() {
  return true;
}

void UDPSocketStarboard::InternalStopWatchingSocket() {
}

void UDPSocketStarboard::SetMaxPacketSize(size_t max_packet_size) {
}

int UDPSocketStarboard::ResetLastAsyncResult() {
  return OK;
}

int UDPSocketStarboard::ResetWrittenBytes() {
  return OK;
}

}  // namespace net
