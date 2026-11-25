// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/udp_socket.h"

#include <algorithm>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_clear_last_error.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/port_util.h"
#include "net/base/test_completion_callback.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/net_buildflags.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/udp_client_socket.h"
#include "net/socket/udp_server_socket.h"
#include "net/socket/udp_socket_global_limits.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !BUILDFLAG(IS_WIN)
#include <netinet/in.h>
#include <sys/socket.h>
#else
#include <winsock2.h>
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#endif

#if BUILDFLAG(IS_IOS)
#include <TargetConditionals.h>
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_MULTI_PACKETS_PER_CALL_QUIC_OPTIMIZATIONS)
#include <limits>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/metrics/field_trial.h"
#endif  // BUILDFLAG(ENABLE_MULTI_PACKETS_PER_CALL_QUIC_OPTIMIZATIONS)

using net::test::IsError;
using net::test::IsOk;
using testing::DoAll;
using testing::Not;

namespace net {

namespace {

// Creates an address from ip address and port and writes it to |*address|.
bool CreateUDPAddress(const std::string& ip_str,
                      uint16_t port,
                      IPEndPoint* address) {
  IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(ip_str))
    return false;

  *address = IPEndPoint(ip_address, port);
  return true;
}

class UDPSocketTest : public PlatformTest, public WithTaskEnvironment {
 public:
  UDPSocketTest() : buffer_(base::MakeRefCounted<IOBufferWithSize>(kMaxRead)) {}

  // Blocks until data is read from the socket.
  std::string RecvFromSocket(UDPServerSocket* socket) {
    return RecvFromSocket(socket, DSCP_DEFAULT, ECN_DEFAULT);
  }

  std::string RecvFromSocket(UDPServerSocket* socket,
                             DiffServCodePoint dscp,
                             EcnCodePoint ecn) {
    TestCompletionCallback callback;

    int rv = socket->RecvFrom(buffer_.get(), kMaxRead, &recv_from_address_,
                              callback.callback());
    rv = callback.GetResult(rv);
    if (rv < 0)
      return std::string();
#if BUILDFLAG(IS_WIN)
    // The DSCP value is not populated on Windows, in order to avoid incurring
    // an extra system call.
    EXPECT_EQ(socket->GetLastTos().dscp, DSCP_DEFAULT);
#else
    EXPECT_EQ(socket->GetLastTos().dscp, dscp);
#endif
    EXPECT_EQ(socket->GetLastTos().ecn, ecn);
    return std::string(buffer_->data(), rv);
  }

  // Sends UDP packet.
  // If |address| is specified, then it is used for the destination
  // to send to. Otherwise, will send to the last socket this server
  // received from.
  int SendToSocket(UDPServerSocket* socket, const std::string& msg) {
    return SendToSocket(socket, msg, recv_from_address_);
  }

  int SendToSocket(UDPServerSocket* socket,
                   std::string msg,
                   const IPEndPoint& address) {
    scoped_refptr<StringIOBuffer> io_buffer =
        base::MakeRefCounted<StringIOBuffer>(msg);
    TestCompletionCallback callback;
    int rv = socket->SendTo(io_buffer.get(), io_buffer->size(), address,
                            callback.callback());
    return callback.GetResult(rv);
  }

  std::string ReadSocket(UDPClientSocket* socket) {
    return ReadSocket(socket, DSCP_DEFAULT, ECN_DEFAULT);
  }

  std::string ReadSocket(UDPClientSocket* socket,
                         DiffServCodePoint dscp,
                         EcnCodePoint ecn) {
    TestCompletionCallback callback;

    int rv = socket->Read(buffer_.get(), kMaxRead, callback.callback());
    rv = callback.GetResult(rv);
    if (rv < 0)
      return std::string();
#if BUILDFLAG(IS_WIN)
    // The DSCP value is not populated on Windows, in order to avoid incurring
    // an extra system call.
    EXPECT_EQ(socket->GetLastTos().dscp, DSCP_DEFAULT);
#else
    EXPECT_EQ(socket->GetLastTos().dscp, dscp);
#endif
    EXPECT_EQ(socket->GetLastTos().ecn, ecn);
    return std::string(buffer_->data(), rv);
  }

  // Writes specified message to the socket.
  int WriteSocket(UDPClientSocket* socket, const std::string& msg) {
    scoped_refptr<StringIOBuffer> io_buffer =
        base::MakeRefCounted<StringIOBuffer>(msg);
    TestCompletionCallback callback;
    int rv = socket->Write(io_buffer.get(), io_buffer->size(),
                           callback.callback(), TRAFFIC_ANNOTATION_FOR_TESTS);
    return callback.GetResult(rv);
  }

  void WriteSocketIgnoreResult(UDPClientSocket* socket,
                               const std::string& msg) {
    WriteSocket(socket, msg);
  }

  // And again for a bare socket
  int SendToSocket(UDPSocket* socket,
                   std::string msg,
                   const IPEndPoint& address) {
    auto io_buffer = base::MakeRefCounted<StringIOBuffer>(msg);
    TestCompletionCallback callback;
    int rv = socket->SendTo(io_buffer.get(), io_buffer->size(), address,
                            callback.callback());
    return callback.GetResult(rv);
  }

  // Run unit test for a connection test.
  // |use_nonblocking_io| is used to switch between overlapped and non-blocking
  // IO on Windows. It has no effect in other ports.
  void ConnectTest(bool use_nonblocking_io, bool use_async);

 protected:
  static const int kMaxRead = 1024;
  scoped_refptr<IOBufferWithSize> buffer_;
  IPEndPoint recv_from_address_;
};

const int UDPSocketTest::kMaxRead;

void ReadCompleteCallback(int* result_out,
                          base::OnceClosure callback,
                          int result) {
  *result_out = result;
  std::move(callback).Run();
}

void UDPSocketTest::ConnectTest(bool use_nonblocking_io, bool use_async) {
  std::string simple_message("hello world!");
  RecordingNetLogObserver net_log_observer;
  // Setup the server to listen.
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0 /* port */);
  auto server =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  if (use_nonblocking_io)
    server->UseNonBlockingIO();
  server->AllowAddressReuse();
  ASSERT_THAT(server->Listen(server_address), IsOk());
  // Get bound port.
  ASSERT_THAT(server->GetLocalAddress(&server_address), IsOk());

  // Setup the client.
  auto client = std::make_unique<UDPClientSocket>(
      DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  if (use_nonblocking_io)
    client->UseNonBlockingIO();

  if (!use_async) {
    EXPECT_THAT(client->Connect(server_address), IsOk());
  } else {
    TestCompletionCallback callback;
    int rv = client->ConnectAsync(server_address, callback.callback());
    if (rv != OK) {
      ASSERT_EQ(rv, ERR_IO_PENDING);
      rv = callback.WaitForResult();
      EXPECT_EQ(rv, OK);
    } else {
      EXPECT_EQ(rv, OK);
    }
  }
  // Client sends to the server.
  EXPECT_EQ(simple_message.length(),
            static_cast<size_t>(WriteSocket(client.get(), simple_message)));

  // Server waits for message.
  std::string str = RecvFromSocket(server.get());
  EXPECT_EQ(simple_message, str);

  // Server echoes reply.
  EXPECT_EQ(simple_message.length(),
            static_cast<size_t>(SendToSocket(server.get(), simple_message)));

  // Client waits for response.
  str = ReadSocket(client.get());
  EXPECT_EQ(simple_message, str);

  // Test asynchronous read. Server waits for message.
  base::RunLoop run_loop;
  int read_result = 0;
  int rv = server->RecvFrom(buffer_.get(), kMaxRead, &recv_from_address_,
                            base::BindOnce(&ReadCompleteCallback, &read_result,
                                           run_loop.QuitClosure()));
  EXPECT_THAT(rv, IsError(ERR_IO_PENDING));

  // Client sends to the server.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&UDPSocketTest::WriteSocketIgnoreResult,
                     base::Unretained(this), client.get(), simple_message));
  run_loop.Run();
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(read_result));
  EXPECT_EQ(simple_message, std::string(buffer_->data(), read_result));

  NetLogSource server_net_log_source = server->NetLog().source();
  NetLogSource client_net_log_source = client->NetLog().source();

  // Delete sockets so they log their final events.
  server.reset();
  client.reset();

  // Check the server's log.
  auto server_entries =
      net_log_observer.GetEntriesForSource(server_net_log_source);
  ASSERT_EQ(6u, server_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(server_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(LogContainsEvent(server_entries, 1,
                               NetLogEventType::UDP_LOCAL_ADDRESS,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(server_entries, 2,
                               NetLogEventType::UDP_BYTES_RECEIVED,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(server_entries, 3,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(server_entries, 4,
                               NetLogEventType::UDP_BYTES_RECEIVED,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(
      LogContainsEndEvent(server_entries, 5, NetLogEventType::SOCKET_ALIVE));

  // Check the client's log.
  auto client_entries =
      net_log_observer.GetEntriesForSource(client_net_log_source);
  EXPECT_EQ(7u, client_entries.size());
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 0, NetLogEventType::SOCKET_ALIVE));
  EXPECT_TRUE(
      LogContainsBeginEvent(client_entries, 1, NetLogEventType::UDP_CONNECT));
  EXPECT_TRUE(
      LogContainsEndEvent(client_entries, 2, NetLogEventType::UDP_CONNECT));
  EXPECT_TRUE(LogContainsEvent(client_entries, 3,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 4,
                               NetLogEventType::UDP_BYTES_RECEIVED,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(LogContainsEvent(client_entries, 5,
                               NetLogEventType::UDP_BYTES_SENT,
                               NetLogEventPhase::NONE));
  EXPECT_TRUE(
      LogContainsEndEvent(client_entries, 6, NetLogEventType::SOCKET_ALIVE));
}

TEST_F(UDPSocketTest, Connect) {
  // The variable |use_nonblocking_io| has no effect in non-Windows ports.
  // Run ConnectTest once with sync connect and once with async connect
  ConnectTest(false, false);
  ConnectTest(false, true);
}

TEST_F(UDPSocketTest, ConnectRestrictedPort) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  // Setup the server to listen.
  UDPServerSocket server(NetLog::Get(), NetLogSource());
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(IPEndPoint(IPAddress::IPv4Localhost(), 0)), IsOk());
  // Get bound port.
  IPEndPoint server_address;
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());
  feature_list.InitAndEnableFeatureWithParameters(
      features::kRestrictAbusePortsOnLocalhost,
      {{"localhost_restrict_ports",
        base::NumberToString(server_address.port())}});
  ReloadLocalhostRestrictedPortsForTesting();
  // Setup the client.
  auto client = std::make_unique<UDPClientSocket>(
      DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  EXPECT_THAT(client->Connect(server_address), IsError(ERR_UNSAFE_PORT));
  histogram_tester.ExpectTotalCount("Net.RestrictedLocalhostPorts", 1);
  histogram_tester.ExpectBucketCount("Net.RestrictedLocalhostPorts",
                                     server_address.port(), 1);
}

#if BUILDFLAG(IS_WIN)
TEST_F(UDPSocketTest, ConnectNonBlocking) {
  ConnectTest(true, false);
  ConnectTest(true, true);
}
#endif

TEST_F(UDPSocketTest, PartialRecv) {
  UDPServerSocket server_socket(nullptr, NetLogSource());
  ASSERT_THAT(server_socket.Listen(IPEndPoint(IPAddress::IPv4Localhost(), 0)),
              IsOk());
  IPEndPoint server_address;
  ASSERT_THAT(server_socket.GetLocalAddress(&server_address), IsOk());

  UDPClientSocket client_socket(DatagramSocket::DEFAULT_BIND, nullptr,
                                NetLogSource());
  ASSERT_THAT(client_socket.Connect(server_address), IsOk());

  std::string test_packet("hello world!");
  ASSERT_EQ(static_cast<int>(test_packet.size()),
            WriteSocket(&client_socket, test_packet));

  TestCompletionCallback recv_callback;

  // Read just 2 bytes. Read() is expected to return the first 2 bytes from the
  // packet and discard the rest.
  const int kPartialReadSize = 2;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(kPartialReadSize);
  int rv =
      server_socket.RecvFrom(buffer.get(), kPartialReadSize,
                             &recv_from_address_, recv_callback.callback());
  rv = recv_callback.GetResult(rv);

  EXPECT_EQ(rv, ERR_MSG_TOO_BIG);

  // Send a different message again.
  std::string second_packet("Second packet");
  ASSERT_EQ(static_cast<int>(second_packet.size()),
            WriteSocket(&client_socket, second_packet));

  // Read whole packet now.
  std::string received = RecvFromSocket(&server_socket);
  EXPECT_EQ(second_packet, received);
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
// - MacOS: requires root permissions on OSX 10.7+.
// - Android: devices attached to testbots don't have default network, so
// broadcasting to 255.255.255.255 returns error -109 (Address not reachable).
// crbug.com/139144.
#define MAYBE_LocalBroadcast DISABLED_LocalBroadcast
#else
#define MAYBE_LocalBroadcast LocalBroadcast
#endif
TEST_F(UDPSocketTest, MAYBE_LocalBroadcast) {
  std::string first_message("first message"), second_message("second message");

  IPEndPoint listen_address;
  ASSERT_TRUE(CreateUDPAddress("0.0.0.0", 0 /* port */, &listen_address));

  auto server1 =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  auto server2 =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  server1->AllowAddressReuse();
  server1->AllowBroadcast();
  server2->AllowAddressReuse();
  server2->AllowBroadcast();

  EXPECT_THAT(server1->Listen(listen_address), IsOk());
  // Get bound port.
  EXPECT_THAT(server1->GetLocalAddress(&listen_address), IsOk());
  EXPECT_THAT(server2->Listen(listen_address), IsOk());

  IPEndPoint broadcast_address;
  ASSERT_TRUE(CreateUDPAddress("127.255.255.255", listen_address.port(),
                               &broadcast_address));
  ASSERT_EQ(static_cast<int>(first_message.size()),
            SendToSocket(server1.get(), first_message, broadcast_address));
  std::string str = RecvFromSocket(server1.get());
  ASSERT_EQ(first_message, str);
  str = RecvFromSocket(server2.get());
  ASSERT_EQ(first_message, str);

  ASSERT_EQ(static_cast<int>(second_message.size()),
            SendToSocket(server2.get(), second_message, broadcast_address));
  str = RecvFromSocket(server1.get());
  ASSERT_EQ(second_message, str);
  str = RecvFromSocket(server2.get());
  ASSERT_EQ(second_message, str);
}

// ConnectRandomBind verifies RANDOM_BIND is handled correctly. It connects
// 1000 sockets and then verifies that the allocated port numbers satisfy the
// following 2 conditions:
//  1. Range from min port value to max is greater than 10000.
//  2. There is at least one port in the 5 buckets in the [min, max] range.
//
// These conditions are not enough to verify that the port numbers are truly
// random, but they are enough to protect from most common non-random port
// allocation strategies (e.g. counter, pool of available ports, etc.) False
// positive result is theoretically possible, but its probability is negligible.
TEST_F(UDPSocketTest, ConnectRandomBind) {
  const int kIterations = 1000;

  std::vector<int> used_ports;
  for (int i = 0; i < kIterations; ++i) {
    UDPClientSocket socket(DatagramSocket::RANDOM_BIND, nullptr,
                           NetLogSource());
    EXPECT_THAT(socket.Connect(IPEndPoint(IPAddress::IPv4Localhost(), 53)),
                IsOk());

    IPEndPoint client_address;
    EXPECT_THAT(socket.GetLocalAddress(&client_address), IsOk());
    used_ports.push_back(client_address.port());
  }

  int min_port = *std::min_element(used_ports.begin(), used_ports.end());
  int max_port = *std::max_element(used_ports.begin(), used_ports.end());
  int range = max_port - min_port + 1;

  // Verify that the range of ports used by the random port allocator is wider
  // than 10k. Assuming that socket implementation limits port range to 16k
  // ports (default on Fuchsia) probability of false negative is below
  // 10^-200.
  static int kMinRange = 10000;
  EXPECT_GT(range, kMinRange);

  static int kBuckets = 5;
  std::vector<int> bucket_sizes(kBuckets, 0);
  for (int port : used_ports) {
    bucket_sizes[(port - min_port) * kBuckets / range] += 1;
  }

  // Verify that there is at least one value in each bucket. Probability of
  // false negative is below (kBuckets * (1 - 1 / kBuckets) ^ kIterations),
  // which is less than 10^-96.
  for (int size : bucket_sizes) {
    EXPECT_GT(size, 0);
  }
}

TEST_F(UDPSocketTest, ConnectFail) {
  UDPSocket socket(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());

  EXPECT_THAT(socket.Open(ADDRESS_FAMILY_IPV4), IsOk());

  // Connect to an IPv6 address should fail since the socket was created for
  // IPv4.
  EXPECT_THAT(socket.Connect(net::IPEndPoint(IPAddress::IPv6Localhost(), 53)),
              Not(IsOk()));

  // Make sure that UDPSocket actually closed the socket.
  EXPECT_FALSE(socket.is_connected());
}

// Similar to ConnectFail but UDPSocket adopts an opened socket instead of
// opening one directly.
TEST_F(UDPSocketTest, AdoptedSocket) {
  auto socketfd =
      CreatePlatformSocket(ConvertAddressFamily(ADDRESS_FAMILY_IPV4),
                           SOCK_DGRAM, AF_UNIX ? 0 : IPPROTO_UDP);
  UDPSocket socket(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());

  EXPECT_THAT(socket.AdoptOpenedSocket(ADDRESS_FAMILY_IPV4, socketfd), IsOk());

  // Connect to an IPv6 address should fail since the socket was created for
  // IPv4.
  EXPECT_THAT(socket.Connect(net::IPEndPoint(IPAddress::IPv6Localhost(), 53)),
              Not(IsOk()));

  // Make sure that UDPSocket actually closed the socket.
  EXPECT_FALSE(socket.is_connected());
}

// Tests that UDPSocket updates the global counter correctly.
TEST_F(UDPSocketTest, LimitAdoptSocket) {
  ASSERT_EQ(0, GetGlobalUDPSocketCountForTesting());
  {
    // Creating a platform socket does not increase count.
    auto socketfd =
        CreatePlatformSocket(ConvertAddressFamily(ADDRESS_FAMILY_IPV4),
                             SOCK_DGRAM, AF_UNIX ? 0 : IPPROTO_UDP);
    ASSERT_EQ(0, GetGlobalUDPSocketCountForTesting());

    // Simply allocating a UDPSocket does not increase count.
    UDPSocket socket(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
    EXPECT_EQ(0, GetGlobalUDPSocketCountForTesting());

    // Calling AdoptOpenedSocket() allocates the socket and increases the global
    // counter.
    EXPECT_THAT(socket.AdoptOpenedSocket(ADDRESS_FAMILY_IPV4, socketfd),
                IsOk());
    EXPECT_EQ(1, GetGlobalUDPSocketCountForTesting());

    // Connect to an IPv6 address should fail since the socket was created for
    // IPv4.
    EXPECT_THAT(socket.Connect(net::IPEndPoint(IPAddress::IPv6Localhost(), 53)),
                Not(IsOk()));

    // That Connect() failed doesn't change the global counter.
    EXPECT_EQ(1, GetGlobalUDPSocketCountForTesting());
  }
  // Finally, destroying UDPSocket decrements the global counter.
  EXPECT_EQ(0, GetGlobalUDPSocketCountForTesting());
}

// In this test, we verify that connect() on a socket will have the effect
// of filtering reads on this socket only to data read from the destination
// we connected to.
//
// The purpose of this test is that some documentation indicates that connect
// binds the client's sends to send to a particular server endpoint, but does
// not bind the client's reads to only be from that endpoint, and that we need
// to always use recvfrom() to disambiguate.
TEST_F(UDPSocketTest, VerifyConnectBindsAddr) {
  std::string simple_message("hello world!");
  std::string foreign_message("BAD MESSAGE TO GET!!");

  // Setup the first server to listen.
  IPEndPoint server1_address(IPAddress::IPv4Localhost(), 0 /* port */);
  UDPServerSocket server1(nullptr, NetLogSource());
  ASSERT_THAT(server1.Listen(server1_address), IsOk());
  // Get the bound port.
  ASSERT_THAT(server1.GetLocalAddress(&server1_address), IsOk());

  // Setup the second server to listen.
  IPEndPoint server2_address(IPAddress::IPv4Localhost(), 0 /* port */);
  UDPServerSocket server2(nullptr, NetLogSource());
  ASSERT_THAT(server2.Listen(server2_address), IsOk());

  // Setup the client, connected to server 1.
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  EXPECT_THAT(client.Connect(server1_address), IsOk());

  // Client sends to server1.
  EXPECT_EQ(simple_message.length(),
            static_cast<size_t>(WriteSocket(&client, simple_message)));

  // Server1 waits for message.
  std::string str = RecvFromSocket(&server1);
  EXPECT_EQ(simple_message, str);

  // Get the client's address.
  IPEndPoint client_address;
  EXPECT_THAT(client.GetLocalAddress(&client_address), IsOk());

  // Server2 sends reply.
  EXPECT_EQ(foreign_message.length(),
            static_cast<size_t>(
                SendToSocket(&server2, foreign_message, client_address)));

  // Server1 sends reply.
  EXPECT_EQ(simple_message.length(),
            static_cast<size_t>(
                SendToSocket(&server1, simple_message, client_address)));

  // Client waits for response.
  str = ReadSocket(&client);
  EXPECT_EQ(simple_message, str);
}

TEST_F(UDPSocketTest, ClientGetLocalPeerAddresses) {
  struct TestData {
    std::string remote_address;
    std::string local_address;
    bool may_fail;
  } tests[] = {
    {"127.0.00.1", "127.0.0.1", false},
    {"::1", "::1", true},
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
    // Addresses below are disabled on Android. See crbug.com/161248
    // They are also disabled on iOS. See https://crbug.com/523225
    {"192.168.1.1", "127.0.0.1", false},
    {"2001:db8:0::42", "::1", true},
#endif
  };
  for (const auto& test : tests) {
    SCOPED_TRACE(std::string("Connecting from ") + test.local_address +
                 std::string(" to ") + test.remote_address);

    IPAddress ip_address;
    EXPECT_TRUE(ip_address.AssignFromIPLiteral(test.remote_address));
    IPEndPoint remote_address(ip_address, 80);
    EXPECT_TRUE(ip_address.AssignFromIPLiteral(test.local_address));
    IPEndPoint local_address(ip_address, 80);

    UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr,
                           NetLogSource());
    int rv = client.Connect(remote_address);
    if (test.may_fail && rv == ERR_ADDRESS_UNREACHABLE) {
      // Connect() may return ERR_ADDRESS_UNREACHABLE for IPv6
      // addresses if IPv6 is not configured.
      continue;
    }

    EXPECT_LE(ERR_IO_PENDING, rv);

    IPEndPoint fetched_local_address;
    rv = client.GetLocalAddress(&fetched_local_address);
    EXPECT_THAT(rv, IsOk());

    // TODO(mbelshe): figure out how to verify the IP and port.
    //                The port is dynamically generated by the udp stack.
    //                The IP is the real IP of the client, not necessarily
    //                loopback.
    // EXPECT_EQ(local_address.address(), fetched_local_address.address());

    IPEndPoint fetched_remote_address;
    rv = client.GetPeerAddress(&fetched_remote_address);
    EXPECT_THAT(rv, IsOk());

    EXPECT_EQ(remote_address, fetched_remote_address);
  }
}

TEST_F(UDPSocketTest, ServerGetLocalAddress) {
  IPEndPoint bind_address(IPAddress::IPv4Localhost(), 0);
  UDPServerSocket server(nullptr, NetLogSource());
  int rv = server.Listen(bind_address);
  EXPECT_THAT(rv, IsOk());

  IPEndPoint local_address;
  rv = server.GetLocalAddress(&local_address);
  EXPECT_EQ(rv, 0);

  // Verify that port was allocated.
  EXPECT_GT(local_address.port(), 0);
  EXPECT_EQ(local_address.address(), bind_address.address());
}

TEST_F(UDPSocketTest, ServerGetPeerAddress) {
  IPEndPoint bind_address(IPAddress::IPv4Localhost(), 0);
  UDPServerSocket server(nullptr, NetLogSource());
  int rv = server.Listen(bind_address);
  EXPECT_THAT(rv, IsOk());

  IPEndPoint peer_address;
  rv = server.GetPeerAddress(&peer_address);
  EXPECT_EQ(rv, ERR_SOCKET_NOT_CONNECTED);
}

TEST_F(UDPSocketTest, ClientSetDoNotFragment) {
  for (std::string ip : {"127.0.0.1", "::1"}) {
    UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr,
                           NetLogSource());
    IPAddress ip_address;
    EXPECT_TRUE(ip_address.AssignFromIPLiteral(ip));
    IPEndPoint remote_address(ip_address, 80);
    int rv = client.Connect(remote_address);
    // May fail on IPv6 is IPv6 is not configured.
    if (ip_address.IsIPv6() && rv == ERR_ADDRESS_UNREACHABLE)
      return;
    EXPECT_THAT(rv, IsOk());

    rv = client.SetDoNotFragment();
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA)
    // TODO(crbug.com/42050633): IP_MTU_DISCOVER is not implemented on Fuchsia.
    EXPECT_THAT(rv, IsError(ERR_NOT_IMPLEMENTED));
#else
    EXPECT_THAT(rv, IsOk());
#endif
  }
}

TEST_F(UDPSocketTest, ServerSetDoNotFragment) {
  for (std::string ip : {"127.0.0.1", "::1"}) {
    IPEndPoint bind_address;
    ASSERT_TRUE(CreateUDPAddress(ip, 0, &bind_address));
    UDPServerSocket server(nullptr, NetLogSource());
    int rv = server.Listen(bind_address);
    // May fail on IPv6 is IPv6 is not configure
    if (bind_address.address().IsIPv6() &&
        (rv == ERR_ADDRESS_INVALID || rv == ERR_ADDRESS_UNREACHABLE))
      return;
    EXPECT_THAT(rv, IsOk());

    rv = server.SetDoNotFragment();
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_FUCHSIA)
    // TODO(crbug.com/42050633): IP_MTU_DISCOVER is not implemented on Fuchsia.
    EXPECT_THAT(rv, IsError(ERR_NOT_IMPLEMENTED));
#else
    EXPECT_THAT(rv, IsOk());
#endif
  }
}

// Close the socket while read is pending.
TEST_F(UDPSocketTest, CloseWithPendingRead) {
  IPEndPoint bind_address(IPAddress::IPv4Localhost(), 0);
  UDPServerSocket server(nullptr, NetLogSource());
  int rv = server.Listen(bind_address);
  EXPECT_THAT(rv, IsOk());

  TestCompletionCallback callback;
  IPEndPoint from;
  rv = server.RecvFrom(buffer_.get(), kMaxRead, &from, callback.callback());
  EXPECT_EQ(rv, ERR_IO_PENDING);

  server.Close();

  EXPECT_FALSE(callback.have_result());
}

// Some Android devices do not support multicast.
// The ones supporting multicast need WifiManager.MulitcastLock to enable it.
// http://goo.gl/jjAk9
#if !BUILDFLAG(IS_ANDROID)
TEST_F(UDPSocketTest, JoinMulticastGroup) {
#if BUILDFLAG(IS_MAC)
  // See https://crbug.com/354933441
  if (base::mac::MacOSMajorVersion() == 15) {
    GTEST_SKIP() << "Disabled on macOS Sequoia.";
  }
#endif

  const char kGroup[] = "237.132.100.17";

  IPAddress group_ip;
  EXPECT_TRUE(group_ip.AssignFromIPLiteral(kGroup));
// TODO(https://github.com/google/gvisor/issues/3839): don't guard on
// OS_FUCHSIA.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
  IPEndPoint bind_address(IPAddress::AllZeros(group_ip.size()), 0 /* port */);
#else
  IPEndPoint bind_address(group_ip, 0 /* port */);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)

  UDPSocket socket(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  EXPECT_THAT(socket.Open(bind_address.GetFamily()), IsOk());

  EXPECT_THAT(socket.Bind(bind_address), IsOk());
  EXPECT_THAT(socket.JoinGroup(group_ip), IsOk());
  // Joining group multiple times.
  EXPECT_NE(OK, socket.JoinGroup(group_ip));
  EXPECT_THAT(socket.LeaveGroup(group_ip), IsOk());
  // Leaving group multiple times.
  EXPECT_NE(OK, socket.LeaveGroup(group_ip));

  socket.Close();
}

// TODO(crbug.com/40620614): failing on device on iOS 12.2.
// TODO(crbug.com/40189274): flaky on Mac 11.
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_MAC)
#define MAYBE_SharedMulticastAddress DISABLED_SharedMulticastAddress
#else
#define MAYBE_SharedMulticastAddress SharedMulticastAddress
#endif
TEST_F(UDPSocketTest, MAYBE_SharedMulticastAddress) {
  const char kGroup[] = "224.0.0.251";

  IPAddress group_ip;
  ASSERT_TRUE(group_ip.AssignFromIPLiteral(kGroup));
// TODO(https://github.com/google/gvisor/issues/3839): don't guard on
// OS_FUCHSIA.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
  IPEndPoint receive_address(IPAddress::AllZeros(group_ip.size()),
                             0 /* port */);
#else
  IPEndPoint receive_address(group_ip, 0 /* port */);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)

  NetworkInterfaceList interfaces;
  ASSERT_TRUE(GetNetworkList(&interfaces, 0));
  // The test fails with the Hyper-V switch interface (on the host side).
  interfaces.erase(std::remove_if(interfaces.begin(), interfaces.end(),
                                  [](const auto& iface) {
                                    return iface.friendly_name.rfind(
                                               "vEthernet", 0) == 0;
                                  }),
                   interfaces.end());
  ASSERT_FALSE(interfaces.empty());

  // Setup first receiving socket.
  UDPServerSocket socket1(nullptr, NetLogSource());
  socket1.AllowAddressSharingForMulticast();
  ASSERT_THAT(socket1.SetMulticastInterface(interfaces[0].interface_index),
              IsOk());
  ASSERT_THAT(socket1.Listen(receive_address), IsOk());
  ASSERT_THAT(socket1.JoinGroup(group_ip), IsOk());
  // Get the bound port.
  ASSERT_THAT(socket1.GetLocalAddress(&receive_address), IsOk());

  // Setup second receiving socket.
  UDPServerSocket socket2(nullptr, NetLogSource());
  socket2.AllowAddressSharingForMulticast(), IsOk();
  ASSERT_THAT(socket2.SetMulticastInterface(interfaces[0].interface_index),
              IsOk());
  ASSERT_THAT(socket2.Listen(receive_address), IsOk());
  ASSERT_THAT(socket2.JoinGroup(group_ip), IsOk());

  // Setup client socket.
  IPEndPoint send_address(group_ip, receive_address.port());
  UDPClientSocket client_socket(DatagramSocket::DEFAULT_BIND, nullptr,
                                NetLogSource());
  ASSERT_THAT(client_socket.Connect(send_address), IsOk());

#if !BUILDFLAG(IS_CHROMEOS)
  // Send a message via the multicast group. That message is expected be be
  // received by both receving sockets.
  //
  // Skip on ChromeOS where it's known to sometimes not work.
  // TODO(crbug.com/898964): If possible, fix and reenable.
  const char kMessage[] = "hello!";
  ASSERT_GE(WriteSocket(&client_socket, kMessage), 0);
  EXPECT_EQ(kMessage, RecvFromSocket(&socket1));
  EXPECT_EQ(kMessage, RecvFromSocket(&socket2));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(UDPSocketTest, MulticastOptions) {
  IPEndPoint bind_address;
  ASSERT_TRUE(CreateUDPAddress("0.0.0.0", 0 /* port */, &bind_address));

  UDPSocket socket(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  // Before binding.
  EXPECT_THAT(socket.SetMulticastLoopbackMode(false), IsOk());
  EXPECT_THAT(socket.SetMulticastLoopbackMode(true), IsOk());
  EXPECT_THAT(socket.SetMulticastTimeToLive(0), IsOk());
  EXPECT_THAT(socket.SetMulticastTimeToLive(3), IsOk());
  EXPECT_NE(OK, socket.SetMulticastTimeToLive(-1));
  EXPECT_THAT(socket.SetMulticastInterface(0), IsOk());

  EXPECT_THAT(socket.Open(bind_address.GetFamily()), IsOk());
  EXPECT_THAT(socket.Bind(bind_address), IsOk());

  EXPECT_NE(OK, socket.SetMulticastLoopbackMode(false));
  EXPECT_NE(OK, socket.SetMulticastTimeToLive(0));
  EXPECT_NE(OK, socket.SetMulticastInterface(0));

  socket.Close();
}

// Checking that DSCP bits are set correctly is difficult,
// but let's check that the code doesn't crash at least.
TEST_F(UDPSocketTest, SetDSCP) {
  // Setup the server to listen.
  IPEndPoint bind_address;
  UDPSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  // We need a real IP, but we won't actually send anything to it.
  ASSERT_TRUE(CreateUDPAddress("8.8.8.8", 9999, &bind_address));
  int rv = client.Open(bind_address.GetFamily());
  EXPECT_THAT(rv, IsOk());

  rv = client.Connect(bind_address);
  if (rv != OK) {
    // Let's try localhost then.
    bind_address = IPEndPoint(IPAddress::IPv4Localhost(), 9999);
    rv = client.Connect(bind_address);
  }
  EXPECT_THAT(rv, IsOk());

  client.SetDiffServCodePoint(DSCP_NO_CHANGE);
  client.SetDiffServCodePoint(DSCP_AF41);
  client.SetDiffServCodePoint(DSCP_DEFAULT);
  client.SetDiffServCodePoint(DSCP_CS2);
  client.SetDiffServCodePoint(DSCP_NO_CHANGE);
  client.SetDiffServCodePoint(DSCP_DEFAULT);
  client.Close();
}

// Send DSCP + ECN marked packets from server to client and verify the TOS
// bytes that arrive.
TEST_F(UDPSocketTest, VerifyDscpAndEcnExchangeV4) {
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  UDPServerSocket server(nullptr, NetLogSource());
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(server_address), IsOk());
  // Get bound port.
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  client.Connect(server_address);
  EXPECT_EQ(client.SetRecvTos(), 0);
  EXPECT_EQ(server.SetRecvTos(), 0);

#if BUILDFLAG(IS_WIN)
  // Do not exercise the DSCP code because it requires a mock Qwave API.
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_ECT1), 0);
#else
  EXPECT_EQ(client.SetTos(DSCP_AF41, ECN_ECT1), 0);
#endif
  std::string client_message = "hello";
  EXPECT_EQ(WriteSocket(&client, client_message),
            static_cast<int>(client_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, ECN_ECT1),
            client_message.data());

  // Server messages
  EXPECT_EQ(server.SetTos(DSCP_AF41, ECN_ECT1), 0);
  std::string first_message = "foobar";
  EXPECT_EQ(SendToSocket(&server, first_message),
            static_cast<int>(first_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, ECN_ECT1), first_message.data());

  std::string second_message = "foo";
  EXPECT_EQ(server.SetTos(DSCP_CS2, ECN_ECT0), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_CS2, ECN_ECT0), second_message.data());

#if BUILDFLAG(IS_WIN)
  // The Windows sendmsg API does not allow setting ECN_CE as the outgoing mark.
  EcnCodePoint final_ecn = ECN_ECT1;
#else
  EcnCodePoint final_ecn = ECN_CE;
#endif

  EXPECT_EQ(server.SetTos(DSCP_NO_CHANGE, final_ecn), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_CS2, final_ecn), second_message.data());

  EXPECT_EQ(server.SetTos(DSCP_AF41, ECN_NO_CHANGE), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, final_ecn), second_message.data());

  EXPECT_EQ(server.SetTos(DSCP_NO_CHANGE, ECN_NO_CHANGE), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, final_ecn), second_message.data());

  server.Close();
  client.Close();
}

// Send DSCP + ECN marked packets from server to client and verify the TOS
// bytes that arrive.
TEST_F(UDPSocketTest, VerifyDscpAndEcnExchangeV6) {
  IPEndPoint server_address(IPAddress::IPv6Localhost(), 0);
  UDPServerSocket server(nullptr, NetLogSource());
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(server_address), IsOk());
  // Get bound port.
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  EXPECT_THAT(client.Connect(server_address), IsOk());
  EXPECT_EQ(client.SetRecvTos(), 0);
  EXPECT_EQ(server.SetRecvTos(), 0);

#if BUILDFLAG(IS_WIN)
  // Do not exercise the DSCP code because it requires a mock Qwave API.
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_ECT1), 0);
#else
  EXPECT_EQ(client.SetTos(DSCP_AF41, ECN_ECT1), 0);
#endif
  std::string client_message = "hello";
  EXPECT_EQ(WriteSocket(&client, client_message),
            static_cast<int>(client_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, ECN_ECT1),
            client_message.data());

  // Server messages
  EXPECT_EQ(server.SetTos(DSCP_AF41, ECN_ECT1), 0);
  std::string first_message = "foobar";
  EXPECT_EQ(SendToSocket(&server, first_message),
            static_cast<int>(first_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, ECN_ECT1), first_message.data());

  std::string second_message = "foo";
  EXPECT_EQ(server.SetTos(DSCP_CS2, ECN_ECT0), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_CS2, ECN_ECT0), second_message.data());

#if BUILDFLAG(IS_WIN)
  // The Windows sendmsg API does not allow setting ECN_CE as the outgoing mark.
  EcnCodePoint final_ecn = ECN_ECT1;
#else
  EcnCodePoint final_ecn = ECN_CE;
#endif

  EXPECT_EQ(server.SetTos(DSCP_NO_CHANGE, final_ecn), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_CS2, final_ecn), second_message.data());

  EXPECT_EQ(server.SetTos(DSCP_AF41, ECN_NO_CHANGE), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, final_ecn), second_message.data());

  EXPECT_EQ(server.SetTos(DSCP_NO_CHANGE, ECN_NO_CHANGE), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, final_ecn), second_message.data());

  server.Close();
  client.Close();
}

// Send DSCP + ECN marked packets from client to a dual-stack server and verify
// the TOS bytes that arrive.
TEST_F(UDPSocketTest, VerifyDscpAndEcnExchangeDualStack) {
  IPEndPoint server_v6_address(IPAddress::IPv6AllZeros(), 0);
  UDPServerSocket server(nullptr, NetLogSource());
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(server_v6_address), IsOk());
  // Get bound port.
  ASSERT_THAT(server.GetLocalAddress(&server_v6_address), IsOk());
  // The server is bound to IPV6_ANY, so it will receive IPv4 packets addressed
  // to localhost.
  IPEndPoint server_v4_address(IPAddress::IPv4Localhost(),
                               server_v6_address.port());
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  EXPECT_THAT(client.Connect(server_v4_address), IsOk());
  EXPECT_EQ(server.SetRecvTos(), 0);

#if BUILDFLAG(IS_WIN)
  // Windows requires a Mock QWave API to allow the client to set the DSCP. For
  // efficiency reasons, Chromium windows UDP sockets do not provide access to
  // incoming DSCP anyway. To avoid all the mocking, don't set the DSCP at all
  // for Windows. RecvFromSocket() doesn't check the DSCP for Windows.
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_ECT1), 0);
#else
  EXPECT_EQ(client.SetTos(DSCP_AF41, ECN_ECT1), 0);
#endif  //! BUILDFLAG(IS_WIN)
  std::string first_message = "foobar";
  EXPECT_EQ(WriteSocket(&client, first_message),
            static_cast<int>(first_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, ECN_ECT1), first_message.data());

  std::string second_message = "foo";
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_ECT0), 0);
#else
  EXPECT_EQ(client.SetTos(DSCP_CS2, ECN_ECT0), 0);
#endif
  EXPECT_EQ(WriteSocket(&client, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_CS2, ECN_ECT0), second_message.data());

#if BUILDFLAG(IS_WIN)
  // The Windows sendmsg API does not allow setting ECN_CE as the outgoing mark.
  EcnCodePoint final_ecn = ECN_ECT1;
#else
  EcnCodePoint final_ecn = ECN_CE;
#endif
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, final_ecn), 0);
  EXPECT_EQ(WriteSocket(&client, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_CS2, final_ecn),
            second_message.data());

#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(client.SetTos(DSCP_AF41, ECN_NO_CHANGE), 0);
#endif
  EXPECT_EQ(WriteSocket(&client, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, final_ecn),
            second_message.data());

  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_NO_CHANGE), 0);
  EXPECT_EQ(WriteSocket(&client, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, final_ecn),
            second_message.data());

  server.Close();
  client.Close();
}

// Send DSCP + ECN marked packets from client to a dual-stack server and verify
// the TOS bytes that arrive.
TEST_F(UDPSocketTest, VerifyDscpAndEcnExchangeDualStackV4Mapped) {
  // Bind to a v4-mapped localhost address
  IPEndPoint server_v6_address(*IPAddress::FromIPLiteral("::ffff:7f00:0001"),
                               0);
  UDPServerSocket server(nullptr, NetLogSource());
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(server_v6_address), IsOk());
  // Get bound port.
  ASSERT_THAT(server.GetLocalAddress(&server_v6_address), IsOk());
  IPEndPoint server_v4_address(IPAddress::IPv4Localhost(),
                               server_v6_address.port());
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  EXPECT_THAT(client.Connect(server_v4_address), IsOk());
  EXPECT_EQ(server.SetRecvTos(), 0);

#if BUILDFLAG(IS_WIN)
  // Windows requires a Mock QWave API to allow the client to set the DSCP. For
  // efficiency reasons, Chromium windows UDP sockets do not provide access to
  // incoming DSCP anyway. To avoid all the mocking, don't set the DSCP at all
  // for Windows. RecvFromSocket() doesn't check the DSCP for Windows.
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_ECT1), 0);
#else
  EXPECT_EQ(client.SetTos(DSCP_AF41, ECN_ECT1), 0);
#endif
  std::string first_message = "foobar";
  EXPECT_EQ(WriteSocket(&client, first_message),
            static_cast<int>(first_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, ECN_ECT1), first_message.data());

  std::string second_message = "foo";
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_ECT0), 0);
#else
  EXPECT_EQ(client.SetTos(DSCP_CS2, ECN_ECT0), 0);
#endif
  EXPECT_EQ(WriteSocket(&client, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_CS2, ECN_ECT0), second_message.data());

#if BUILDFLAG(IS_WIN)
  // The Windows sendmsg API does not allow setting ECN_CE as the outgoing mark.
  EcnCodePoint final_ecn = ECN_ECT1;
#else
  EcnCodePoint final_ecn = ECN_CE;
#endif
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, final_ecn), 0);
  EXPECT_EQ(WriteSocket(&client, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_CS2, final_ecn),
            second_message.data());

#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(client.SetTos(DSCP_AF41, ECN_NO_CHANGE), 0);
#endif
  EXPECT_EQ(WriteSocket(&client, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, final_ecn),
            second_message.data());

  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_NO_CHANGE), 0);
  EXPECT_EQ(WriteSocket(&client, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, final_ecn),
            second_message.data());

  server.Close();
  client.Close();
}

// For windows, test with Nonblocking sockets. For other platforms, this test
// is identical to VerifyDscpAndEcnExchange, above.
TEST_F(UDPSocketTest, VerifyDscpAndEcnExchangeNonBlocking) {
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  UDPServerSocket server(nullptr, NetLogSource());
  server.UseNonBlockingIO();
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(server_address), IsOk());
  // Get bound port.
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  client.UseNonBlockingIO();
  client.Connect(server_address);
  EXPECT_EQ(client.SetRecvTos(), 0);
  EXPECT_EQ(server.SetRecvTos(), 0);

#if BUILDFLAG(IS_WIN)
  // Do not exercise the DSCP code because it requires a mock Qwave API.
  EXPECT_EQ(client.SetTos(DSCP_NO_CHANGE, ECN_ECT1), 0);
#else
  EXPECT_EQ(client.SetTos(DSCP_AF41, ECN_ECT1), 0);
#endif
  std::string client_message = "hello";
  EXPECT_EQ(WriteSocket(&client, client_message),
            static_cast<int>(client_message.length()));
  EXPECT_EQ(RecvFromSocket(&server, DSCP_AF41, ECN_ECT1),
            client_message.data());

  // Server messages
  EXPECT_EQ(server.SetTos(DSCP_AF41, ECN_ECT1), 0);
  std::string first_message = "foobar";
  EXPECT_EQ(SendToSocket(&server, first_message),
            static_cast<int>(first_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, ECN_ECT1), first_message.data());

  std::string second_message = "foo";
  EXPECT_EQ(server.SetTos(DSCP_CS2, ECN_ECT0), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_CS2, ECN_ECT0), second_message.data());

  // The Windows sendmsg API does not allow setting ECN_CE as the outgoing mark.
  EcnCodePoint final_ecn = ECN_ECT1;

  EXPECT_EQ(server.SetTos(DSCP_NO_CHANGE, final_ecn), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_CS2, final_ecn), second_message.data());

  EXPECT_EQ(server.SetTos(DSCP_AF41, ECN_NO_CHANGE), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, final_ecn), second_message.data());

  EXPECT_EQ(server.SetTos(DSCP_NO_CHANGE, ECN_NO_CHANGE), 0);
  EXPECT_EQ(SendToSocket(&server, second_message),
            static_cast<int>(second_message.length()));
  EXPECT_EQ(ReadSocket(&client, DSCP_AF41, final_ecn), second_message.data());

  server.Close();
  client.Close();
}

TEST_F(UDPSocketTest, ConnectUsingNetwork) {
  // The specific value of this address doesn't really matter, and no
  // server needs to be running here. The test only needs to call
  // ConnectUsingNetwork() and won't send any datagrams.
  const IPEndPoint fake_server_address(IPAddress::IPv4Localhost(), 8080);
  const handles::NetworkHandle wrong_network_handle = 65536;
#if BUILDFLAG(IS_ANDROID)
  NetworkChangeNotifierFactoryAndroid ncn_factory;
  NetworkChangeNotifier::DisableForTest ncn_disable_for_test;
  std::unique_ptr<NetworkChangeNotifier> ncn(ncn_factory.CreateInstance());
  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    GTEST_SKIP() << "Network handles are required to test BindToNetwork.";

  {
    // Connecting using a not existing network should fail but not report
    // ERR_NOT_IMPLEMENTED when network handles are supported.
    UDPClientSocket socket(DatagramSocket::RANDOM_BIND, nullptr,
                           NetLogSource());
    int rv =
        socket.ConnectUsingNetwork(wrong_network_handle, fake_server_address);
    EXPECT_NE(ERR_NOT_IMPLEMENTED, rv);
    EXPECT_NE(OK, rv);
    EXPECT_NE(wrong_network_handle, socket.GetBoundNetwork());
  }

  {
    // Connecting using an existing network should succeed when
    // NetworkChangeNotifier returns a valid default network.
    UDPClientSocket socket(DatagramSocket::RANDOM_BIND, nullptr,
                           NetLogSource());
    const handles::NetworkHandle network_handle =
        NetworkChangeNotifier::GetDefaultNetwork();
    if (network_handle != handles::kInvalidNetworkHandle) {
      EXPECT_EQ(
          OK, socket.ConnectUsingNetwork(network_handle, fake_server_address));
      EXPECT_EQ(network_handle, socket.GetBoundNetwork());
    }
  }
#else
  UDPClientSocket socket(DatagramSocket::RANDOM_BIND, nullptr, NetLogSource());
  EXPECT_EQ(
      ERR_NOT_IMPLEMENTED,
      socket.ConnectUsingNetwork(wrong_network_handle, fake_server_address));
#endif  // BUILDFLAG(IS_ANDROID)
}

TEST_F(UDPSocketTest, ConnectUsingNetworkAsync) {
  // The specific value of this address doesn't really matter, and no
  // server needs to be running here. The test only needs to call
  // ConnectUsingNetwork() and won't send any datagrams.
  const IPEndPoint fake_server_address(IPAddress::IPv4Localhost(), 8080);
  const handles::NetworkHandle wrong_network_handle = 65536;
#if BUILDFLAG(IS_ANDROID)
  NetworkChangeNotifierFactoryAndroid ncn_factory;
  NetworkChangeNotifier::DisableForTest ncn_disable_for_test;
  std::unique_ptr<NetworkChangeNotifier> ncn(ncn_factory.CreateInstance());
  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    GTEST_SKIP() << "Network handles are required to test BindToNetwork.";

  {
    // Connecting using a not existing network should fail but not report
    // ERR_NOT_IMPLEMENTED when network handles are supported.
    UDPClientSocket socket(DatagramSocket::RANDOM_BIND, nullptr,
                           NetLogSource());
    TestCompletionCallback callback;
    int rv = socket.ConnectUsingNetworkAsync(
        wrong_network_handle, fake_server_address, callback.callback());

    if (rv == ERR_IO_PENDING) {
      rv = callback.WaitForResult();
    }
    EXPECT_NE(ERR_NOT_IMPLEMENTED, rv);
    EXPECT_NE(OK, rv);
  }

  {
    // Connecting using an existing network should succeed when
    // NetworkChangeNotifier returns a valid default network.
    UDPClientSocket socket(DatagramSocket::RANDOM_BIND, nullptr,
                           NetLogSource());
    TestCompletionCallback callback;
    const handles::NetworkHandle network_handle =
        NetworkChangeNotifier::GetDefaultNetwork();
    if (network_handle != handles::kInvalidNetworkHandle) {
      int rv = socket.ConnectUsingNetworkAsync(
          network_handle, fake_server_address, callback.callback());
      if (rv == ERR_IO_PENDING) {
        rv = callback.WaitForResult();
      }
      EXPECT_EQ(OK, rv);
      EXPECT_EQ(network_handle, socket.GetBoundNetwork());
    }
  }
#else
  UDPClientSocket socket(DatagramSocket::RANDOM_BIND, nullptr, NetLogSource());
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_NOT_IMPLEMENTED, socket.ConnectUsingNetworkAsync(
                                     wrong_network_handle, fake_server_address,
                                     callback.callback()));
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace

#if BUILDFLAG(IS_WIN)

namespace {

const HANDLE kFakeHandle1 = (HANDLE)12;
const HANDLE kFakeHandle2 = (HANDLE)13;

const QOS_FLOWID kFakeFlowId1 = (QOS_FLOWID)27;
const QOS_FLOWID kFakeFlowId2 = (QOS_FLOWID)38;

class TestUDPSocketWin : public UDPSocketWin {
 public:
  TestUDPSocketWin(QwaveApi* qos,
                   DatagramSocket::BindType bind_type,
                   net::NetLog* net_log,
                   const net::NetLogSource& source)
      : UDPSocketWin(bind_type, net_log, source), qos_(qos) {}

  TestUDPSocketWin(const TestUDPSocketWin&) = delete;
  TestUDPSocketWin& operator=(const TestUDPSocketWin&) = delete;

  // Overriding GetQwaveApi causes the test class to use the injected mock
  // QwaveApi instance instead of the singleton.
  QwaveApi* GetQwaveApi() const override { return qos_; }

 private:
  raw_ptr<QwaveApi> qos_;
};

class MockQwaveApi : public QwaveApi {
 public:
  MOCK_CONST_METHOD0(qwave_supported, bool());
  MOCK_METHOD0(OnFatalError, void());
  MOCK_METHOD2(CreateHandle, BOOL(PQOS_VERSION version, PHANDLE handle));
  MOCK_METHOD1(CloseHandle, BOOL(HANDLE handle));
  MOCK_METHOD6(AddSocketToFlow,
               BOOL(HANDLE handle,
                    SOCKET socket,
                    PSOCKADDR addr,
                    QOS_TRAFFIC_TYPE traffic_type,
                    DWORD flags,
                    PQOS_FLOWID flow_id));

  MOCK_METHOD4(
      RemoveSocketFromFlow,
      BOOL(HANDLE handle, SOCKET socket, QOS_FLOWID flow_id, DWORD reserved));
  MOCK_METHOD7(SetFlow,
               BOOL(HANDLE handle,
                    QOS_FLOWID flow_id,
                    QOS_SET_FLOW op,
                    ULONG size,
                    PVOID data,
                    DWORD reserved,
                    LPOVERLAPPED overlapped));
};

std::unique_ptr<UDPSocket> OpenedDscpTestClient(QwaveApi* api,
                                                IPEndPoint bind_address) {
  auto client = std::make_unique<TestUDPSocketWin>(
      api, DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  int rv = client->Open(bind_address.GetFamily());
  EXPECT_THAT(rv, IsOk());

  return client;
}

std::unique_ptr<UDPSocket> ConnectedDscpTestClient(QwaveApi* api) {
  IPEndPoint bind_address;
  // We need a real IP, but we won't actually send anything to it.
  EXPECT_TRUE(CreateUDPAddress("8.8.8.8", 9999, &bind_address));
  auto client = OpenedDscpTestClient(api, bind_address);
  EXPECT_THAT(client->Connect(bind_address), IsOk());
  return client;
}

std::unique_ptr<UDPSocket> UnconnectedDscpTestClient(QwaveApi* api) {
  IPEndPoint bind_address;
  EXPECT_TRUE(CreateUDPAddress("0.0.0.0", 9999, &bind_address));
  auto client = OpenedDscpTestClient(api, bind_address);
  EXPECT_THAT(client->Bind(bind_address), IsOk());
  return client;
}

}  // namespace

using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

TEST_F(UDPSocketTest, SetDSCPNoopIfPassedNoChange) {
  MockQwaveApi api;
  EXPECT_CALL(api, qwave_supported()).WillRepeatedly(Return(true));

  EXPECT_CALL(api, AddSocketToFlow(_, _, _, _, _, _)).Times(0);
  std::unique_ptr<UDPSocket> client = ConnectedDscpTestClient(&api);
  EXPECT_THAT(client->SetDiffServCodePoint(DSCP_NO_CHANGE), IsOk());
}

TEST_F(UDPSocketTest, SetDSCPFailsIfQOSDoesntLink) {
  MockQwaveApi api;
  EXPECT_CALL(api, qwave_supported()).WillRepeatedly(Return(false));
  EXPECT_CALL(api, CreateHandle(_, _)).Times(0);

  std::unique_ptr<UDPSocket> client = ConnectedDscpTestClient(&api);
  EXPECT_EQ(ERR_NOT_IMPLEMENTED, client->SetDiffServCodePoint(DSCP_AF41));
}

TEST_F(UDPSocketTest, SetDSCPFailsIfHandleCantBeCreated) {
  MockQwaveApi api;
  EXPECT_CALL(api, qwave_supported()).WillRepeatedly(Return(true));
  EXPECT_CALL(api, CreateHandle(_, _)).WillOnce(Return(false));
  EXPECT_CALL(api, OnFatalError()).Times(1);

  std::unique_ptr<UDPSocket> client = ConnectedDscpTestClient(&api);
  EXPECT_EQ(ERR_INVALID_HANDLE, client->SetDiffServCodePoint(DSCP_AF41));

  RunUntilIdle();

  EXPECT_CALL(api, qwave_supported()).WillRepeatedly(Return(false));
  EXPECT_EQ(ERR_NOT_IMPLEMENTED, client->SetDiffServCodePoint(DSCP_AF41));
}

MATCHER_P(DscpPointee, dscp, "") {
  return *(DWORD*)arg == (DWORD)dscp;
}

TEST_F(UDPSocketTest, ConnectedSocketDelayedInitAndUpdate) {
  MockQwaveApi api;
  std::unique_ptr<UDPSocket> client = ConnectedDscpTestClient(&api);
  EXPECT_CALL(api, qwave_supported()).WillRepeatedly(Return(true));
  EXPECT_CALL(api, CreateHandle(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFakeHandle1), Return(true)));

  EXPECT_CALL(api, AddSocketToFlow(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId1), Return(true)));
  EXPECT_CALL(api, SetFlow(_, _, _, _, _, _, _));

  // First set on connected sockets will fail since init is async and
  // we haven't given the runloop a chance to execute the callback.
  EXPECT_EQ(ERR_INVALID_HANDLE, client->SetDiffServCodePoint(DSCP_AF41));
  RunUntilIdle();
  EXPECT_THAT(client->SetDiffServCodePoint(DSCP_AF41), IsOk());

  // New dscp value should reset the flow.
  EXPECT_CALL(api, RemoveSocketFromFlow(_, _, kFakeFlowId1, _));
  EXPECT_CALL(api, AddSocketToFlow(_, _, _, QOSTrafficTypeBestEffort, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId2), Return(true)));
  EXPECT_CALL(api, SetFlow(_, _, QOSSetOutgoingDSCPValue, _,
                           DscpPointee(DSCP_DEFAULT), _, _));
  EXPECT_THAT(client->SetDiffServCodePoint(DSCP_DEFAULT), IsOk());

  // Called from DscpManager destructor.
  EXPECT_CALL(api, RemoveSocketFromFlow(_, _, kFakeFlowId2, _));
  EXPECT_CALL(api, CloseHandle(kFakeHandle1));
}

TEST_F(UDPSocketTest, UnonnectedSocketDelayedInitAndUpdate) {
  MockQwaveApi api;
  EXPECT_CALL(api, qwave_supported()).WillRepeatedly(Return(true));
  EXPECT_CALL(api, CreateHandle(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFakeHandle1), Return(true)));

  // CreateHandle won't have completed yet.  Set passes.
  std::unique_ptr<UDPSocket> client = UnconnectedDscpTestClient(&api);
  EXPECT_THAT(client->SetDiffServCodePoint(DSCP_AF41), IsOk());

  RunUntilIdle();
  EXPECT_THAT(client->SetDiffServCodePoint(DSCP_AF42), IsOk());

  // Called from DscpManager destructor.
  EXPECT_CALL(api, CloseHandle(kFakeHandle1));
}

// TODO(zstein): Mocking out DscpManager might be simpler here
// (just verify that DscpManager::Set and DscpManager::PrepareForSend are
// called).
TEST_F(UDPSocketTest, SendToCallsQwaveApis) {
  MockQwaveApi api;
  std::unique_ptr<UDPSocket> client = UnconnectedDscpTestClient(&api);
  EXPECT_CALL(api, qwave_supported()).WillRepeatedly(Return(true));
  EXPECT_CALL(api, CreateHandle(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFakeHandle1), Return(true)));
  EXPECT_THAT(client->SetDiffServCodePoint(DSCP_AF41), IsOk());
  RunUntilIdle();

  EXPECT_CALL(api, AddSocketToFlow(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId1), Return(true)));
  EXPECT_CALL(api, SetFlow(_, _, _, _, _, _, _));
  std::string simple_message("hello world");
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 9438);
  int rv = SendToSocket(client.get(), simple_message, server_address);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // TODO(zstein): Move to second test case (Qwave APIs called once per address)
  rv = SendToSocket(client.get(), simple_message, server_address);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // TODO(zstein): Move to third test case (Qwave APIs called for each
  // destination address).
  EXPECT_CALL(api, AddSocketToFlow(_, _, _, _, _, _)).WillOnce(Return(true));
  IPEndPoint server_address2(IPAddress::IPv4Localhost(), 9439);

  rv = SendToSocket(client.get(), simple_message, server_address2);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Called from DscpManager destructor.
  EXPECT_CALL(api, RemoveSocketFromFlow(_, _, _, _));
  EXPECT_CALL(api, CloseHandle(kFakeHandle1));
}

TEST_F(UDPSocketTest, SendToCallsApisAfterDeferredInit) {
  MockQwaveApi api;
  std::unique_ptr<UDPSocket> client = UnconnectedDscpTestClient(&api);
  EXPECT_CALL(api, qwave_supported()).WillRepeatedly(Return(true));
  EXPECT_CALL(api, CreateHandle(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFakeHandle1), Return(true)));

  // SetDiffServCodepoint works even if qos api hasn't finished initing.
  EXPECT_THAT(client->SetDiffServCodePoint(DSCP_CS7), IsOk());

  std::string simple_message("hello world");
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 9438);

  // SendTo works, but doesn't yet apply TOS
  EXPECT_CALL(api, AddSocketToFlow(_, _, _, _, _, _)).Times(0);
  int rv = SendToSocket(client.get(), simple_message, server_address);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  RunUntilIdle();
  // Now we're initialized, SendTo triggers qos calls with correct codepoint.
  EXPECT_CALL(api, AddSocketToFlow(_, _, _, QOSTrafficTypeControl, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId1), Return(true)));
  EXPECT_CALL(api, SetFlow(_, _, _, _, _, _, _)).WillOnce(Return(true));
  rv = SendToSocket(client.get(), simple_message, server_address);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));

  // Called from DscpManager destructor.
  EXPECT_CALL(api, RemoveSocketFromFlow(_, _, kFakeFlowId1, _));
  EXPECT_CALL(api, CloseHandle(kFakeHandle1));
}

class DscpManagerTest : public TestWithTaskEnvironment {
 protected:
  DscpManagerTest() {
    EXPECT_CALL(api_, qwave_supported()).WillRepeatedly(Return(true));
    EXPECT_CALL(api_, CreateHandle(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(kFakeHandle1), Return(true)));
    dscp_manager_ = std::make_unique<DscpManager>(&api_, INVALID_SOCKET);

    CreateUDPAddress("1.2.3.4", 9001, &address1_);
    CreateUDPAddress("1234:5678:90ab:cdef:1234:5678:90ab:cdef", 9002,
                     &address2_);
  }

  MockQwaveApi api_;
  std::unique_ptr<DscpManager> dscp_manager_;

  IPEndPoint address1_;
  IPEndPoint address2_;
};

TEST_F(DscpManagerTest, PrepareForSendIsNoopIfNoSet) {
  RunUntilIdle();
  dscp_manager_->PrepareForSend(address1_);
}

TEST_F(DscpManagerTest, PrepareForSendCallsQwaveApisAfterSet) {
  RunUntilIdle();
  dscp_manager_->Set(DSCP_CS2);

  // AddSocketToFlow should be called for each address.
  // SetFlow should only be called when the flow is first created.
  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId1), Return(true)));
  EXPECT_CALL(api_, SetFlow(_, kFakeFlowId1, _, _, _, _, _));
  dscp_manager_->PrepareForSend(address1_);

  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId1), Return(true)));
  EXPECT_CALL(api_, SetFlow(_, _, _, _, _, _, _)).Times(0);
  dscp_manager_->PrepareForSend(address2_);

  // Called from DscpManager destructor.
  EXPECT_CALL(api_, RemoveSocketFromFlow(_, _, kFakeFlowId1, _));
  EXPECT_CALL(api_, CloseHandle(kFakeHandle1));
}

TEST_F(DscpManagerTest, PrepareForSendCallsQwaveApisOncePerAddress) {
  RunUntilIdle();
  dscp_manager_->Set(DSCP_CS2);

  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId1), Return(true)));
  EXPECT_CALL(api_, SetFlow(_, kFakeFlowId1, _, _, _, _, _));
  dscp_manager_->PrepareForSend(address1_);
  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(api_, SetFlow(_, _, _, _, _, _, _)).Times(0);
  dscp_manager_->PrepareForSend(address1_);

  // Called from DscpManager destructor.
  EXPECT_CALL(api_, RemoveSocketFromFlow(_, _, kFakeFlowId1, _));
  EXPECT_CALL(api_, CloseHandle(kFakeHandle1));
}

TEST_F(DscpManagerTest, SetDestroysExistingFlow) {
  RunUntilIdle();
  dscp_manager_->Set(DSCP_CS2);

  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId1), Return(true)));
  EXPECT_CALL(api_, SetFlow(_, kFakeFlowId1, _, _, _, _, _));
  dscp_manager_->PrepareForSend(address1_);

  // Calling Set should destroy the existing flow.
  // TODO(zstein): Verify that RemoveSocketFromFlow with no address
  // destroys the flow for all destinations.
  EXPECT_CALL(api_, RemoveSocketFromFlow(_, NULL, kFakeFlowId1, _));
  dscp_manager_->Set(DSCP_CS5);

  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId2), Return(true)));
  EXPECT_CALL(api_, SetFlow(_, kFakeFlowId2, _, _, _, _, _));
  dscp_manager_->PrepareForSend(address1_);

  // Called from DscpManager destructor.
  EXPECT_CALL(api_, RemoveSocketFromFlow(_, _, kFakeFlowId2, _));
  EXPECT_CALL(api_, CloseHandle(kFakeHandle1));
}

TEST_F(DscpManagerTest, SocketReAddedOnRecreateHandle) {
  RunUntilIdle();
  dscp_manager_->Set(DSCP_CS2);

  // First Set and Send work fine.
  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId1), Return(true)));
  EXPECT_CALL(api_, SetFlow(_, kFakeFlowId1, _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_THAT(dscp_manager_->PrepareForSend(address1_), IsOk());

  // Make Second flow operation fail (requires resetting the codepoint).
  EXPECT_CALL(api_, RemoveSocketFromFlow(_, _, kFakeFlowId1, _))
      .WillOnce(Return(true));
  dscp_manager_->Set(DSCP_CS7);

  auto error = std::make_unique<base::ScopedClearLastError>();
  ::SetLastError(ERROR_DEVICE_REINITIALIZATION_NEEDED);
  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, _, _, _)).WillOnce(Return(false));
  EXPECT_CALL(api_, SetFlow(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(api_, CloseHandle(kFakeHandle1));
  EXPECT_CALL(api_, CreateHandle(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(kFakeHandle2), Return(true)));
  EXPECT_EQ(ERR_INVALID_HANDLE, dscp_manager_->PrepareForSend(address1_));
  error = nullptr;
  RunUntilIdle();

  // Next Send should work fine, without requiring another Set
  EXPECT_CALL(api_, AddSocketToFlow(_, _, _, QOSTrafficTypeControl, _, _))
      .WillOnce(DoAll(SetArgPointee<5>(kFakeFlowId2), Return(true)));
  EXPECT_CALL(api_, SetFlow(_, kFakeFlowId2, _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_THAT(dscp_manager_->PrepareForSend(address1_), IsOk());

  // Called from DscpManager destructor.
  EXPECT_CALL(api_, RemoveSocketFromFlow(_, _, kFakeFlowId2, _));
  EXPECT_CALL(api_, CloseHandle(kFakeHandle2));
}
#endif

TEST_F(UDPSocketTest, ReadWithSocketOptimization) {
  std::string simple_message("hello world!");

  // Setup the server to listen.
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0 /* port */);
  UDPServerSocket server(nullptr, NetLogSource());
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(server_address), IsOk());
  // Get bound port.
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());

  // Setup the client, enable experimental optimization and connected to the
  // server.
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  client.EnableRecvOptimization();
  EXPECT_THAT(client.Connect(server_address), IsOk());

  // Get the client's address.
  IPEndPoint client_address;
  EXPECT_THAT(client.GetLocalAddress(&client_address), IsOk());

  // Server sends the message to the client.
  EXPECT_EQ(simple_message.length(),
            static_cast<size_t>(
                SendToSocket(&server, simple_message, client_address)));

  // Client receives the message.
  std::string str = ReadSocket(&client);
  EXPECT_EQ(simple_message, str);

  server.Close();
  client.Close();
}

// Tests that read from a socket correctly returns
// |ERR_MSG_TOO_BIG| when the buffer is too small and
// returns the actual message when it fits the buffer.
// For the optimized path, the buffer size should be at least
// 1 byte greater than the message.
TEST_F(UDPSocketTest, ReadWithSocketOptimizationTruncation) {
  std::string too_long_message(kMaxRead + 1, 'A');
  std::string right_length_message(kMaxRead - 1, 'B');
  std::string exact_length_message(kMaxRead, 'C');

  // Setup the server to listen.
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0 /* port */);
  UDPServerSocket server(nullptr, NetLogSource());
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(server_address), IsOk());
  // Get bound port.
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());

  // Setup the client, enable experimental optimization and connected to the
  // server.
  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  client.EnableRecvOptimization();
  EXPECT_THAT(client.Connect(server_address), IsOk());

  // Get the client's address.
  IPEndPoint client_address;
  EXPECT_THAT(client.GetLocalAddress(&client_address), IsOk());

  // Send messages to the client.
  EXPECT_EQ(too_long_message.length(),
            static_cast<size_t>(
                SendToSocket(&server, too_long_message, client_address)));
  EXPECT_EQ(right_length_message.length(),
            static_cast<size_t>(
                SendToSocket(&server, right_length_message, client_address)));
  EXPECT_EQ(exact_length_message.length(),
            static_cast<size_t>(
                SendToSocket(&server, exact_length_message, client_address)));

  // Client receives the messages.

  // 1. The first message is |too_long_message|. Its size exceeds the buffer.
  // In that case, the client is expected to get |ERR_MSG_TOO_BIG| when the
  // data is read.
  TestCompletionCallback callback;
  int rv = client.Read(buffer_.get(), kMaxRead, callback.callback());
  EXPECT_EQ(ERR_MSG_TOO_BIG, callback.GetResult(rv));
  EXPECT_EQ(client.GetLastTos().dscp, DSCP_DEFAULT);
  EXPECT_EQ(client.GetLastTos().ecn, ECN_DEFAULT);

  // 2. The second message is |right_length_message|. Its size is
  // one byte smaller than the size of the buffer. In that case, the client
  // is expected to read the whole message successfully.
  rv = client.Read(buffer_.get(), kMaxRead, callback.callback());
  rv = callback.GetResult(rv);
  EXPECT_EQ(static_cast<int>(right_length_message.length()), rv);
  EXPECT_EQ(right_length_message, std::string(buffer_->data(), rv));
  EXPECT_EQ(client.GetLastTos().dscp, DSCP_DEFAULT);
  EXPECT_EQ(client.GetLastTos().ecn, ECN_DEFAULT);

  // 3. The third message is |exact_length_message|. Its size is equal to
  // the read buffer size. In that case, the client expects to get
  // |ERR_MSG_TOO_BIG| when the socket is read. Internally, the optimized
  // path uses read() system call that requires one extra byte to detect
  // truncated messages; therefore, messages that fill the buffer exactly
  // are considered truncated.
  // The optimization is only enabled on POSIX platforms. On Windows,
  // the optimization is turned off; therefore, the client
  // should be able to read the whole message without encountering
  // |ERR_MSG_TOO_BIG|.
  rv = client.Read(buffer_.get(), kMaxRead, callback.callback());
  rv = callback.GetResult(rv);
  EXPECT_EQ(client.GetLastTos().dscp, DSCP_DEFAULT);
  EXPECT_EQ(client.GetLastTos().ecn, ECN_DEFAULT);
#if BUILDFLAG(IS_POSIX)
  EXPECT_EQ(ERR_MSG_TOO_BIG, rv);
#else
  EXPECT_EQ(static_cast<int>(exact_length_message.length()), rv);
  EXPECT_EQ(exact_length_message, std::string(buffer_->data(), rv));
#endif
  server.Close();
  client.Close();
}

// On Android, where socket tagging is supported, verify that UDPSocket::Tag
// works as expected.
#if BUILDFLAG(IS_ANDROID)
TEST_F(UDPSocketTest, Tag) {
  if (!CanGetTaggedBytes()) {
    DVLOG(0) << "Skipping test - GetTaggedBytes unsupported.";
    return;
  }

  UDPServerSocket server(nullptr, NetLogSource());
  ASSERT_THAT(server.Listen(IPEndPoint(IPAddress::IPv4Localhost(), 0)), IsOk());
  IPEndPoint server_address;
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());

  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  ASSERT_THAT(client.Connect(server_address), IsOk());

  // Verify UDP packets are tagged and counted properly.
  int32_t tag_val1 = 0x12345678;
  uint64_t old_traffic = GetTaggedBytes(tag_val1);
  SocketTag tag1(SocketTag::UNSET_UID, tag_val1);
  client.ApplySocketTag(tag1);
  // Client sends to the server.
  std::string simple_message("hello world!");
  int rv = WriteSocket(&client, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));
  // Server waits for message.
  std::string str = RecvFromSocket(&server);
  EXPECT_EQ(simple_message, str);
  // Server echoes reply.
  rv = SendToSocket(&server, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));
  // Client waits for response.
  str = ReadSocket(&client);
  EXPECT_EQ(simple_message, str);
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  int32_t tag_val2 = 0x87654321;
  old_traffic = GetTaggedBytes(tag_val2);
  SocketTag tag2(getuid(), tag_val2);
  client.ApplySocketTag(tag2);
  // Client sends to the server.
  rv = WriteSocket(&client, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));
  // Server waits for message.
  str = RecvFromSocket(&server);
  EXPECT_EQ(simple_message, str);
  // Server echoes reply.
  rv = SendToSocket(&server, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));
  // Client waits for response.
  str = ReadSocket(&client);
  EXPECT_EQ(simple_message, str);
  EXPECT_GT(GetTaggedBytes(tag_val2), old_traffic);

  // Verify socket can be retagged with a new value and the current process's
  // UID.
  old_traffic = GetTaggedBytes(tag_val1);
  client.ApplySocketTag(tag1);
  // Client sends to the server.
  rv = WriteSocket(&client, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));
  // Server waits for message.
  str = RecvFromSocket(&server);
  EXPECT_EQ(simple_message, str);
  // Server echoes reply.
  rv = SendToSocket(&server, simple_message);
  EXPECT_EQ(simple_message.length(), static_cast<size_t>(rv));
  // Client waits for response.
  str = ReadSocket(&client);
  EXPECT_EQ(simple_message, str);
  EXPECT_GT(GetTaggedBytes(tag_val1), old_traffic);
}

TEST_F(UDPSocketTest, BindToNetwork) {
  // The specific value of this address doesn't really matter, and no
  // server needs to be running here. The test only needs to call
  // Connect() and won't send any datagrams.
  const IPEndPoint fake_server_address(IPAddress::IPv4Localhost(), 8080);
  NetworkChangeNotifierFactoryAndroid ncn_factory;
  NetworkChangeNotifier::DisableForTest ncn_disable_for_test;
  std::unique_ptr<NetworkChangeNotifier> ncn(ncn_factory.CreateInstance());
  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    GTEST_SKIP() << "Network handles are required to test BindToNetwork.";

  // Binding the socket to a not existing network should fail at connect time.
  const handles::NetworkHandle wrong_network_handle = 65536;
  UDPClientSocket wrong_socket(DatagramSocket::RANDOM_BIND, nullptr,
                               NetLogSource(), wrong_network_handle);
  // Different Android versions might report different errors. Hence, just check
  // what shouldn't happen.
  int rv = wrong_socket.Connect(fake_server_address);
  EXPECT_NE(OK, rv);
  EXPECT_NE(ERR_NOT_IMPLEMENTED, rv);
  EXPECT_NE(wrong_network_handle, wrong_socket.GetBoundNetwork());

  // Binding the socket to an existing network should succeed.
  const handles::NetworkHandle network_handle =
      NetworkChangeNotifier::GetDefaultNetwork();
  if (network_handle != handles::kInvalidNetworkHandle) {
    UDPClientSocket correct_socket(DatagramSocket::RANDOM_BIND, nullptr,
                                   NetLogSource(), network_handle);
    EXPECT_EQ(OK, correct_socket.Connect(fake_server_address));
    EXPECT_EQ(network_handle, correct_socket.GetBoundNetwork());
  }
}

#endif  // BUILDFLAG(IS_ANDROID)

// Scoped helper to override the process-wide UDP socket limit.
class OverrideUDPSocketLimit {
 public:
  explicit OverrideUDPSocketLimit(int new_limit) {
    base::FieldTrialParams params;
    params[features::kLimitOpenUDPSocketsMax.name] =
        base::NumberToString(new_limit);

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kLimitOpenUDPSockets, params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that UDPClientSocket respects the global UDP socket limits.
TEST_F(UDPSocketTest, LimitClientSocket) {
  // Reduce the global UDP limit to 2.
  OverrideUDPSocketLimit set_limit(2);

  ASSERT_EQ(0, GetGlobalUDPSocketCountForTesting());

  auto socket1 = std::make_unique<UDPClientSocket>(DatagramSocket::DEFAULT_BIND,
                                                   nullptr, NetLogSource());
  auto socket2 = std::make_unique<UDPClientSocket>(DatagramSocket::DEFAULT_BIND,
                                                   nullptr, NetLogSource());

  // Simply constructing a UDPClientSocket does not increase the limit (no
  // Connect() or Bind() has been called yet).
  ASSERT_EQ(0, GetGlobalUDPSocketCountForTesting());

  // The specific value of this address doesn't really matter, and no server
  // needs to be running here. The test only needs to call Connect() and won't
  // send any datagrams.
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 8080);

  // Successful Connect() on socket1 increases socket count.
  EXPECT_THAT(socket1->Connect(server_address), IsOk());
  EXPECT_EQ(1, GetGlobalUDPSocketCountForTesting());

  // Successful Connect() on socket2 increases socket count.
  EXPECT_THAT(socket2->Connect(server_address), IsOk());
  EXPECT_EQ(2, GetGlobalUDPSocketCountForTesting());

  // Attempting a third Connect() should fail with ERR_INSUFFICIENT_RESOURCES,
  // as the limit is currently 2.
  auto socket3 = std::make_unique<UDPClientSocket>(DatagramSocket::DEFAULT_BIND,
                                                   nullptr, NetLogSource());
  EXPECT_THAT(socket3->Connect(server_address),
              IsError(ERR_INSUFFICIENT_RESOURCES));
  EXPECT_EQ(2, GetGlobalUDPSocketCountForTesting());

  // Check that explicitly closing socket2 free up a count.
  socket2->Close();
  EXPECT_EQ(1, GetGlobalUDPSocketCountForTesting());

  // Since the socket was already closed, deleting it will not affect the count.
  socket2.reset();
  EXPECT_EQ(1, GetGlobalUDPSocketCountForTesting());

  // Now that the count is below limit, try to connect another socket. This time
  // it will work.
  auto socket4 = std::make_unique<UDPClientSocket>(DatagramSocket::DEFAULT_BIND,
                                                   nullptr, NetLogSource());
  EXPECT_THAT(socket4->Connect(server_address), IsOk());
  EXPECT_EQ(2, GetGlobalUDPSocketCountForTesting());

  // Verify that closing the two remaining sockets brings the open count back to
  // 0.
  socket1.reset();
  EXPECT_EQ(1, GetGlobalUDPSocketCountForTesting());
  socket4.reset();
  EXPECT_EQ(0, GetGlobalUDPSocketCountForTesting());
}

// Tests that UDPSocketClient updates the global counter
// correctly when Connect() fails.
TEST_F(UDPSocketTest, LimitConnectFail) {
  ASSERT_EQ(0, GetGlobalUDPSocketCountForTesting());

  {
    // Simply allocating a UDPSocket does not increase count.
    UDPSocket socket(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
    EXPECT_EQ(0, GetGlobalUDPSocketCountForTesting());

    // Calling Open() allocates the socket and increases the global counter.
    EXPECT_THAT(socket.Open(ADDRESS_FAMILY_IPV4), IsOk());
    EXPECT_EQ(1, GetGlobalUDPSocketCountForTesting());

    // Connect to an IPv6 address should fail since the socket was created for
    // IPv4.
    EXPECT_THAT(socket.Connect(net::IPEndPoint(IPAddress::IPv6Localhost(), 53)),
                Not(IsOk()));

    // That Connect() failed doesn't change the global counter.
    EXPECT_EQ(1, GetGlobalUDPSocketCountForTesting());
  }

  // Finally, destroying UDPSocket decrements the global counter.
  EXPECT_EQ(0, GetGlobalUDPSocketCountForTesting());
}

// Tests allocating UDPClientSockets and Connect()ing them in parallel.
//
// This is primarily intended for coverage under TSAN, to check for races
// enforcing the global socket counter.
TEST_F(UDPSocketTest, LimitConnectMultithreaded) {
  ASSERT_EQ(0, GetGlobalUDPSocketCountForTesting());

  // Start up some threads.
  std::vector<std::unique_ptr<base::Thread>> threads;
  for (size_t i = 0; i < 5; ++i) {
    threads.push_back(std::make_unique<base::Thread>("Worker thread"));
    ASSERT_TRUE(threads.back()->Start());
  }

  // Post tasks to each of the threads.
  for (const auto& thread : threads) {
    thread->task_runner()->PostTask(
        FROM_HERE, base::BindOnce([] {
          // The specific value of this address doesn't really matter, and no
          // server needs to be running here. The test only needs to call
          // Connect() and won't send any datagrams.
          IPEndPoint server_address(IPAddress::IPv4Localhost(), 8080);

          UDPClientSocket socket(DatagramSocket::DEFAULT_BIND, nullptr,
                                 NetLogSource());
          EXPECT_THAT(socket.Connect(server_address), IsOk());
        }));
  }

  // Complete all the tasks.
  threads.clear();

  EXPECT_EQ(0, GetGlobalUDPSocketCountForTesting());
}

#if BUILDFLAG(ENABLE_MULTI_PACKETS_PER_CALL_QUIC_OPTIMIZATIONS)
// Clears the process-wide recvmmsg scratch buffer pool on construction and
// destruction so tests never inherit or leak pooled allocations.
class ScopedClearRecvmmsgBufferPool {
 public:
  ScopedClearRecvmmsgBufferPool() {
    Socket::ReadPacketResults::ClearBufferPoolForTesting();
  }
  ~ScopedClearRecvmmsgBufferPool() {
    Socket::ReadPacketResults::ClearBufferPoolForTesting();
  }
};

// Verifies the QUIC burst-receive journey when CobaltQuicReadBatch is enabled
// (for example during video segment downloads on 32-bit Android TV): a single
// ReadMultiplePackets call is capped at the default 16-packet batch so its
// scratch buffer fits in a 24 KiB PartitionAlloc slot span, and follow-up calls
// reusing the same ReadPacketResults drain remaining queued datagrams intact.
TEST_F(UDPSocketTest, ReadMultiplePacketsCapsBatchAt16AndDrainsRemaining) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kCobaltQuicReadBatch);
  ScopedClearRecvmmsgBufferPool clear_pool;
  constexpr int kPacketBufSize = 1473;
  constexpr int kExpectedMaxBatch = 16;
  constexpr int kNumPackets = 20;

  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  auto server =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  ASSERT_THAT(server->Listen(server_address), IsOk());
  ASSERT_THAT(server->GetLocalAddress(&server_address), IsOk());

  auto client = std::make_unique<UDPClientSocket>(
      DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  ASSERT_THAT(client->Connect(server_address), IsOk());
  ASSERT_THAT(client->SetReceiveBufferSize(64 * 1024), IsOk());

  IPEndPoint client_address;
  ASSERT_THAT(client->GetLocalAddress(&client_address), IsOk());

  for (int i = 0; i < kNumPackets; ++i) {
    std::string payload = "pkt-" + base::NumberToString(i);
    ASSERT_EQ(static_cast<int>(payload.size()),
              SendToSocket(server.get(), payload, client_address));
  }

  const size_t expected_total_buffer_size =
      static_cast<size_t>(kExpectedMaxBatch) *
      (sizeof(Socket::ReadPacketResult) + kPacketBufSize);
  std::vector<std::string> received;
  int read_calls = 0;
  // Reuse one ReadPacketResults across back-to-back calls to verify in-place
  // buffer reuse preserves packet pointers and drains all queued datagrams.
  Socket::ReadPacketResults results;
  const IOBufferWithSize* first_buffer = nullptr;
  while (static_cast<int>(received.size()) < kNumPackets) {
    TestCompletionCallback callback;
    int rv = client->ReadMultiplePackets(&results, kPacketBufSize,
                                         callback.callback());
    rv = callback.GetResult(rv);
    ASSERT_GT(rv, 0);
    EXPECT_LE(rv, kExpectedMaxBatch);
    ASSERT_NE(results.buffer, nullptr);
    if (!first_buffer) {
      first_buffer = results.buffer.get();
    } else {
      EXPECT_EQ(results.buffer.get(), first_buffer);
    }
    EXPECT_EQ(results.buffer->span().size(), expected_total_buffer_size);
    EXPECT_LE(results.buffer->span().size(), 24u * 1024u);
    EXPECT_EQ(results.result, rv);

    // SAFETY: |results.packets| points to |kExpectedMaxBatch| entries in
    // |results.buffer|, and |rv| is verified in (0, kExpectedMaxBatch].
    auto packets =
        UNSAFE_BUFFERS(base::span(results.packets, static_cast<size_t>(rv)));
    for (int i = 0; i < rv; ++i) {
      ASSERT_GT(packets[i].result, 0);
      received.emplace_back(packets[i].buffer,
                            static_cast<size_t>(packets[i].result));
    }
    ++read_calls;
  }

  EXPECT_GE(read_calls, 2);
  ASSERT_EQ(static_cast<int>(received.size()), kNumPackets);
  for (int i = 0; i < kNumPackets; ++i) {
    EXPECT_EQ(received[i], "pkt-" + base::NumberToString(i));
  }
}

// Verifies the idle QUIC connection memory-conservation journey (for example
// when multiple origin sockets sit idle while browsing YouTube TV): waiting
// for incoming datagrams (ERR_IO_PENDING) releases the scratch buffer back to
// the shared bounded pool so idle sockets hold zero bytes, and subsequent reads
// reuse the pooled allocation without getting poisoned by mismatched sizes.
TEST_F(UDPSocketTest, ReadMultiplePacketsReleasesBufferWhenIdleAndReusesPool) {
  ScopedClearRecvmmsgBufferPool clear_pool;
  constexpr int kPacketBufSize = 1473;
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 0u);

  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  auto server =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  ASSERT_THAT(server->Listen(server_address), IsOk());
  ASSERT_THAT(server->GetLocalAddress(&server_address), IsOk());

  auto client1 = std::make_unique<UDPClientSocket>(
      DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  auto client2 = std::make_unique<UDPClientSocket>(
      DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  ASSERT_THAT(client1->Connect(server_address), IsOk());
  ASSERT_THAT(client2->Connect(server_address), IsOk());

  IPEndPoint client1_address;
  IPEndPoint client2_address;
  ASSERT_THAT(client1->GetLocalAddress(&client1_address), IsOk());
  ASSERT_THAT(client2->GetLocalAddress(&client2_address), IsOk());

  // Both idle sockets return ERR_IO_PENDING with no pinned buffer, sharing the
  // single pooled buffer across both attempts.
  Socket::ReadPacketResults results1;
  TestCompletionCallback cb1;
  ASSERT_EQ(
      client1->ReadMultiplePackets(&results1, kPacketBufSize, cb1.callback()),
      ERR_IO_PENDING);
  EXPECT_EQ(results1.buffer, nullptr);
  EXPECT_EQ(results1.packets, nullptr);
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);

  Socket::ReadPacketResults results2;
  TestCompletionCallback cb2;
  ASSERT_EQ(
      client2->ReadMultiplePackets(&results2, kPacketBufSize, cb2.callback()),
      ERR_IO_PENDING);
  EXPECT_EQ(results2.buffer, nullptr);
  EXPECT_EQ(results2.packets, nullptr);
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);

  // Receiving on client1 checks out the pooled buffer; releasing it returns
  // the exact same allocation for client2's subsequent receive.
  ASSERT_EQ(5, SendToSocket(server.get(), "first", client1_address));
  ASSERT_EQ(cb1.WaitForResult(), 1);
  ASSERT_NE(results1.buffer, nullptr);
  const IOBufferWithSize* first_buffer = results1.buffer.get();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 0u);
  EXPECT_EQ(
      std::string(UNSAFE_TODO(results1.packets[0]).buffer,
                  static_cast<size_t>(UNSAFE_TODO(results1.packets[0]).result)),
      "first");
  results1.ReleaseBuffer();
  EXPECT_EQ(results1.buffer, nullptr);
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);

  ASSERT_EQ(6, SendToSocket(server.get(), "second", client2_address));
  ASSERT_EQ(cb2.WaitForResult(), 1);
  ASSERT_NE(results2.buffer, nullptr);
  EXPECT_EQ(results2.buffer.get(), first_buffer);
  EXPECT_EQ(
      std::string(UNSAFE_TODO(results2.packets[0]).buffer,
                  static_cast<size_t>(UNSAFE_TODO(results2.packets[0]).result)),
      "second");
  results2.ReleaseBuffer();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);

  // Verify the shared pool remains bounded at 2 cached buffers even if 3
  // concurrent readers acquire and release buffers simultaneously, reuses
  // entries in LIFO/LRU order with intact pre-initialized packet pointers,
  // evicts stale layouts when active-size buffers are returned, and never
  // collides on identical total_size with different (max_packets, buf_size).
  Socket::ReadPacketResults::ClearBufferPoolForTesting();
  Socket::ReadPacketResults r1;
  Socket::ReadPacketResults r2;
  Socket::ReadPacketResults r3;
  ASSERT_TRUE(r1.AcquireBuffer(16, kPacketBufSize));
  ASSERT_TRUE(r2.AcquireBuffer(16, kPacketBufSize));
  ASSERT_TRUE(r3.AcquireBuffer(16, kPacketBufSize));
  const IOBufferWithSize* r1_buf = r1.buffer.get();
  const IOBufferWithSize* r2_buf = r2.buffer.get();
  const IOBufferWithSize* r3_buf = r3.buffer.get();
  // Fast-path in-place reuse keeps the exact backing buffer and pointer layout.
  ASSERT_TRUE(r1.AcquireBuffer(16, kPacketBufSize));
  EXPECT_EQ(r1.buffer.get(), r1_buf);
  EXPECT_EQ(UNSAFE_TODO(r1.packets[1].buffer - r1.packets[0].buffer),
            kPacketBufSize);

  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 0u);
  r1.ReleaseBuffer();
  r2.ReleaseBuffer();
  // Plain LRU evicts r1 (oldest) so the pool holds [r2, r3].
  r3.ReleaseBuffer();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 2u);

  // LIFO reuse hands back the most recently released (cache-warm) buffer first.
  Socket::ReadPacketResults lifo1;
  Socket::ReadPacketResults lifo2;
  ASSERT_TRUE(lifo1.AcquireBuffer(16, kPacketBufSize));
  EXPECT_EQ(lifo1.buffer.get(), r3_buf);
  EXPECT_EQ(UNSAFE_TODO(lifo1.packets[15].buffer - lifo1.packets[0].buffer),
            15 * kPacketBufSize);
  ASSERT_TRUE(lifo2.AcquireBuffer(16, kPacketBufSize));
  EXPECT_EQ(lifo2.buffer.get(), r2_buf);
  lifo1.ReleaseBuffer();
  lifo2.ReleaseBuffer();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 2u);

  Socket::ReadPacketResults alt_size;
  ASSERT_TRUE(alt_size.AcquireBuffer(16, kPacketBufSize + 64));
  const IOBufferWithSize* alt_buf = alt_size.buffer.get();
  alt_size.ReleaseBuffer();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 2u);

  Socket::ReadPacketResults alt_reuse;
  ASSERT_TRUE(alt_reuse.AcquireBuffer(16, kPacketBufSize + 64));
  EXPECT_EQ(alt_reuse.buffer.get(), alt_buf);
  alt_reuse.ReleaseBuffer();

  // Returning two common-size buffers evicts the stale alternate layout rather
  // than keeping it pinned in the pool.
  Socket::ReadPacketResults common_a;
  Socket::ReadPacketResults common_b;
  ASSERT_TRUE(common_a.AcquireBuffer(16, kPacketBufSize));
  ASSERT_TRUE(common_b.AcquireBuffer(16, kPacketBufSize));
  const IOBufferWithSize* common_a_buf = common_a.buffer.get();
  const IOBufferWithSize* common_b_buf = common_b.buffer.get();
  common_a.ReleaseBuffer();
  common_b.ReleaseBuffer();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 2u);
  Socket::ReadPacketResults check_a;
  Socket::ReadPacketResults check_b;
  ASSERT_TRUE(check_a.AcquireBuffer(16, kPacketBufSize));
  ASSERT_TRUE(check_b.AcquireBuffer(16, kPacketBufSize));
  EXPECT_EQ(check_a.buffer.get(), common_b_buf);
  EXPECT_EQ(check_b.buffer.get(), common_a_buf);
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 0u);

  // Two distinct layouts producing identical total byte size must not share
  // pre-initialized packet pointer tables.
  Socket::ReadPacketResults::ClearBufferPoolForTesting();
  constexpr int kEntrySize = static_cast<int>(sizeof(Socket::ReadPacketResult));
  static_assert(kEntrySize % 2 == 0);
  constexpr int kLayoutABufSize = 2 * kEntrySize + 64;
  constexpr int kLayoutBBufSize = kEntrySize / 2 + 32;
  static_assert(2 * (kEntrySize + kLayoutABufSize) ==
                4 * (kEntrySize + kLayoutBBufSize));

  Socket::ReadPacketResults layout_a;
  ASSERT_TRUE(layout_a.AcquireBuffer(2, kLayoutABufSize));
  const IOBufferWithSize* layout_a_buf = layout_a.buffer.get();
  EXPECT_EQ(
      UNSAFE_TODO(layout_a.packets[1].buffer - layout_a.packets[0].buffer),
      kLayoutABufSize);
  layout_a.ReleaseBuffer();

  Socket::ReadPacketResults layout_b;
  ASSERT_TRUE(layout_b.AcquireBuffer(4, kLayoutBBufSize));
  EXPECT_NE(layout_b.buffer.get(), layout_a_buf);
  EXPECT_EQ(
      UNSAFE_TODO(layout_b.packets[1].buffer - layout_b.packets[0].buffer),
      kLayoutBBufSize);
  EXPECT_EQ(
      UNSAFE_TODO(layout_b.packets[3].buffer - layout_b.packets[0].buffer),
      3 * kLayoutBBufSize);

  // In-place re-acquisition with identical total_size but a different
  // packet_buffer_size must swap buffers rather than reusing stale pointers.
  ASSERT_TRUE(layout_b.AcquireBuffer(2, kLayoutABufSize));
  EXPECT_EQ(layout_b.buffer.get(), layout_a_buf);
  EXPECT_EQ(
      UNSAFE_TODO(layout_b.packets[1].buffer - layout_b.packets[0].buffer),
      kLayoutABufSize);

  // Verify the 256 KiB total byte cap: a single oversized buffer (> 256 KiB)
  // is not cached, and two ~154 KiB buffers cannot coexist in the pool even
  // though the slot cap is 2.
  Socket::ReadPacketResults::ClearBufferPoolForTesting();
  Socket::ReadPacketResults huge;
  ASSERT_TRUE(huge.AcquireBuffer(64, 5000));
  huge.ReleaseBuffer();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 0u);

  Socket::ReadPacketResults med1;
  Socket::ReadPacketResults med2;
  ASSERT_TRUE(med1.AcquireBuffer(16, 9600));
  ASSERT_TRUE(med2.AcquireBuffer(16, 9600));
  const IOBufferWithSize* med2_buf = med2.buffer.get();
  med1.ReleaseBuffer();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);
  med2.ReleaseBuffer();
  EXPECT_EQ(Socket::ReadPacketResults::GetBufferPoolSizeForTesting(), 1u);
  Socket::ReadPacketResults med_check;
  ASSERT_TRUE(med_check.AcquireBuffer(16, 9600));
  EXPECT_EQ(med_check.buffer.get(), med2_buf);
}

// Verifies closing a QUIC UDP socket while waiting on an asynchronous
// multi-packet read (for example during connection teardown or network switch)
// clears pending socket state, releases the results pointer so reopening the
// socket for a single-packet Read() works cleanly, and never fires a stale
// completion callback.
TEST_F(UDPSocketTest, ReadMultiplePacketsCloseWhilePendingReleasesState) {
  ScopedClearRecvmmsgBufferPool clear_pool;
  constexpr int kPacketBufSize = 1473;

  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  auto server =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  ASSERT_THAT(server->Listen(server_address), IsOk());
  ASSERT_THAT(server->GetLocalAddress(&server_address), IsOk());

  UDPSocket client(DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  ASSERT_THAT(client.Open(ADDRESS_FAMILY_IPV4), IsOk());
  ASSERT_THAT(client.Connect(server_address), IsOk());

  IPEndPoint client_address;
  ASSERT_THAT(client.GetLocalAddress(&client_address), IsOk());

  TestCompletionCallback pending_cb;
  {
    Socket::ReadPacketResults results;
    ASSERT_EQ(client.ReadMultiplePackets(&results, kPacketBufSize,
                                         pending_cb.callback()),
              ERR_IO_PENDING);
    EXPECT_EQ(results.buffer, nullptr);

    client.Close();
    SendToSocket(server.get(), "late-packet", client_address);
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(pending_cb.have_result());
    EXPECT_EQ(results.buffer, nullptr);
  }  // Destroys |results| before reopening |client|.

  // Reopen the socket and complete an async single-packet RecvFrom(): if
  // Close() failed to clear |results_|, ReadWatcher would dispatch
  // DidCompleteMultiplePacketRead() into the destroyed |results|.
  ASSERT_THAT(client.Open(ADDRESS_FAMILY_IPV4), IsOk());
  ASSERT_THAT(client.Bind(IPEndPoint(IPAddress::IPv4Localhost(), 0)), IsOk());
  IPEndPoint reopened_address;
  ASSERT_THAT(client.GetLocalAddress(&reopened_address), IsOk());

  auto read_buf = base::MakeRefCounted<IOBufferWithSize>(kPacketBufSize);
  IPEndPoint from_address;
  TestCompletionCallback read_cb;
  ASSERT_EQ(client.RecvFrom(read_buf.get(), read_buf->size(), &from_address,
                            read_cb.callback()),
            ERR_IO_PENDING);
  ASSERT_EQ(5, SendToSocket(server.get(), "after", reopened_address));
  ASSERT_EQ(read_cb.WaitForResult(), 5);
  EXPECT_EQ(std::string(read_buf->data(), 5), "after");
}

// Verifies Finch control and defensive bounds for CobaltQuicReadBatch: when
// disabled, baseline keeps 64-packet batches (still pooled); when enabled,
// CobaltQuicReadBatch_packets sets the recvmmsg cap clamped to [1, 64]; and
// invalid or overflowing buffer sizes return ERR_INVALID_ARGUMENT cleanly.
TEST_F(UDPSocketTest, ReadMultiplePacketsRespectsFinchBatchParamAndClamps) {
  ScopedClearRecvmmsgBufferPool clear_pool;
  constexpr int kPacketBufSize = 1473;

  auto measure_batch_buffer = [&](int expected_max_packets, int send_count) {
    Socket::ReadPacketResults::ClearBufferPoolForTesting();
    IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
    auto server =
        std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
    ASSERT_THAT(server->Listen(server_address), IsOk());
    ASSERT_THAT(server->GetLocalAddress(&server_address), IsOk());

    auto client = std::make_unique<UDPClientSocket>(
        DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
    ASSERT_THAT(client->Connect(server_address), IsOk());
    ASSERT_THAT(client->SetReceiveBufferSize(64 * 1024), IsOk());

    IPEndPoint client_address;
    ASSERT_THAT(client->GetLocalAddress(&client_address), IsOk());

    for (int i = 0; i < send_count; ++i) {
      std::string payload = "p" + base::NumberToString(i);
      ASSERT_EQ(static_cast<int>(payload.size()),
                SendToSocket(server.get(), payload, client_address));
    }

    Socket::ReadPacketResults results;
    TestCompletionCallback cb;
    int rv =
        client->ReadMultiplePackets(&results, kPacketBufSize, cb.callback());
    rv = cb.GetResult(rv);
    ASSERT_GT(rv, 0);
    EXPECT_LE(rv, expected_max_packets);
    ASSERT_NE(results.buffer, nullptr);
    const size_t expected_bytes =
        static_cast<size_t>(expected_max_packets) *
        (sizeof(Socket::ReadPacketResult) + kPacketBufSize);
    EXPECT_EQ(results.buffer->span().size(), expected_bytes);
  };

  // Disabled by default: preserves 64-packet baseline batch size.
  EXPECT_FALSE(base::FeatureList::IsEnabled(features::kCobaltQuicReadBatch));
  measure_batch_buffer(64, 5);
  // Enabled with custom batch size 4: caps a 10-packet burst at 4 packets.
  {
    base::test::ScopedFeatureList custom;
    custom.InitAndEnableFeatureWithParameters(
        features::kCobaltQuicReadBatch, {{"CobaltQuicReadBatch_packets", "4"}});
    measure_batch_buffer(4, 10);
  }
  // Out-of-range low and negative params clamp to 1.
  {
    base::test::ScopedFeatureList clamp_low;
    clamp_low.InitAndEnableFeatureWithParameters(
        features::kCobaltQuicReadBatch, {{"CobaltQuicReadBatch_packets", "0"}});
    measure_batch_buffer(1, 3);
  }
  {
    base::test::ScopedFeatureList clamp_negative;
    clamp_negative.InitAndEnableFeatureWithParameters(
        features::kCobaltQuicReadBatch,
        {{"CobaltQuicReadBatch_packets", "-5"}});
    measure_batch_buffer(1, 3);
  }
  // Malformed param falls back to the 16-packet default.
  {
    base::test::ScopedFeatureList malformed;
    malformed.InitAndEnableFeatureWithParameters(
        features::kCobaltQuicReadBatch,
        {{"CobaltQuicReadBatch_packets", "not-a-number"}});
    measure_batch_buffer(16, 3);
  }
  // Out-of-range high param clamps to 64.
  {
    base::test::ScopedFeatureList clamp_high;
    clamp_high.InitAndEnableFeatureWithParameters(
        features::kCobaltQuicReadBatch,
        {{"CobaltQuicReadBatch_packets", "128"}});
    measure_batch_buffer(64, 3);
  }

  // Non-positive or overflowing buffer dimensions fail cleanly without abort.
  Socket::ReadPacketResults bad_results;
  EXPECT_FALSE(bad_results.AcquireBuffer(0, kPacketBufSize));
  EXPECT_FALSE(bad_results.AcquireBuffer(16, 0));
  EXPECT_FALSE(bad_results.AcquireBuffer(16, std::numeric_limits<int>::max()));
  EXPECT_EQ(bad_results.buffer, nullptr);

  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  auto server =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  ASSERT_THAT(server->Listen(server_address), IsOk());
  ASSERT_THAT(server->GetLocalAddress(&server_address), IsOk());
  auto client = std::make_unique<UDPClientSocket>(
      DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  ASSERT_THAT(client->Connect(server_address), IsOk());

  TestCompletionCallback err_cb;
  EXPECT_EQ(client->ReadMultiplePackets(&bad_results, 0, err_cb.callback()),
            ERR_INVALID_ARGUMENT);
  EXPECT_EQ(
      client->ReadMultiplePackets(&bad_results, std::numeric_limits<int>::max(),
                                  err_cb.callback()),
      ERR_INVALID_ARGUMENT);
}

// Verifies the non-QUIC UDP socket journey (such as DNS, DIAL, or P2P
// sockets): creating, connecting, and performing single-packet reads does not
// query or activate the CobaltQuicReadBatch field trial; only the first
// ReadMultiplePackets() call resolves and activates it, and Close() resets the
// cached batch size before a socket is reopened.
TEST_F(UDPSocketTest, CobaltQuicReadBatchResolvesLazilyAndResetsOnClose) {
  ScopedClearRecvmmsgBufferPool clear_pool;
  constexpr int kPacketBufSize = 1473;
  constexpr char kTrialName[] = "CobaltQuicReadBatchLazyTestTrial";

  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitFromCommandLine(
      "CobaltQuicReadBatch<CobaltQuicReadBatchLazyTestTrial.Enabled:"
      "CobaltQuicReadBatch_packets/4",
      "");
  ASSERT_TRUE(base::FieldTrialList::TrialExists(kTrialName));
  ASSERT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  auto server =
      std::make_unique<UDPServerSocket>(NetLog::Get(), NetLogSource());
  ASSERT_THAT(server->Listen(server_address), IsOk());
  ASSERT_THAT(server->GetLocalAddress(&server_address), IsOk());

  UDPSocket client(DatagramSocket::DEFAULT_BIND, NetLog::Get(), NetLogSource());
  ASSERT_THAT(client.Open(ADDRESS_FAMILY_IPV4), IsOk());
  ASSERT_THAT(client.Connect(server_address), IsOk());
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

  // Single-packet Read() must not activate CobaltQuicReadBatch.
  IPEndPoint client_address;
  ASSERT_THAT(client.GetLocalAddress(&client_address), IsOk());
  ASSERT_EQ(4, SendToSocket(server.get(), "ping", client_address));
  auto buf = base::MakeRefCounted<IOBufferWithSize>(64);
  TestCompletionCallback single_cb;
  int rv = client.Read(buf.get(), buf->size(), single_cb.callback());
  ASSERT_EQ(single_cb.GetResult(rv), 4);
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(kTrialName));

  // First ReadMultiplePackets() activates the trial and uses batch size 4.
  ASSERT_EQ(5, SendToSocket(server.get(), "multi", client_address));
  Socket::ReadPacketResults results;
  TestCompletionCallback multi_cb;
  rv =
      client.ReadMultiplePackets(&results, kPacketBufSize, multi_cb.callback());
  ASSERT_EQ(multi_cb.GetResult(rv), 1);
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(kTrialName));
  EXPECT_EQ(results.buffer->span().size(),
            4u * (sizeof(Socket::ReadPacketResult) + kPacketBufSize));

  // Close() resets the cached batch size so reopening picks up a new config.
  results.ReleaseBuffer();
  client.Close();
  {
    base::test::ScopedFeatureList override_8;
    override_8.InitAndEnableFeatureWithParameters(
        features::kCobaltQuicReadBatch, {{"CobaltQuicReadBatch_packets", "8"}});
    ASSERT_THAT(client.Open(ADDRESS_FAMILY_IPV4), IsOk());
    ASSERT_THAT(client.Bind(IPEndPoint(IPAddress::IPv4Localhost(), 0)), IsOk());
    IPEndPoint reopened_address;
    ASSERT_THAT(client.GetLocalAddress(&reopened_address), IsOk());
    ASSERT_EQ(6, SendToSocket(server.get(), "reopen", reopened_address));
    TestCompletionCallback reopen_cb;
    rv = client.ReadMultiplePackets(&results, kPacketBufSize,
                                    reopen_cb.callback());
    ASSERT_EQ(reopen_cb.GetResult(rv), 1);
    EXPECT_EQ(results.buffer->span().size(),
              8u * (sizeof(Socket::ReadPacketResult) + kPacketBufSize));
  }
}

namespace {

struct ExpectedBatchPacket {
  std::string payload;
  DiffServCodePoint dscp;
  EcnCodePoint ecn;
};

void VerifyBatchDscpAndEcn(UDPServerSocket* server,
                           UDPClientSocket* client,
                           const IPEndPoint& client_address,
                           int max_read,
                           const std::vector<ExpectedBatchPacket>& batch,
                           Socket::ReadPacketResults* results) {
  for (const auto& expected : batch) {
    ASSERT_EQ(server->SetTos(expected.dscp, expected.ecn), OK);
    auto io_buffer = base::MakeRefCounted<StringIOBuffer>(expected.payload);
    TestCompletionCallback send_callback;
    int send_rv = server->SendTo(io_buffer.get(), io_buffer->size(),
                                 client_address, send_callback.callback());
    ASSERT_EQ(send_callback.GetResult(send_rv),
              static_cast<int>(expected.payload.size()));
  }

  size_t received = 0;
  while (received < batch.size()) {
    TestCompletionCallback read_callback;
    int rv = client->ReadMultiplePackets(results, max_read,
                                         read_callback.callback());
    rv = read_callback.GetResult(rv);
    ASSERT_GT(rv, 0);
    auto packet_span =
        UNSAFE_BUFFERS(base::span(results->packets, static_cast<size_t>(rv)));
    for (const auto& packet : packet_span) {
      ASSERT_LT(received, batch.size());
      ASSERT_GT(packet.result, 0);
      std::string payload(packet.buffer, static_cast<size_t>(packet.result));
      EXPECT_EQ(payload, batch[received].payload);
      DscpAndEcn actual_tos = TosToDscpAndEcn(packet.tos);
      EXPECT_EQ(actual_tos.dscp, batch[received].dscp);
      EXPECT_EQ(actual_tos.ecn, batch[received].ecn);
      ++received;
    }
  }

  EXPECT_EQ(client->GetLastTos().dscp, batch.back().dscp);
  EXPECT_EQ(client->GetLastTos().ecn, batch.back().ecn);
}

void RunReadMultiplePacketsDscpAndEcnTest(const IPAddress& localhost,
                                          int max_read) {
  ScopedClearRecvmmsgBufferPool clear_pool;
  IPEndPoint server_address(localhost, 0);
  UDPServerSocket server(nullptr, NetLogSource());
  server.AllowAddressReuse();
  int listen_rv = server.Listen(server_address);
  if (localhost.IsIPv6() && (listen_rv == ERR_ADDRESS_INVALID ||
                             listen_rv == ERR_ADDRESS_UNREACHABLE)) {
    GTEST_SKIP() << "IPv6 loopback is not available.";
  }
  ASSERT_THAT(listen_rv, IsOk());
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());

  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  ASSERT_THAT(client.Connect(server_address), IsOk());
  ASSERT_EQ(client.SetRecvTos(), OK);
  IPEndPoint client_address;
  ASSERT_THAT(client.GetLocalAddress(&client_address), IsOk());

  Socket::ReadPacketResults results;
  // Send a mixed-ECN burst so recvmmsg returns packets carrying distinct
  // IP_TOS / IPV6_TCLASS control messages in a single ReadPacketResults buffer.
  const std::vector<ExpectedBatchPacket> mixed_batch = {
      {"pkt-not-ect", DSCP_DEFAULT, ECN_NOT_ECT},
      {"pkt-ect1", DSCP_AF41, ECN_ECT1},
      {"pkt-ect0", DSCP_CS2, ECN_ECT0},
      {"pkt-ce", DSCP_CS2, ECN_CE},
  };
  VerifyBatchDscpAndEcn(&server, &client, client_address, max_read, mixed_batch,
                        &results);

  // Reuse the same ReadPacketResults slots with unmarked packets to confirm
  // stale TOS values from the prior batch are cleared per packet.
  const std::vector<ExpectedBatchPacket> unmarked_batch = {
      {"pkt-reset-0", DSCP_DEFAULT, ECN_NOT_ECT},
      {"pkt-reset-1", DSCP_DEFAULT, ECN_NOT_ECT},
  };
  VerifyBatchDscpAndEcn(&server, &client, client_address, max_read,
                        unmarked_batch, &results);

  server.Close();
  client.Close();
}

}  // namespace

// User journey: A Cobalt client receiving IPv4 QUIC packets in recvmmsg batches
// must observe each packet's router-stamped DSCP/ECN bits accurately and clear
// stale TOS state when receive slots are reused across bursts.
TEST_F(UDPSocketTest, ReadMultiplePacketsVerifyDscpAndEcnV4) {
  RunReadMultiplePacketsDscpAndEcnTest(IPAddress::IPv4Localhost(), kMaxRead);
}

// User journey: A Cobalt client receiving IPv6 QUIC packets in recvmmsg batches
// must observe each packet's IPv6 Traffic Class DSCP/ECN bits accurately and
// clear stale TOS state when receive slots are reused across bursts.
TEST_F(UDPSocketTest, ReadMultiplePacketsVerifyDscpAndEcnV6) {
  RunReadMultiplePacketsDscpAndEcnTest(IPAddress::IPv6Localhost(), kMaxRead);
}

// User journey: A Cobalt client reading batched UDP packets without enabling
// SetRecvTos() must still deliver payloads cleanly with default DSCP/Not-ECT
// metadata rather than uninitialized control-buffer bytes.
TEST_F(UDPSocketTest, ReadMultiplePacketsDefaultsTosWithoutSetRecvTos) {
  ScopedClearRecvmmsgBufferPool clear_pool;
  IPEndPoint server_address(IPAddress::IPv4Localhost(), 0);
  UDPServerSocket server(nullptr, NetLogSource());
  server.AllowAddressReuse();
  ASSERT_THAT(server.Listen(server_address), IsOk());
  ASSERT_THAT(server.GetLocalAddress(&server_address), IsOk());

  UDPClientSocket client(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource());
  ASSERT_THAT(client.Connect(server_address), IsOk());
  IPEndPoint client_address;
  ASSERT_THAT(client.GetLocalAddress(&client_address), IsOk());

  ASSERT_EQ(server.SetTos(DSCP_AF41, ECN_CE), OK);
  const std::vector<std::string> payloads = {"untracked-0", "untracked-1"};
  for (const auto& payload : payloads) {
    ASSERT_EQ(SendToSocket(&server, payload, client_address),
              static_cast<int>(payload.size()));
  }

  Socket::ReadPacketResults results;
  size_t received = 0;
  while (received < payloads.size()) {
    TestCompletionCallback read_callback;
    int rv = client.ReadMultiplePackets(&results, kMaxRead,
                                        read_callback.callback());
    rv = read_callback.GetResult(rv);
    ASSERT_GT(rv, 0);
    auto packet_span =
        UNSAFE_BUFFERS(base::span(results.packets, static_cast<size_t>(rv)));
    for (const auto& packet : packet_span) {
      ASSERT_LT(received, payloads.size());
      ASSERT_GT(packet.result, 0);
      EXPECT_EQ(std::string(packet.buffer, static_cast<size_t>(packet.result)),
                payloads[received]);
      DscpAndEcn actual_tos = TosToDscpAndEcn(packet.tos);
      EXPECT_EQ(actual_tos.dscp, DSCP_DEFAULT);
      EXPECT_EQ(actual_tos.ecn, ECN_NOT_ECT);
      ++received;
    }
  }

  EXPECT_EQ(client.GetLastTos().dscp, DSCP_DEFAULT);
  EXPECT_EQ(client.GetLastTos().ecn, ECN_NOT_ECT);

  server.Close();
  client.Close();
}
#endif  // BUILDFLAG(ENABLE_MULTI_PACKETS_PER_CALL_QUIC_OPTIMIZATIONS)

}  // namespace net
