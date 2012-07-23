// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_tracker_linux.h"

#include <vector>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace internal {

void Noop() {}

class AddressTrackerLinuxTest : public testing::Test {
 protected:
  AddressTrackerLinuxTest() : tracker_(base::Bind(&Noop)) {}

  bool HandleMessage(char* buf, size_t length) {
    return tracker_.HandleMessage(buf, length);
  }

  AddressTrackerLinux::AddressMap GetAddressMap() {
    return tracker_.GetAddressMap();
  }

  AddressTrackerLinux tracker_;
};

namespace {

typedef std::vector<char> Buffer;

class NetlinkMessage {
 public:
  explicit NetlinkMessage(uint16 type) : buffer_(NLMSG_HDRLEN) {
    header()->nlmsg_type = type;
    Align();
  }

  void AddPayload(const void* data, size_t length) {
    CHECK_EQ(static_cast<size_t>(NLMSG_HDRLEN),
             buffer_.size()) << "Payload must be added first";
    Append(data, length);
    Align();
  }

  void AddAttribute(uint16 type, const void* data, size_t length) {
    struct nlattr attr;
    attr.nla_len = NLA_HDRLEN + length;
    attr.nla_type = type;
    Append(&attr, sizeof(attr));
    Align();
    Append(data, length);
    Align();
  }

  void AppendTo(Buffer* output) const {
    CHECK_EQ(NLMSG_ALIGN(output->size()), output->size());
    output->reserve(output->size() + NLMSG_LENGTH(buffer_.size()));
    output->insert(output->end(), buffer_.begin(), buffer_.end());
  }

 private:
  void Append(const void* data, size_t length) {
    const char* chardata = reinterpret_cast<const char*>(data);
    buffer_.insert(buffer_.end(), chardata, chardata + length);
  }

  void Align() {
    header()->nlmsg_len = buffer_.size();
    buffer_.insert(buffer_.end(), NLMSG_ALIGN(buffer_.size()) - buffer_.size(),
                   0);
    CHECK(NLMSG_OK(header(), buffer_.size()));
  }

  struct nlmsghdr* header() {
    return reinterpret_cast<struct nlmsghdr*>(&buffer_[0]);
  }

  Buffer buffer_;
};

void MakeMessage(uint16 type,
                 uint8 flags,
                 uint8 family,
                 const IPAddressNumber& address,
                 const IPAddressNumber& local,
                 Buffer* output) {
  NetlinkMessage nlmsg(type);
  struct ifaddrmsg msg = {};
  msg.ifa_family = family;
  msg.ifa_flags = flags;
  nlmsg.AddPayload(&msg, sizeof(msg));
  if (address.size())
    nlmsg.AddAttribute(IFA_ADDRESS, &address[0], address.size());
  if (local.size())
    nlmsg.AddAttribute(IFA_LOCAL, &local[0], local.size());
  nlmsg.AppendTo(output);
}

const unsigned char kAddress0[] = { 127, 0, 0, 1 };
const unsigned char kAddress1[] = { 10, 0, 0, 1 };
const unsigned char kAddress2[] = { 192, 168, 0, 1 };
const unsigned char kAddress3[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 1 };
const unsigned char kAddress4[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255,
                                    169, 254, 0, 1 };

TEST_F(AddressTrackerLinuxTest, NewAddress) {
  const IPAddressNumber kEmpty;
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));
  const IPAddressNumber kAddr1(kAddress1, kAddress1 + arraysize(kAddress1));
  const IPAddressNumber kAddr2(kAddress2, kAddress2 + arraysize(kAddress2));
  const IPAddressNumber kAddr3(kAddress3, kAddress3 + arraysize(kAddress3));

  Buffer buffer;
  MakeMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_TRUE(map.find(kAddr0) != map.end());
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  buffer.clear();
  MakeMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kAddr1, kAddr2, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  map = GetAddressMap();
  EXPECT_EQ(2u, map.size());
  EXPECT_TRUE(map.find(kAddr0) != map.end());
  EXPECT_TRUE(map.find(kAddr2) != map.end());
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr2].ifa_flags);

  buffer.clear();
  MakeMessage(RTM_NEWADDR, 0, AF_INET6, kEmpty, kAddr3, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  map = GetAddressMap();
  EXPECT_EQ(3u, map.size());
  EXPECT_TRUE(map.find(kAddr3) != map.end());
}

TEST_F(AddressTrackerLinuxTest, NewAddressChange) {
  const IPAddressNumber kEmpty;
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));

  Buffer buffer;
  MakeMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_TRUE(map.find(kAddr0) != map.end());
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  buffer.clear();
  MakeMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_TRUE(map.find(kAddr0) != map.end());
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr0].ifa_flags);

  // Both messages in one buffer.
  buffer.clear();
  MakeMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kAddr0, kEmpty, &buffer);
  MakeMessage(RTM_NEWADDR, IFA_F_HOMEADDRESS, AF_INET, kAddr0, kEmpty, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_HOMEADDRESS, map[kAddr0].ifa_flags);
}

TEST_F(AddressTrackerLinuxTest, NewAddressDuplicate) {
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));

  Buffer buffer;
  MakeMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET, kAddr0, kAddr0, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_TRUE(map.find(kAddr0) != map.end());
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);

  EXPECT_FALSE(HandleMessage(&buffer[0], buffer.size()));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(IFA_F_TEMPORARY, map[kAddr0].ifa_flags);
}

TEST_F(AddressTrackerLinuxTest, DeleteAddress) {
  const IPAddressNumber kEmpty;
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));
  const IPAddressNumber kAddr1(kAddress1, kAddress1 + arraysize(kAddress1));
  const IPAddressNumber kAddr2(kAddress2, kAddress2 + arraysize(kAddress2));

  Buffer buffer;
  MakeMessage(RTM_NEWADDR, 0, AF_INET, kAddr0, kEmpty, &buffer);
  MakeMessage(RTM_NEWADDR, 0, AF_INET, kAddr1, kAddr2, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  AddressTrackerLinux::AddressMap map = GetAddressMap();
  EXPECT_EQ(2u, map.size());

  buffer.clear();
  MakeMessage(RTM_DELADDR, 0, AF_INET, kEmpty, kAddr0, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());
  EXPECT_TRUE(map.find(kAddr0) == map.end());
  EXPECT_TRUE(map.find(kAddr2) != map.end());

  buffer.clear();
  MakeMessage(RTM_DELADDR, 0, AF_INET, kAddr2, kAddr1, &buffer);
  // kAddr1 does not exist in the map.
  EXPECT_FALSE(HandleMessage(&buffer[0], buffer.size()));
  map = GetAddressMap();
  EXPECT_EQ(1u, map.size());

  buffer.clear();
  MakeMessage(RTM_DELADDR, 0, AF_INET, kAddr2, kEmpty, &buffer);
  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  map = GetAddressMap();
  EXPECT_EQ(0u, map.size());
}

TEST_F(AddressTrackerLinuxTest, IgnoredMessage) {
  const IPAddressNumber kEmpty;
  const IPAddressNumber kAddr0(kAddress0, kAddress0 + arraysize(kAddress0));
  const IPAddressNumber kAddr3(kAddress3, kAddress3 + arraysize(kAddress3));

  Buffer buffer;
  // Ignored family.
  MakeMessage(RTM_NEWADDR, 0, AF_UNSPEC, kAddr3, kAddr0, &buffer);
  // No address.
  MakeMessage(RTM_NEWADDR, 0, AF_INET, kEmpty, kEmpty, &buffer);
  // Ignored type.
  MakeMessage(RTM_DELROUTE, 0, AF_INET6, kAddr3, kEmpty, &buffer);
  EXPECT_FALSE(HandleMessage(&buffer[0], buffer.size()));
  EXPECT_EQ(0u, GetAddressMap().size());

  // Valid message after ignored messages.
  NetlinkMessage nlmsg(RTM_NEWADDR);
  struct ifaddrmsg msg = {};
  msg.ifa_family = AF_INET;
  nlmsg.AddPayload(&msg, sizeof(msg));
  // Ignored attribute.
  struct ifa_cacheinfo cache_info = {};
  nlmsg.AddAttribute(IFA_CACHEINFO, &cache_info, sizeof(cache_info));
  nlmsg.AddAttribute(IFA_ADDRESS, &kAddr0[0], kAddr0.size());
  nlmsg.AppendTo(&buffer);

  EXPECT_TRUE(HandleMessage(&buffer[0], buffer.size()));
  EXPECT_EQ(1u, GetAddressMap().size());
}

}  // namespace

}  // namespace internal
}  // namespace net
