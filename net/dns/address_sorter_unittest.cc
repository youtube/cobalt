// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/address_sorter.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#endif

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "net/base/winsock_init.h"
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/notimplemented.h"
#include "net/dns/address_sorter_posix.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/transport_client_socket.h"
#endif

namespace net {
namespace {

IPEndPoint MakeEndPoint(const std::string& str) {
  IPAddress addr;
  CHECK(addr.AssignFromIPLiteral(str));
  return IPEndPoint(addr, 0);
}

void OnSortComplete(std::vector<IPEndPoint>* sorted_buf,
                    CompletionOnceCallback callback,
                    bool success,
                    std::vector<IPEndPoint> sorted) {
  if (success)
    *sorted_buf = std::move(sorted);
  std::move(callback).Run(success ? OK : ERR_FAILED);
}

TEST(AddressSorterTest, Sort) {
  base::test::TaskEnvironment task_environment;
  int expected_result = OK;
#if BUILDFLAG(IS_WIN)
  EnsureWinsockInit();
  SOCKET sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    expected_result = ERR_FAILED;
  } else {
    closesocket(sock);
  }
#endif
  std::unique_ptr<AddressSorter> sorter(AddressSorter::CreateAddressSorter());
  std::vector<IPEndPoint> endpoints;
  endpoints.push_back(MakeEndPoint("10.0.0.1"));
  endpoints.push_back(MakeEndPoint("8.8.8.8"));
  endpoints.push_back(MakeEndPoint("::1"));
  endpoints.push_back(MakeEndPoint("2001:4860:4860::8888"));

  std::vector<IPEndPoint> result;
  TestCompletionCallback callback;
  sorter->Sort(endpoints, NetworkAnonymizationKey(),
               handles::kInvalidNetworkHandle,
               base::BindOnce(&OnSortComplete, &result, callback.callback()));
  EXPECT_EQ(expected_result, callback.WaitForResult());
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// A simple no-op DatagramClientSocket implementation used to verify the target
// network used by AddressSorter.
class TargetNetworkCheckingDatagramClientSocket : public DatagramClientSocket {
 public:
  explicit TargetNetworkCheckingDatagramClientSocket(
      handles::NetworkHandle network)
      : network_(network) {}
  ~TargetNetworkCheckingDatagramClientSocket() override = default;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return ERR_FAILED;
  }
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return ERR_FAILED;
  }
  base::expected<DatagramsMetadata, Error> ReadMultiple(
      IOBuffer* buf,
      size_t buf_len,
      size_t max_message_size,
      base::OnceCallback<void(base::expected<DatagramsMetadata, Error>)>
          callback) override {
    return base::unexpected(Error(ERR_FAILED));
  }
  int SetReceiveBufferSize(int32_t size) override { return OK; }
  int SetSendBufferSize(int32_t size) override { return OK; }

  // DatagramSocket implementation:
  void Close() override {}
  int GetPeerAddress(IPEndPoint* address) const override { return ERR_FAILED; }
  int GetLocalAddress(IPEndPoint* address) const override { return ERR_FAILED; }
  void UseNonBlockingIO() override {}
  int SetDoNotFragment() override { return OK; }
  int SetRecvTos() override { return OK; }
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override { return OK; }
  void SetMsgConfirm(bool confirm) override {}

  // DatagramClientSocket implementation:
  int Connect(const IPEndPoint& address) override { return OK; }
  int ConnectUsingNetwork(handles::NetworkHandle network,
                          const IPEndPoint& address) override {
    return ERR_NOT_IMPLEMENTED;
  }
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override {
    return ERR_NOT_IMPLEMENTED;
  }
  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override {
    return OK;  // Synchronous success.
  }
  int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) override {
    return ERR_NOT_IMPLEMENTED;
  }
  int ConnectUsingDefaultNetworkAsync(
      const IPEndPoint& address,
      CompletionOnceCallback callback) override {
    return ERR_NOT_IMPLEMENTED;
  }
  handles::NetworkHandle GetBoundNetwork() const override { return network_; }
  void ApplySocketTag(const SocketTag& tag) override {}
  int SetMulticastInterface(uint32_t interface_index) override {
    return ERR_NOT_IMPLEMENTED;
  }
  const NetLogWithSource& NetLog() const override { return net_log_; }
  DscpAndEcn GetLastTos() const override { return {DSCP_DEFAULT, ECN_DEFAULT}; }

 private:
  handles::NetworkHandle network_;
  NetLogWithSource net_log_;
};

// A simple no-op ClientSocketFactory implementation used to verify the target
// network used by AddressSorter.
class TargetNetworkCheckingSocketFactory : public ClientSocketFactory {
 public:
  explicit TargetNetworkCheckingSocketFactory(
      handles::NetworkHandle expected_network)
      : expected_network_(expected_network) {}
  ~TargetNetworkCheckingSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      handles::NetworkHandle network,
      NetLog* net_log,
      const NetLogSource& source) override {
    EXPECT_EQ(expected_network_, network);
    called_ = true;
    return std::make_unique<TargetNetworkCheckingDatagramClientSocket>(network);
  }

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      handles::NetworkHandle network,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> stream_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  bool called() const { return called_; }

 private:
  handles::NetworkHandle expected_network_;
  bool called_ = false;
};

TEST(AddressSorterPosixTargetNetworkTest, PassesTargetNetworkToSocketFactory) {
  base::test::TaskEnvironment task_environment;

  handles::NetworkHandle target_network = 12345;
  TargetNetworkCheckingSocketFactory socket_factory(target_network);

  AddressSorterPosix sorter(&socket_factory);

  std::vector<IPEndPoint> endpoints;
  endpoints.push_back(MakeEndPoint("10.0.0.1"));

  std::vector<IPEndPoint> result;
  TestCompletionCallback callback;
  sorter.Sort(endpoints, NetworkAnonymizationKey(), target_network,
              base::BindOnce(&OnSortComplete, &result, callback.callback()));

  EXPECT_TRUE(socket_factory.called());
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

}  // namespace
}  // namespace net
