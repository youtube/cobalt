// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_connector.h"

#include <functional>
#include <utility>

#include "ipcz/driver_transport.h"
#include "ipcz/node.h"
#include "ipcz/node_link.h"
#include "ipcz/node_messages.h"
#include "ipcz/portal.h"
#include "ipcz/router.h"
#include "reference_drivers/sync_reference_driver.h"
#include "test/test.h"
#include "test/test_transport_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

const IpczDriver& kDriver = reference_drivers::kSyncReferenceDriver;

class NodeConnectorTest : public test::Test {
 protected:
  Ref<Node> CreateBrokerNode() {
    return MakeRefCounted<Node>(Node::Type::kBroker, kDriver,
                                IPCZ_INVALID_DRIVER_HANDLE);
  }

  Ref<Node> CreateNonBrokerNode() {
    return MakeRefCounted<Node>(Node::Type::kNormal, kDriver,
                                IPCZ_INVALID_DRIVER_HANDLE);
  }

  Ref<Portal> CreatePortal(Ref<Node> node) {
    return MakeRefCounted<Portal>(node, MakeRefCounted<Router>());
  }

  DriverTransport::Pair CreateTransports() {
    IpczDriverHandle handle0, handle1;
    EXPECT_EQ(IPCZ_RESULT_OK,
              kDriver.CreateTransports(
                  IPCZ_INVALID_DRIVER_HANDLE, IPCZ_INVALID_DRIVER_HANDLE,
                  IPCZ_NO_FLAGS, nullptr, &handle0, &handle1));
    auto transport0 =
        MakeRefCounted<DriverTransport>(DriverObject(kDriver, handle0));
    auto transport1 =
        MakeRefCounted<DriverTransport>(DriverObject(kDriver, handle1));
    return {transport0, transport1};
  }
};

TEST_F(NodeConnectorTest, ConnectBrokerToNonBroker) {
  Ref<Node> broker = CreateBrokerNode();
  Ref<Node> non_broker = CreateNonBrokerNode();

  auto [broker_transport, non_broker_transport] = CreateTransports();

  bool non_broker_received_connect = false;
  test::TestTransportListener listener(non_broker_transport);
  listener.OnMessage<msg::ConnectFromBrokerToNonBroker>([&](auto& connect) {
    EXPECT_EQ(broker->GetAssignedName(), connect.params().broker_name);
    EXPECT_EQ(1u, connect.params().num_initial_portals);
    non_broker_received_connect = true;
    return true;
  });

  // Initiate connection from the broker side. The non-broker's transport should
  // receive an appropriate connection message.
  EXPECT_FALSE(non_broker_received_connect);
  std::vector<Ref<Portal>> initial_portals = {CreatePortal(broker)};
  NodeConnector::ConnectNode(broker, std::move(broker_transport), IPCZ_NO_FLAGS,
                             initial_portals);
  EXPECT_TRUE(non_broker_received_connect);
}

TEST_F(NodeConnectorTest, ConnectNonBrokerToBroker) {
  Ref<Node> broker = CreateBrokerNode();
  Ref<Node> non_broker = CreateNonBrokerNode();

  auto [broker_transport, non_broker_transport] = CreateTransports();

  bool broker_received_connect = false;
  test::TestTransportListener listener(broker_transport);
  listener.OnMessage<msg::ConnectFromNonBrokerToBroker>([&](auto& connect) {
    EXPECT_EQ(1u, connect.params().num_initial_portals);
    broker_received_connect = true;
    return true;
  });

  // Initiate connection from the non-broker side. The broker's transport should
  // receive an appropriate connection message.
  EXPECT_FALSE(broker_received_connect);
  std::vector<Ref<Portal>> initial_portals = {CreatePortal(non_broker)};
  NodeConnector::ConnectNode(non_broker, std::move(non_broker_transport),
                             IPCZ_CONNECT_NODE_TO_BROKER, initial_portals);
  EXPECT_TRUE(broker_received_connect);

  broker->Close();
  non_broker->Close();
}

TEST_F(NodeConnectorTest, BrokerRejectInvalidMessage) {
  Ref<Node> broker = CreateBrokerNode();
  Ref<Node> non_broker = CreateNonBrokerNode();

  {
    auto [broker_transport, non_broker_transport] = CreateTransports();

    bool rejected = false;
    std::vector<Ref<Portal>> initial_portals = {CreatePortal(broker)};
    NodeConnector::ConnectNode(broker, std::move(broker_transport),
                               IPCZ_NO_FLAGS, initial_portals,
                               [&](Ref<NodeLink> link) { rejected = !link; });

    // Make sure we receive and deserialize the message so no driver objects are
    // leaked.
    bool received_connect = false;
    test::TestTransportListener listener(non_broker_transport);
    listener.OnMessage<msg::ConnectFromBrokerToNonBroker>([&](auto&) {
      received_connect = true;
      return true;
    });
    EXPECT_TRUE(received_connect);

    // Now try sending a non-connection message, which should be rejected by the
    // NodeConnector.
    EXPECT_FALSE(rejected);
    msg::RouteClosed message;
    non_broker_transport->Transmit(message);
    EXPECT_TRUE(rejected);
  }

  {
    auto [broker_transport, non_broker_transport] = CreateTransports();

    bool rejected = false;
    std::vector<Ref<Portal>> initial_portals = {CreatePortal(broker)};
    NodeConnector::ConnectNode(broker, std::move(broker_transport),
                               IPCZ_NO_FLAGS, initial_portals,
                               [&](Ref<NodeLink> link) { rejected = !link; });

    // Make sure we receive and deserialize the message so no driver objects are
    // leaked.
    bool received_connect = false;
    test::TestTransportListener listener(non_broker_transport);
    listener.OnMessage<msg::ConnectFromBrokerToNonBroker>([&](auto&) {
      received_connect = true;
      return true;
    });
    EXPECT_TRUE(received_connect);

    // The recipient here is a broker node, but we're sending it a message that
    // is only expected to be sent from a broker to a non-broker. Its
    // NodeConnector should reject this message.
    EXPECT_FALSE(rejected);
    msg::ConnectFromBrokerToNonBroker message;
    message.params().buffer = message.AppendDriverObject(
        DriverMemory(kDriver, 64).TakeDriverObject());
    non_broker_transport->Transmit(message);
    EXPECT_TRUE(rejected);
  }

  broker->Close();
  non_broker->Close();
}

TEST_F(NodeConnectorTest, NonBrokerRejectInvalidMessage) {
  Ref<Node> broker = CreateBrokerNode();
  Ref<Node> non_broker = CreateNonBrokerNode();

  {
    auto [broker_transport, non_broker_transport] = CreateTransports();

    bool rejected = false;
    std::vector<Ref<Portal>> initial_portals = {CreatePortal(non_broker)};
    NodeConnector::ConnectNode(non_broker, std::move(non_broker_transport),
                               IPCZ_CONNECT_NODE_TO_BROKER, initial_portals,
                               [&](Ref<NodeLink> link) { rejected = !link; });

    // Try sending a non-connection message, which should be rejected by the
    // NodeConnector.
    EXPECT_FALSE(rejected);
    msg::RouteClosed message;
    broker_transport->Transmit(message);
    EXPECT_TRUE(rejected);
  }

  {
    auto [broker_transport, non_broker_transport] = CreateTransports();

    bool rejected = false;
    std::vector<Ref<Portal>> initial_portals = {CreatePortal(non_broker)};
    NodeConnector::ConnectNode(non_broker, std::move(non_broker_transport),
                               IPCZ_CONNECT_NODE_TO_BROKER, initial_portals,
                               [&](Ref<NodeLink> link) { rejected = !link; });

    // For good measure, also try sending an inappropriate connection message,
    // which should have the same result as above. This is a message which
    // should only be accepted by broker nodes from a transport expected to link
    // to a non-broker node. Since the receiving NodeConnector is for a
    // non-broker, it must reject this message.
    EXPECT_FALSE(rejected);
    msg::ConnectFromNonBrokerToBroker message;
    broker_transport->Transmit(message);
    EXPECT_TRUE(rejected);
  }

  broker->Close();
  non_broker->Close();
}

TEST_F(NodeConnectorTest, EndToEndSuccess_BrokerFirst) {
  Ref<Node> broker = CreateBrokerNode();
  Ref<Node> non_broker = CreateNonBrokerNode();
  auto [broker_transport, non_broker_transport] = CreateTransports();

  std::vector<Ref<Portal>> initial_broker_portals = {CreatePortal(broker)};
  Ref<NodeLink> broker_link;
  NodeConnector::ConnectNode(broker, std::move(broker_transport), IPCZ_NO_FLAGS,
                             initial_broker_portals, [&](Ref<NodeLink> link) {
                               broker_link = std::move(link);
                             });

  std::vector<Ref<Portal>> initial_non_broker_portals = {
      CreatePortal(non_broker)};
  Ref<NodeLink> non_broker_link;
  NodeConnector::ConnectNode(
      non_broker, std::move(non_broker_transport), IPCZ_CONNECT_NODE_TO_BROKER,
      initial_non_broker_portals,
      [&](Ref<NodeLink> link) { non_broker_link = std::move(link); });

  EXPECT_TRUE(broker_link);
  EXPECT_TRUE(non_broker_link);

  // Verify that sublink 0 on both NodeLinks corresponds to the appropriate
  // initial portals.
  EXPECT_EQ(initial_broker_portals[0]->router(),
            broker_link->GetRouter(SublinkId{0}));
  EXPECT_EQ(initial_non_broker_portals[0]->router(),
            non_broker_link->GetRouter(SublinkId{0}));

  initial_broker_portals[0]->Close();
  initial_non_broker_portals[0]->Close();
  broker->Close();
  non_broker->Close();
}

TEST_F(NodeConnectorTest, EndToEndSuccess_NonBrokerFirst) {
  Ref<Node> broker = CreateBrokerNode();
  Ref<Node> non_broker = CreateNonBrokerNode();
  auto [broker_transport, non_broker_transport] = CreateTransports();

  std::vector<Ref<Portal>> initial_non_broker_portals = {
      CreatePortal(non_broker)};
  Ref<NodeLink> non_broker_link;
  NodeConnector::ConnectNode(
      non_broker, std::move(non_broker_transport), IPCZ_CONNECT_NODE_TO_BROKER,
      initial_non_broker_portals,
      [&](Ref<NodeLink> link) { non_broker_link = std::move(link); });

  std::vector<Ref<Portal>> initial_broker_portals = {CreatePortal(broker)};
  Ref<NodeLink> broker_link;
  NodeConnector::ConnectNode(broker, std::move(broker_transport), IPCZ_NO_FLAGS,
                             initial_broker_portals, [&](Ref<NodeLink> link) {
                               broker_link = std::move(link);
                             });

  EXPECT_TRUE(broker_link);
  EXPECT_TRUE(non_broker_link);

  // Verify that sublink 0 on both NodeLinks corresponds to the appropriate
  // initial portals.
  EXPECT_EQ(initial_broker_portals[0]->router(),
            broker_link->GetRouter(SublinkId{0}));
  EXPECT_EQ(initial_non_broker_portals[0]->router(),
            non_broker_link->GetRouter(SublinkId{0}));

  initial_broker_portals[0]->Close();
  initial_non_broker_portals[0]->Close();
  broker->Close();
  non_broker->Close();
}

TEST_F(NodeConnectorTest, MultipleInitialPortals) {
  Ref<Node> broker = CreateBrokerNode();
  Ref<Node> non_broker = CreateNonBrokerNode();
  auto [broker_transport, non_broker_transport] = CreateTransports();

  // We establish multiple initial portals on connection, with a surplus on the
  // broker side to test that behavior as well.
  constexpr size_t kNumBrokerPortals = 5;
  constexpr size_t kNumNonBrokerPortals = 3;
  static_assert(kNumBrokerPortals > kNumNonBrokerPortals,
                "Test requires more broker portals than non-broker portals");

  std::vector<Ref<Portal>> initial_broker_portals(kNumBrokerPortals);
  for (auto& portal : initial_broker_portals) {
    portal = CreatePortal(broker);
  }
  std::vector<Ref<Portal>> initial_non_broker_portals(kNumNonBrokerPortals);
  for (auto& portal : initial_non_broker_portals) {
    portal = CreatePortal(broker);
  }

  Ref<NodeLink> broker_link;
  NodeConnector::ConnectNode(broker, std::move(broker_transport), IPCZ_NO_FLAGS,
                             initial_broker_portals, [&](Ref<NodeLink> link) {
                               broker_link = std::move(link);
                             });

  Ref<NodeLink> non_broker_link;
  NodeConnector::ConnectNode(
      non_broker, std::move(non_broker_transport), IPCZ_CONNECT_NODE_TO_BROKER,
      initial_non_broker_portals,
      [&](Ref<NodeLink> link) { non_broker_link = std::move(link); });

  EXPECT_TRUE(broker_link);
  EXPECT_TRUE(non_broker_link);

  // Verify that the first sublinks of both NodeLinks correspond to appropriate
  // initial portals. Any surplus sublinks on the broker's NodeLink should also
  // correspond to an appropriate initial portal on the broker side, but they
  // should see that their peer is closed since the non-broker established a
  // smaller set of initial portals.
  ASSERT_LE(initial_non_broker_portals.size(), initial_broker_portals.size());
  const size_t kNumConnectedPortals = kNumNonBrokerPortals;
  const size_t kNumDisconnectedPortals =
      kNumBrokerPortals - kNumConnectedPortals;
  for (size_t i = 0; i < kNumConnectedPortals; ++i) {
    EXPECT_EQ(initial_broker_portals[i]->router(),
              broker_link->GetRouter(SublinkId{i}));
    EXPECT_EQ(initial_non_broker_portals[i]->router(),
              non_broker_link->GetRouter(SublinkId{i}));
    initial_broker_portals[i]->Close();
    initial_non_broker_portals[i]->Close();
  }

  for (size_t i = kNumConnectedPortals; i < kNumDisconnectedPortals; ++i) {
    IpczPortalStatus status;
    initial_broker_portals[i]->router()->QueryStatus(status);
    EXPECT_TRUE((status.flags & IPCZ_PORTAL_STATUS_PEER_CLOSED) != 0);
  }

  broker->Close();
  non_broker->Close();
}

}  // namespace
}  // namespace ipcz
