// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection_helper.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/quic_send_scheduler.h"
#include "net/quic/quic_utils.h"

namespace net {

QuicConnectionHelper::QuicConnectionHelper(base::TaskRunner* task_runner,
                                           const QuicClock* clock,
                                           DatagramClientSocket* socket)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      task_runner_(task_runner),
      socket_(socket),
      clock_(clock),
      send_alarm_registered_(false),
      timeout_alarm_registered_(false) {
}

QuicConnectionHelper::~QuicConnectionHelper() {
}

void QuicConnectionHelper::SetConnection(QuicConnection* connection) {
  connection_ = connection;
}

const QuicClock* QuicConnectionHelper::GetClock() const {
  return clock_;
}

int QuicConnectionHelper::WritePacketToWire(
    const QuicEncryptedPacket& packet,
    int* error) {
  if (connection_->ShouldSimulateLostPacket()) {
    DLOG(INFO) << "Dropping packet due to fake packet loss.";
    *error = 0;
    return packet.length();
  }

  scoped_refptr<StringIOBuffer> buf(
      new StringIOBuffer(std::string(packet.data(),
                                     packet.length())));
   return socket_->Write(buf, packet.length(),
                         base::Bind(&QuicConnectionHelper::OnWriteComplete,
                                    weak_factory_.GetWeakPtr()));
}

void QuicConnectionHelper::SetResendAlarm(
    QuicPacketSequenceNumber sequence_number,
    QuicTime::Delta delay) {
  // TODO(rch): Coalesce these alarms.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&QuicConnectionHelper::OnResendAlarm,
                 weak_factory_.GetWeakPtr(), sequence_number),
      base::TimeDelta::FromMicroseconds(delay.ToMicroseconds()));
}

void QuicConnectionHelper::SetSendAlarm(QuicTime::Delta delay) {
  send_alarm_registered_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&QuicConnectionHelper::OnSendAlarm,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMicroseconds(delay.ToMicroseconds()));
}

void QuicConnectionHelper::SetTimeoutAlarm(QuicTime::Delta delay) {
  DCHECK(!timeout_alarm_registered_);
  timeout_alarm_registered_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&QuicConnectionHelper::OnTimeoutAlarm,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMicroseconds(delay.ToMicroseconds()));
}

bool QuicConnectionHelper::IsSendAlarmSet() {
  return send_alarm_registered_;
}

void QuicConnectionHelper::UnregisterSendAlarmIfRegistered() {
  send_alarm_registered_ = false;
}

void QuicConnectionHelper::OnResendAlarm(
    QuicPacketSequenceNumber sequence_number) {
  connection_->MaybeResendPacket(sequence_number);
}

void QuicConnectionHelper::OnSendAlarm() {
  if (send_alarm_registered_) {
    send_alarm_registered_ = false;
    connection_->OnCanWrite();
  }
}

void QuicConnectionHelper::OnTimeoutAlarm() {
  timeout_alarm_registered_ = false;
  connection_->CheckForTimeout();
}

void QuicConnectionHelper::OnWriteComplete(int result) {
  // TODO(rch): Inform the connection about the result.
  connection_->OnCanWrite();
}

}  // namespace net
