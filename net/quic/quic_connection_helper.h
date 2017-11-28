// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Chrome-specific helper for QuicConnection which uses
// a TaskRunner for alarms, and uses a DatagramClientSocket for writing data.

#ifndef NET_QUIC_QUIC_CONNECTION_HELPER_H_
#define NET_QUIC_QUIC_CONNECTION_HELPER_H_

#include "net/quic/quic_connection.h"

#include <set>

#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_clock.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_time.h"
#include "net/udp/datagram_client_socket.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace net {

class NET_EXPORT_PRIVATE QuicConnectionHelper
    : public QuicConnectionHelperInterface {
 public:
  QuicConnectionHelper(base::TaskRunner* task_runner,
                       const QuicClock* clock,
                       DatagramClientSocket* socket);

  virtual ~QuicConnectionHelper();

  // QuicConnectionHelperInterface
  virtual void SetConnection(QuicConnection* connection) override;
  virtual const QuicClock* GetClock() const override;
  virtual int WritePacketToWire(const QuicEncryptedPacket& packet,
                                int* error) override;
  virtual void SetResendAlarm(QuicPacketSequenceNumber sequence_number,
                              QuicTime::Delta delay) override;
  virtual void SetSendAlarm(QuicTime::Delta delay) override;
  virtual void SetTimeoutAlarm(QuicTime::Delta delay) override;
  virtual bool IsSendAlarmSet() override;
  virtual void UnregisterSendAlarmIfRegistered() override;

  int Read(IOBuffer* buf, int buf_len, const CompletionCallback& callback);
  void GetLocalAddress(IPEndPoint* local_address);
  void GetPeerAddress(IPEndPoint* peer_address);

 private:
  friend class QuicConnectionHelperPeer;

  // An alarm is scheduled for each data-bearing packet as it is sent out.
  // When the alarm goes off, the connection checks to see if the packet has
  // been acked, and resends if it has not.
  void OnResendAlarm(QuicPacketSequenceNumber sequence_number);
  // An alarm that is scheduled when the sent scheduler requires a
  // a delay before sending packets and fires when the packet may be sent.
  void OnSendAlarm();
  // An alarm which fires when the connection may have timed out.
  void OnTimeoutAlarm();
  // A completion callback invoked when a write completes.
  void OnWriteComplete(int result);

  base::WeakPtrFactory<QuicConnectionHelper> weak_factory_;
  base::TaskRunner* task_runner_;
  scoped_ptr<DatagramClientSocket> socket_;
  QuicConnection* connection_;
  const QuicClock* clock_;
  bool send_alarm_registered_;
  bool timeout_alarm_registered_;

  DISALLOW_COPY_AND_ASSIGN(QuicConnectionHelper);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_HELPER_H_
