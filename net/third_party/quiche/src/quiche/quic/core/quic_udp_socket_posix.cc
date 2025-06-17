// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_types.h"
#if defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)
// This must be defined before including any system headers.
#define __APPLE_USE_RFC_3542
#endif  // defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "absl/base/optimization.h"
#include "quiche/quic/core/io/socket.h"
#include "quiche/quic/core/quic_udp_socket.h"
#include "quiche/quic/platform/api/quic_bug_tracker.h"
#include "quiche/quic/platform/api/quic_flag_utils.h"
#include "quiche/quic/platform/api/quic_udp_socket_platform_api.h"

#if defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)
#error "__APPLE_USE_RFC_3542 needs to be defined."
#endif

#if defined(__linux__)
#include <alloca.h>
// For SO_TIMESTAMPING.
#include <linux/net_tstamp.h>
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#define QUIC_UDP_SOCKET_SUPPORT_TTL 1
#endif

namespace quic {
namespace {

// Explicit Congestion Notification is the last two bits of the TOS byte.
constexpr uint8_t kEcnMask = 0x03;

#if defined(__linux__) && (!defined(__ANDROID_API__) || __ANDROID_API__ >= 21)
#define QUIC_UDP_SOCKET_SUPPORT_LINUX_TIMESTAMPING 1
// This is the structure that SO_TIMESTAMPING fills into the cmsg header.
// It is well-defined, but does not have a definition in a public header.
// See https://www.kernel.org/doc/Documentation/networking/timestamping.txt
// for more information.
struct LinuxSoTimestamping {
  // The converted system time of the timestamp.
  struct timespec systime;
  // Deprecated; serves only as padding.
  struct timespec hwtimetrans;
  // The raw hardware timestamp.
  struct timespec hwtimeraw;
};
const size_t kCmsgSpaceForRecvTimestamp =
    CMSG_SPACE(sizeof(LinuxSoTimestamping));
#else
const size_t kCmsgSpaceForRecvTimestamp = 0;
#endif

const size_t kMinCmsgSpaceForRead =
    CMSG_SPACE(sizeof(uint32_t))       // Dropped packet count
    + CMSG_SPACE(sizeof(in_pktinfo))   // V4 Self IP
    + CMSG_SPACE(sizeof(in6_pktinfo))  // V6 Self IP
    + kCmsgSpaceForRecvTimestamp + CMSG_SPACE(sizeof(int))  // TTL
    + kCmsgSpaceForGooglePacketHeader;

void SetV4SelfIpInControlMessage(const QuicIpAddress& self_address,
                                 cmsghdr* cmsg) {
  QUICHE_DCHECK(self_address.IsIPv4());
  in_pktinfo* pktinfo = reinterpret_cast<in_pktinfo*>(CMSG_DATA(cmsg));
  memset(pktinfo, 0, sizeof(in_pktinfo));
  pktinfo->ipi_ifindex = 0;
  std::string address_string = self_address.ToPackedString();
  memcpy(&pktinfo->ipi_spec_dst, address_string.c_str(),
         address_string.length());
}

void SetV6SelfIpInControlMessage(const QuicIpAddress& self_address,
                                 cmsghdr* cmsg) {
  QUICHE_DCHECK(self_address.IsIPv6());
  in6_pktinfo* pktinfo = reinterpret_cast<in6_pktinfo*>(CMSG_DATA(cmsg));
  memset(pktinfo, 0, sizeof(in6_pktinfo));
  std::string address_string = self_address.ToPackedString();
  memcpy(&pktinfo->ipi6_addr, address_string.c_str(), address_string.length());
}

void PopulatePacketInfoFromControlMessage(struct cmsghdr* cmsg,
                                          QuicUdpPacketInfo* packet_info,
                                          BitMask64 packet_info_interested) {
#ifdef SOL_UDP
  if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::IS_GRO) &&
      cmsg->cmsg_level == SOL_UDP && cmsg->cmsg_type == UDP_GRO) {
    packet_info->set_gso_size(*reinterpret_cast<uint16_t*>(CMSG_DATA(cmsg)));
  }
#endif

#if defined(__linux__) && defined(SO_RXQ_OVFL)
  if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_RXQ_OVFL) {
    if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::DROPPED_PACKETS)) {
      packet_info->SetDroppedPackets(
          *(reinterpret_cast<uint32_t*> CMSG_DATA(cmsg)));
    }
    return;
  }
#endif

#if defined(QUIC_UDP_SOCKET_SUPPORT_LINUX_TIMESTAMPING)
  if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPING) {
    if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::RECV_TIMESTAMP)) {
      LinuxSoTimestamping* linux_ts =
          reinterpret_cast<LinuxSoTimestamping*>(CMSG_DATA(cmsg));
      timespec* ts = &linux_ts->systime;
      int64_t usec = (static_cast<int64_t>(ts->tv_sec) * 1000 * 1000) +
                     (static_cast<int64_t>(ts->tv_nsec) / 1000);
      packet_info->SetReceiveTimestamp(
          QuicWallTime::FromUNIXMicroseconds(usec));
    }
    return;
  }
#endif

  if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
    if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::V6_SELF_IP)) {
      const in6_pktinfo* info = reinterpret_cast<in6_pktinfo*>(CMSG_DATA(cmsg));
      const char* addr_data = reinterpret_cast<const char*>(&info->ipi6_addr);
      int addr_len = sizeof(in6_addr);
      QuicIpAddress self_v6_ip;
      if (self_v6_ip.FromPackedString(addr_data, addr_len)) {
        packet_info->SetSelfV6Ip(self_v6_ip);
      } else {
        QUIC_BUG(quic_bug_10751_1) << "QuicIpAddress::FromPackedString failed";
      }
    }
    return;
  }

  if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
    if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::V4_SELF_IP)) {
      const in_pktinfo* info = reinterpret_cast<in_pktinfo*>(CMSG_DATA(cmsg));
      const char* addr_data = reinterpret_cast<const char*>(&info->ipi_addr);
      int addr_len = sizeof(in_addr);
      QuicIpAddress self_v4_ip;
      if (self_v4_ip.FromPackedString(addr_data, addr_len)) {
        packet_info->SetSelfV4Ip(self_v4_ip);
      } else {
        QUIC_BUG(quic_bug_10751_2) << "QuicIpAddress::FromPackedString failed";
      }
    }
    return;
  }

  if ((cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TTL) ||
      (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_HOPLIMIT)) {
    if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::TTL)) {
      packet_info->SetTtl(*(reinterpret_cast<int*>(CMSG_DATA(cmsg))));
    }
    return;
  }

  if ((cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TOS) ||
      (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_TCLASS)) {
    if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::ECN)) {
      packet_info->SetEcnCodepoint(QuicEcnCodepoint(
          *(reinterpret_cast<uint8_t*>(CMSG_DATA(cmsg))) & kEcnMask));
    }
  }

  if (packet_info_interested.IsSet(
          QuicUdpPacketInfoBit::GOOGLE_PACKET_HEADER)) {
    BufferSpan google_packet_headers;
    if (GetGooglePacketHeadersFromControlMessage(
            cmsg, &google_packet_headers.buffer,
            &google_packet_headers.buffer_len)) {
      packet_info->SetGooglePacketHeaders(google_packet_headers);
    }
  }
}

bool NextCmsg(msghdr* hdr, char* control_buffer, size_t control_buffer_len,
              int cmsg_level, int cmsg_type, size_t data_size,
              cmsghdr** cmsg /*in, out*/) {
  // msg_controllen needs to be increased first, otherwise CMSG_NXTHDR will
  // return nullptr.
  hdr->msg_controllen += CMSG_SPACE(data_size);
  if (hdr->msg_controllen > control_buffer_len) {
    return false;
  }

  if ((*cmsg) == nullptr) {
    QUICHE_DCHECK_EQ(nullptr, hdr->msg_control);
    memset(control_buffer, 0, control_buffer_len);
    hdr->msg_control = control_buffer;
    (*cmsg) = CMSG_FIRSTHDR(hdr);
  } else {
    QUICHE_DCHECK_NE(nullptr, hdr->msg_control);
    (*cmsg) = CMSG_NXTHDR(hdr, (*cmsg));
  }

  if (nullptr == (*cmsg)) {
    return false;
  }

  (*cmsg)->cmsg_len = CMSG_LEN(data_size);
  (*cmsg)->cmsg_level = cmsg_level;
  (*cmsg)->cmsg_type = cmsg_type;

  return true;
}
}  // namespace

QuicUdpSocketFd QuicUdpSocketApi::Create(int address_family,
                                         int receive_buffer_size,
                                         int send_buffer_size, bool ipv6_only) {
  // QUICHE_DCHECK here so the program exits early(before reading packets) in
  // debug mode. This should have been a static_assert, however it can't be done
  // on ios/osx because CMSG_SPACE isn't a constant expression there.
  QUICHE_DCHECK_GE(kDefaultUdpPacketControlBufferSize, kMinCmsgSpaceForRead);

  absl::StatusOr<SocketFd> socket = socket_api::CreateSocket(
      quiche::FromPlatformAddressFamily(address_family),
      socket_api::SocketProtocol::kUdp,
      /*blocking=*/false);

  if (!socket.ok()) {
    QUIC_LOG_FIRST_N(ERROR, 100)
        << "UDP non-blocking socket creation for address_family="
        << address_family << " failed: " << socket.status();
    return kQuicInvalidSocketFd;
  }

  SetGoogleSocketOptions(socket.value());

  if (!SetupSocket(socket.value(), address_family, receive_buffer_size,
                   send_buffer_size, ipv6_only)) {
    Destroy(socket.value());
    return kQuicInvalidSocketFd;
  }

  return socket.value();
}

bool QuicUdpSocketApi::SetupSocket(QuicUdpSocketFd fd, int address_family,
                                   int receive_buffer_size,
                                   int send_buffer_size, bool ipv6_only) {
  // Receive buffer size.
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &receive_buffer_size,
                 sizeof(receive_buffer_size)) != 0) {
    QUIC_LOG_FIRST_N(ERROR, 100) << "Failed to set socket recv size";
    return false;
  }

  // Send buffer size.
  if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size,
                 sizeof(send_buffer_size)) != 0) {
    QUIC_LOG_FIRST_N(ERROR, 100) << "Failed to set socket send size";
    return false;
  }

  if (GetQuicRestartFlag(quic_quiche_ecn_sockets)) {
    QUIC_RESTART_FLAG_COUNT(quic_quiche_ecn_sockets);
    unsigned int set = 1;
    if (address_family == AF_INET &&
        setsockopt(fd, IPPROTO_IP, IP_RECVTOS, &set, sizeof(set)) != 0) {
      QUIC_LOG_FIRST_N(ERROR, 100) << "Failed to request to receive ECN on "
                                   << "socket";
      return false;
    }
    if (address_family == AF_INET6 &&
        setsockopt(fd, IPPROTO_IPV6, IPV6_RECVTCLASS, &set, sizeof(set)) != 0) {
      QUIC_LOG_FIRST_N(ERROR, 100) << "Failed to request to receive ECN on "
                                   << "socket";
      return false;
    }
  }

  if (!(address_family == AF_INET6 && ipv6_only)) {
    if (!EnableReceiveSelfIpAddressForV4(fd)) {
      QUIC_LOG_FIRST_N(ERROR, 100)
          << "Failed to enable receiving of self v4 ip";
      return false;
    }
  }

  if (address_family == AF_INET6) {
    if (!EnableReceiveSelfIpAddressForV6(fd)) {
      QUIC_LOG_FIRST_N(ERROR, 100)
          << "Failed to enable receiving of self v6 ip";
      return false;
    }
  }

  return true;
}

void QuicUdpSocketApi::Destroy(QuicUdpSocketFd fd) {
  if (fd != kQuicInvalidSocketFd) {
    absl::Status result = socket_api::Close(fd);
    if (!result.ok()) {
      QUIC_LOG_FIRST_N(WARNING, 100)
          << "Failed to close UDP socket with error " << result;
    }
  }
}

bool QuicUdpSocketApi::Bind(QuicUdpSocketFd fd, QuicSocketAddress address) {
  sockaddr_storage addr = address.generic_address();
  int addr_len =
      address.host().IsIPv4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
  return 0 == bind(fd, reinterpret_cast<sockaddr*>(&addr), addr_len);
}

bool QuicUdpSocketApi::BindInterface(QuicUdpSocketFd fd,
                                     const std::string& interface_name) {
#if defined(__linux__) && !defined(__ANDROID_API__)
  if (interface_name.empty() || interface_name.size() >= IFNAMSIZ) {
    QUIC_BUG(udp_bad_interface_name)
        << "interface_name must be nonempty and shorter than " << IFNAMSIZ;
    return false;
  }

  return 0 == setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE,
                         interface_name.c_str(), interface_name.length());
#else
  (void)fd;
  (void)interface_name;
  QUIC_BUG(interface_bind_not_implemented)
      << "Interface binding is not implemented on this platform";
  return false;
#endif
}

bool QuicUdpSocketApi::EnableDroppedPacketCount(QuicUdpSocketFd fd) {
#if defined(__linux__) && defined(SO_RXQ_OVFL)
  int get_overflow = 1;
  return 0 == setsockopt(fd, SOL_SOCKET, SO_RXQ_OVFL, &get_overflow,
                         sizeof(get_overflow));
#else
  (void)fd;
  return false;
#endif
}

bool QuicUdpSocketApi::EnableReceiveSelfIpAddressForV4(QuicUdpSocketFd fd) {
  int get_self_ip = 1;
  return 0 == setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &get_self_ip,
                         sizeof(get_self_ip));
}

bool QuicUdpSocketApi::EnableReceiveSelfIpAddressForV6(QuicUdpSocketFd fd) {
  int get_self_ip = 1;
  return 0 == setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &get_self_ip,
                         sizeof(get_self_ip));
}

bool QuicUdpSocketApi::EnableReceiveTimestamp(QuicUdpSocketFd fd) {
#if defined(__linux__) && (!defined(__ANDROID_API__) || __ANDROID_API__ >= 21)
  int timestamping = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE;
  return 0 == setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &timestamping,
                         sizeof(timestamping));
#else
  (void)fd;
  return false;
#endif
}

bool QuicUdpSocketApi::EnableReceiveTtlForV4(QuicUdpSocketFd fd) {
#if defined(QUIC_UDP_SOCKET_SUPPORT_TTL)
  int get_ttl = 1;
  return 0 == setsockopt(fd, IPPROTO_IP, IP_RECVTTL, &get_ttl, sizeof(get_ttl));
#else
  (void)fd;
  return false;
#endif
}

bool QuicUdpSocketApi::EnableReceiveTtlForV6(QuicUdpSocketFd fd) {
#if defined(QUIC_UDP_SOCKET_SUPPORT_TTL)
  int get_ttl = 1;
  return 0 == setsockopt(fd, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &get_ttl,
                         sizeof(get_ttl));
#else
  (void)fd;
  return false;
#endif
}

bool QuicUdpSocketApi::WaitUntilReadable(QuicUdpSocketFd fd,
                                         QuicTime::Delta timeout) {
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);

  timeval select_timeout;
  select_timeout.tv_sec = timeout.ToSeconds();
  select_timeout.tv_usec = timeout.ToMicroseconds() % 1000000;

  return 1 == select(1 + fd, &read_fds, nullptr, nullptr, &select_timeout);
}

void QuicUdpSocketApi::ReadPacket(QuicUdpSocketFd fd,
                                  BitMask64 packet_info_interested,
                                  ReadPacketResult* result) {
  result->ok = false;
  BufferSpan& packet_buffer = result->packet_buffer;
  BufferSpan& control_buffer = result->control_buffer;
  QuicUdpPacketInfo* packet_info = &result->packet_info;

  QUICHE_DCHECK_GE(control_buffer.buffer_len, kMinCmsgSpaceForRead);

  struct iovec iov = {packet_buffer.buffer, packet_buffer.buffer_len};
  struct sockaddr_storage raw_peer_address;

  if (control_buffer.buffer_len > 0) {
    reinterpret_cast<struct cmsghdr*>(control_buffer.buffer)->cmsg_len =
        control_buffer.buffer_len;
  }

  msghdr hdr;
  hdr.msg_name = &raw_peer_address;
  hdr.msg_namelen = sizeof(raw_peer_address);
  hdr.msg_iov = &iov;
  hdr.msg_iovlen = 1;
  hdr.msg_flags = 0;
  hdr.msg_control = control_buffer.buffer;
  hdr.msg_controllen = control_buffer.buffer_len;

#if defined(__linux__)
  // If MSG_TRUNC is set on Linux, recvmsg will return the real packet size even
  // if |packet_buffer| is too small to receive it.
  int flags = MSG_TRUNC;
#else
  int flags = 0;
#endif

  int bytes_read = recvmsg(fd, &hdr, flags);
  if (bytes_read < 0) {
    const int error_num = errno;
    if (error_num != EAGAIN) {
      QUIC_LOG_FIRST_N(ERROR, 100)
          << "Error reading packet: " << strerror(error_num);
    }
    return;
  }

  if (ABSL_PREDICT_FALSE(hdr.msg_flags & MSG_CTRUNC)) {
    QUIC_BUG(quic_bug_10751_3)
        << "Control buffer too small. size:" << control_buffer.buffer_len;
    return;
  }

  if (ABSL_PREDICT_FALSE(hdr.msg_flags & MSG_TRUNC) ||
      // Normally "bytes_read > packet_buffer.buffer_len" implies the MSG_TRUNC
      // bit is set, but it is not the case if tested with config=android_arm64.
      static_cast<size_t>(bytes_read) > packet_buffer.buffer_len) {
    QUIC_LOG_FIRST_N(WARNING, 100)
        << "Received truncated QUIC packet: buffer size:"
        << packet_buffer.buffer_len << " packet size:" << bytes_read;
    return;
  }

  packet_buffer.buffer_len = bytes_read;
  if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::PEER_ADDRESS)) {
    packet_info->SetPeerAddress(QuicSocketAddress(raw_peer_address));
  }

  if (hdr.msg_controllen > 0) {
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&hdr); cmsg != nullptr;
         cmsg = CMSG_NXTHDR(&hdr, cmsg)) {
      BitMask64 prior_bitmask = packet_info->bitmask();
      PopulatePacketInfoFromControlMessage(cmsg, packet_info,
                                           packet_info_interested);
      if (packet_info->bitmask() == prior_bitmask) {
        QUIC_DLOG(INFO) << "Ignored cmsg_level:" << cmsg->cmsg_level
                        << ", cmsg_type:" << cmsg->cmsg_type;
      }
    }
  }

  result->ok = true;
}

size_t QuicUdpSocketApi::ReadMultiplePackets(QuicUdpSocketFd fd,
                                             BitMask64 packet_info_interested,
                                             ReadPacketResults* results) {
#if defined(__linux__) && !defined(__ANDROID__)
  if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::IS_GRO)) {
    size_t num_packets = 0;
    for (ReadPacketResult& result : *results) {
      result.ok = false;
    }
    for (ReadPacketResult& result : *results) {
      ReadPacket(fd, packet_info_interested, &result);
      if (!result.ok) {
        break;
      }
      ++num_packets;
    }
    return num_packets;
  } else {
    // Use recvmmsg.
    size_t hdrs_size = sizeof(mmsghdr) * results->size();
    mmsghdr* hdrs = static_cast<mmsghdr*>(alloca(hdrs_size));
    memset(hdrs, 0, hdrs_size);

    struct TempPerPacketData {
      iovec iov;
      sockaddr_storage raw_peer_address;
    };
    TempPerPacketData* packet_data_array = static_cast<TempPerPacketData*>(
        alloca(sizeof(TempPerPacketData) * results->size()));

    for (size_t i = 0; i < results->size(); ++i) {
      (*results)[i].ok = false;

      msghdr* hdr = &hdrs[i].msg_hdr;
      TempPerPacketData* packet_data = &packet_data_array[i];
      packet_data->iov.iov_base = (*results)[i].packet_buffer.buffer;
      packet_data->iov.iov_len = (*results)[i].packet_buffer.buffer_len;

      hdr->msg_name = &packet_data->raw_peer_address;
      hdr->msg_namelen = sizeof(sockaddr_storage);
      hdr->msg_iov = &packet_data->iov;
      hdr->msg_iovlen = 1;
      hdr->msg_flags = 0;
      hdr->msg_control = (*results)[i].control_buffer.buffer;
      hdr->msg_controllen = (*results)[i].control_buffer.buffer_len;

      QUICHE_DCHECK_GE(hdr->msg_controllen, kMinCmsgSpaceForRead);
    }
    // If MSG_TRUNC is set on Linux, recvmmsg will return the real packet size
    // in |hdrs[i].msg_len| even if packet buffer is too small to receive it.
    int packets_read = recvmmsg(fd, hdrs, results->size(), MSG_TRUNC, nullptr);
    if (packets_read <= 0) {
      const int error_num = errno;
      if (error_num != EAGAIN) {
        QUIC_LOG_FIRST_N(ERROR, 100)
            << "Error reading packets: " << strerror(error_num);
      }
      return 0;
    }

    for (int i = 0; i < packets_read; ++i) {
      if (hdrs[i].msg_len == 0) {
        continue;
      }

      msghdr& hdr = hdrs[i].msg_hdr;
      if (ABSL_PREDICT_FALSE(hdr.msg_flags & MSG_CTRUNC)) {
        QUIC_BUG(quic_bug_10751_4) << "Control buffer too small. size:"
                                   << (*results)[i].control_buffer.buffer_len
                                   << ", need:" << hdr.msg_controllen;
        continue;
      }

      if (ABSL_PREDICT_FALSE(hdr.msg_flags & MSG_TRUNC)) {
        QUIC_LOG_FIRST_N(WARNING, 100)
            << "Received truncated QUIC packet: buffer size:"
            << (*results)[i].packet_buffer.buffer_len
            << " packet size:" << hdrs[i].msg_len;
        continue;
      }

      (*results)[i].ok = true;
      (*results)[i].packet_buffer.buffer_len = hdrs[i].msg_len;

      QuicUdpPacketInfo* packet_info = &(*results)[i].packet_info;
      if (packet_info_interested.IsSet(QuicUdpPacketInfoBit::PEER_ADDRESS)) {
        packet_info->SetPeerAddress(
            QuicSocketAddress(packet_data_array[i].raw_peer_address));
      }

      if (hdr.msg_controllen > 0) {
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&hdr); cmsg != nullptr;
             cmsg = CMSG_NXTHDR(&hdr, cmsg)) {
          PopulatePacketInfoFromControlMessage(cmsg, packet_info,
                                               packet_info_interested);
        }
      }
    }
    return packets_read;
  }
#else
  size_t num_packets = 0;
  for (ReadPacketResult& result : *results) {
    result.ok = false;
  }
  for (ReadPacketResult& result : *results) {
    errno = 0;
    ReadPacket(fd, packet_info_interested, &result);
    if (!result.ok && errno == EAGAIN) {
      break;
    }
    ++num_packets;
  }
  return num_packets;
#endif
}

WriteResult QuicUdpSocketApi::WritePacket(
    QuicUdpSocketFd fd, const char* packet_buffer, size_t packet_buffer_len,
    const QuicUdpPacketInfo& packet_info) {
  if (!packet_info.HasValue(QuicUdpPacketInfoBit::PEER_ADDRESS)) {
    return WriteResult(WRITE_STATUS_ERROR, EINVAL);
  }

  char control_buffer[512];
  sockaddr_storage raw_peer_address =
      packet_info.peer_address().generic_address();
  iovec iov = {const_cast<char*>(packet_buffer), packet_buffer_len};

  msghdr hdr;
  hdr.msg_name = &raw_peer_address;
  hdr.msg_namelen = packet_info.peer_address().host().IsIPv4()
                        ? sizeof(sockaddr_in)
                        : sizeof(sockaddr_in6);
  hdr.msg_iov = &iov;
  hdr.msg_iovlen = 1;
  hdr.msg_flags = 0;
  hdr.msg_control = nullptr;
  hdr.msg_controllen = 0;

  cmsghdr* cmsg = nullptr;

  // Set self IP.
  if (packet_info.HasValue(QuicUdpPacketInfoBit::V4_SELF_IP) &&
      packet_info.self_v4_ip().IsInitialized()) {
    if (!NextCmsg(&hdr, control_buffer, sizeof(control_buffer), IPPROTO_IP,
                  IP_PKTINFO, sizeof(in_pktinfo), &cmsg)) {
      QUIC_LOG_FIRST_N(ERROR, 100)
          << "Not enough buffer to set self v4 ip address.";
      return WriteResult(WRITE_STATUS_ERROR, EINVAL);
    }
    SetV4SelfIpInControlMessage(packet_info.self_v4_ip(), cmsg);
  } else if (packet_info.HasValue(QuicUdpPacketInfoBit::V6_SELF_IP) &&
             packet_info.self_v6_ip().IsInitialized()) {
    if (!NextCmsg(&hdr, control_buffer, sizeof(control_buffer), IPPROTO_IPV6,
                  IPV6_PKTINFO, sizeof(in6_pktinfo), &cmsg)) {
      QUIC_LOG_FIRST_N(ERROR, 100)
          << "Not enough buffer to set self v6 ip address.";
      return WriteResult(WRITE_STATUS_ERROR, EINVAL);
    }
    SetV6SelfIpInControlMessage(packet_info.self_v6_ip(), cmsg);
  }

#if defined(QUIC_UDP_SOCKET_SUPPORT_TTL)
  // Set ttl.
  if (packet_info.HasValue(QuicUdpPacketInfoBit::TTL)) {
    int cmsg_level =
        packet_info.peer_address().host().IsIPv4() ? IPPROTO_IP : IPPROTO_IPV6;
    int cmsg_type =
        packet_info.peer_address().host().IsIPv4() ? IP_TTL : IPV6_HOPLIMIT;
    if (!NextCmsg(&hdr, control_buffer, sizeof(control_buffer), cmsg_level,
                  cmsg_type, sizeof(int), &cmsg)) {
      QUIC_LOG_FIRST_N(ERROR, 100) << "Not enough buffer to set ttl.";
      return WriteResult(WRITE_STATUS_ERROR, EINVAL);
    }
    *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = packet_info.ttl();
  }
#endif

  // TODO(b/270584616): This code block might go away when full support for
  // marking ECN is implemented.
  if (packet_info.HasValue(QuicUdpPacketInfoBit::ECN)) {
    int cmsg_level =
        packet_info.peer_address().host().IsIPv4() ? IPPROTO_IP : IPPROTO_IPV6;
    int cmsg_type;
    unsigned char value_buf[20];
    socklen_t value_len = sizeof(value_buf);
    if (GetQuicRestartFlag(quic_platform_tos_sockopt)) {
      QUIC_RESTART_FLAG_COUNT(quic_platform_tos_sockopt);
      if (GetEcnCmsgArgsPreserveDscp(
              fd, packet_info.peer_address().host().address_family(),
              packet_info.ecn_codepoint(), cmsg_type, value_buf,
              value_len) != 0) {
        QUIC_LOG_FIRST_N(ERROR, 100)
            << "Could not get ECN msg type for this platform.";
        return WriteResult(WRITE_STATUS_ERROR, EINVAL);
      }
    } else {
      cmsg_type = (cmsg_level == IPPROTO_IP) ? IP_TOS : IPV6_TCLASS;
      *(int*)value_buf = static_cast<int>(packet_info.ecn_codepoint());
      value_len = sizeof(int);
    }
    if (!NextCmsg(&hdr, control_buffer, sizeof(control_buffer), cmsg_level,
                  cmsg_type, value_len, &cmsg)) {
      QUIC_LOG_FIRST_N(ERROR, 100) << "Not enough buffer to set ECN.";
      return WriteResult(WRITE_STATUS_ERROR, EINVAL);
    }
    memcpy(CMSG_DATA(cmsg), value_buf, value_len);
  }

  int rc;
  do {
    rc = sendmsg(fd, &hdr, 0);
  } while (rc < 0 && errno == EINTR);
  if (rc >= 0) {
    return WriteResult(WRITE_STATUS_OK, rc);
  }
  return WriteResult((errno == EAGAIN || errno == EWOULDBLOCK)
                         ? WRITE_STATUS_BLOCKED
                         : WRITE_STATUS_ERROR,
                     errno);
}

}  // namespace quic
