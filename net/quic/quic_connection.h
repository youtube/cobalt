// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The entity that handles framing writes for a Quic client or server.
// Each QuicSession will have a connection associated with it.
//
// On the server side, the Dispatcher handles the raw reads, and hands off
// packets via ProcessUdpPacket for framing and processing.
//
// On the client side, the Connection handles the raw reads, as well as the
// processing.
//
// Note: this class is not thread-safe.

#ifndef NET_QUIC_QUIC_CONNECTION_H_
#define NET_QUIC_QUIC_CONNECTION_H_

#include <list>
#include <set>
#include <vector>

#include "base/hash_tables.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_protocol.h"

namespace net {

class QuicClock;
class QuicConnection;
class QuicEncrypter;
class QuicFecGroup;
class QuicReceiptMetricsCollector;
class QuicSendScheduler;

class NET_EXPORT_PRIVATE QuicConnectionVisitorInterface {
 public:
  typedef std::set<QuicPacketSequenceNumber> AckedPackets;

  virtual ~QuicConnectionVisitorInterface() {}

  // A simple visitor interface for dealing with data frames.  The session
  // should determine if all frames will be accepted, and return true if so.
  // If any frames can't be processed or buffered, none of the data should
  // be used, and the callee should return false.
  virtual bool OnPacket(const IPEndPoint& self_address,
                        const IPEndPoint& peer_address,
                        const QuicPacketHeader& header,
                        const std::vector<QuicStreamFrame>& frame) = 0;

  // Called when the stream is reset by the peer.
  virtual void OnRstStream(const QuicRstStreamFrame& frame) = 0;

  // Called when the connection is closed either locally by the framer, or
  // remotely by the peer.
  virtual void ConnectionClose(QuicErrorCode error, bool from_peer) = 0;

  // Called when packets are acked by the peer.
  virtual void OnAck(AckedPackets acked_packets) = 0;

  // Called when a blocked socket becomes writable.  If all pending bytes for
  // this visitor are consumed by the connection successfully this should
  // return true, otherwise it should return false.
  virtual bool OnCanWrite() = 0;
};

class NET_EXPORT_PRIVATE QuicConnectionHelperInterface {
 public:
  virtual ~QuicConnectionHelperInterface() {}

  // Sets the QuicConnection to be used by this helper.  This method
  // must only be called once.
  virtual void SetConnection(QuicConnection* connection) = 0;

  // Returns a QuicClock to be used for all time related functions.
  virtual const QuicClock* GetClock() const = 0;

  // Sends the packet out to the peer, possibly simulating packet
  // loss if FLAGS_fake_packet_loss_percentage is set.  If the write failed
  // errno will be copied to |*error|.
  virtual int WritePacketToWire(const QuicEncryptedPacket& packet,
                                int* error) = 0;

  // Sets up an alarm to resend the packet with the given sequence number if we
  // haven't gotten an ack in the expected time frame.  Implementations must
  // invoke MaybeResendPacket when the alarm fires.  Implementations must also
  // handle the case where |this| is deleted before the alarm fires.
  virtual void SetResendAlarm(QuicPacketSequenceNumber sequence_number,
                              QuicTime::Delta delay) = 0;

  // Sets an alarm to send packets after |delay_in_us|.  Implementations must
  // invoke OnCanWrite when the alarm fires.  Implementations must also
  // handle the case where |this| is deleted before the alarm fires.
  virtual void SetSendAlarm(QuicTime::Delta delay) = 0;

  // Sets An alarm which fires when the connection may have timed out.
  // Implementations must call CheckForTimeout() and then reregister the alarm
  // if the connection has not yet timed out.
  virtual void SetTimeoutAlarm(QuicTime::Delta delay) = 0;

  // Returns true if a send alarm is currently set.
  virtual bool IsSendAlarmSet() = 0;

  // If a send alarm is currently set, this method unregisters it.  If
  // no send alarm is set, it does nothing.
  virtual void UnregisterSendAlarmIfRegistered() = 0;
};

class NET_EXPORT_PRIVATE QuicConnection : public QuicFramerVisitorInterface {
 public:
  // Constructs a new QuicConnection for the specified |guid| and |address|.
  // |helper| will be owned by this connection.
  QuicConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicConnectionHelperInterface* helper);
  virtual ~QuicConnection();

  // Send the data payload to the peer.
  // TODO(wtc): document the return value.
  size_t SendStreamData(QuicStreamId id,
                        base::StringPiece data,
                        QuicStreamOffset offset,
                        bool fin,
                        QuicPacketSequenceNumber* last_packet);
  // Send a stream reset frame to the peer.
  virtual void SendRstStream(QuicStreamId id,
                             QuicErrorCode error,
                             QuicStreamOffset offset);
  // Sends a connection close frame to the peer, and notifies the visitor of
  // the close.
  virtual void SendConnectionClose(QuicErrorCode error);

  // Processes an incoming UDP packet (consisting of a QuicEncryptedPacket) from
  // the peer.  If processing this packet permits a packet to be revived from
  // its FEC group that packet will be revived and processed.
  virtual void ProcessUdpPacket(const IPEndPoint& self_address,
                                const IPEndPoint& peer_address,
                                const QuicEncryptedPacket& packet);

  // Called when the underlying connection becomes writable to allow
  // queued writes to happen.  Returns false if the socket has become blocked.
  virtual bool OnCanWrite();

  // From QuicFramerVisitorInterface
  virtual void OnError(QuicFramer* framer) override;
  virtual void OnPacket(const IPEndPoint& self_address,
                        const IPEndPoint& peer_address) override;
  virtual void OnRevivedPacket() override;
  virtual bool OnPacketHeader(const QuicPacketHeader& header) override;
  virtual void OnFecProtectedPayload(base::StringPiece payload) override;
  virtual void OnStreamFrame(const QuicStreamFrame& frame) override;
  virtual void OnAckFrame(const QuicAckFrame& frame) override;
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) override;
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) override;
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) override;
  virtual void OnFecData(const QuicFecData& fec) override;
  virtual void OnPacketComplete() override;

  // Accessors
  void set_visitor(QuicConnectionVisitorInterface* visitor) {
    visitor_ = visitor;
  }
  const IPEndPoint& self_address() const { return self_address_; }
  const IPEndPoint& peer_address() const { return peer_address_; }
  QuicGuid guid() const { return guid_; }
  const QuicClock* clock() const { return clock_; }

  // Updates the internal state concerning which packets have been acked, and
  // sends an ack if new data frames have been received.
  void AckPacket(const QuicPacketHeader& header);

  // Called by a ResendAlarm when the timer goes off.  If the packet has been
  // acked it's a no op, otherwise the packet is resent and another alarm is
  // scheduled.
  void MaybeResendPacket(QuicPacketSequenceNumber sequence_number);

  QuicPacketCreator::Options* options() { return packet_creator_.options(); }

  bool connected() { return connected_; }

  size_t NumFecGroups() const { return group_map_.size(); }

  size_t NumQueuedPackets() const { return queued_packets_.size(); }

  // If the connection has timed out, this will close the connection and return
  // true.  Otherwise, it will return false and will reset the timeout alarm.
  bool CheckForTimeout();

  // Returns true of the next packet to be sent should be "lost" by
  // not actually writing it to the wire.
  bool ShouldSimulateLostPacket();

 protected:
  // Send a packet to the peer.  If should_resend is true, this packet contains
  // data, and contents will be resent with a new sequence number if we don't
  // get an ack.  If force is true, then the packet will be sent immediately and
  // the send scheduler will not be consulted.  If is_retransmit is true, this
  // packet is being retransmitted with a new sequence number.
  // TODO(wtc): none of the callers check the return value.
  virtual bool SendPacket(QuicPacketSequenceNumber number,
                          QuicPacket* packet,
                          bool should_resend,
                          bool force,
                          bool is_retransmit);

  // Make sure an ack we got from our peer is sane.
  bool ValidateAckFrame(const QuicAckFrame& incoming_ack);

  // These two are called by OnAckFrame to update the appropriate internal
  // state.
  //
  // Updates internal state based on incoming_ack.received_info
  void UpdatePacketInformationReceivedByPeer(
      const QuicAckFrame& incoming_ack);
  // Updates internal state based in incoming_ack.sent_info
  void UpdatePacketInformationSentByPeer(const QuicAckFrame& incoming_ack);

  // Utility which sets SetLeastUnacked to least_unacked, and updates the list
  // of non-retransmitting packets accordingly.
  void SetLeastUnacked(QuicPacketSequenceNumber least_unacked);

  // Helper to update least unacked.  If acked_sequence_number was not the least
  // unacked packet, this is a no-op.  If it was the least least unacked packet,
  // This finds the new least unacked packet and updates the outgoing ack frame.
  void UpdateLeastUnacked(QuicPacketSequenceNumber acked_sequence_number);

  QuicConnectionHelperInterface* helper() { return helper_; }

 private:
  friend class QuicConnectionPeer;
  // Packets which have not been written to the wire.
  struct QueuedPacket {
    QueuedPacket(QuicPacketSequenceNumber sequence_number,
                 QuicPacket* packet,
                 bool resend,
                 bool retransmit)
        : sequence_number(sequence_number),
          packet(packet),
          resend(resend),
          retransmit(retransmit) {
    }

    QuicPacketSequenceNumber sequence_number;
    QuicPacket* packet;
    bool resend;
    bool retransmit;
  };

  struct UnackedPacket {
    explicit UnackedPacket(QuicPacket* packet)
        : packet(packet), number_nacks(0) {
    }
    QuicPacket* packet;
    uint8 number_nacks;
  };

  typedef std::list<QueuedPacket> QueuedPacketList;
  typedef base::hash_map<QuicPacketSequenceNumber,
      UnackedPacket> UnackedPacketMap;
  typedef std::map<QuicFecGroupNumber, QuicFecGroup*> FecGroupMap;

  // The amount of time we wait before resending a packet.
  static const QuicTime::Delta DefaultResendTime() {
    return QuicTime::Delta::FromMilliseconds(500);
  }

  // Sets up a packet with an QuicAckFrame and sends it out.
  void SendAck();

  // If a packet can be revived from the current FEC group, then
  // revive and process the packet.
  void MaybeProcessRevivedPacket();

  // Get the FEC group associate with the last processed packet.
  QuicFecGroup* GetFecGroup();

  // Fills the ack frame with the appropriate latched information.
  void FillAckFrame(QuicAckFrame* ack);

  // Closes any FEC groups protecting packets before |sequence_number|.
  void CloseFecGroupsBefore(QuicPacketSequenceNumber sequence_number);

  QuicConnectionHelperInterface* helper_;
  QuicFramer framer_;
  const QuicClock* clock_;

  const QuicGuid guid_;
  IPEndPoint self_address_;
  IPEndPoint peer_address_;

  bool last_packet_revived_;  // True if the last packet was revived from FEC.
  size_t last_size_;  // Size of the last received packet.
  QuicPacketHeader last_header_;
  std::vector<QuicStreamFrame> frames_;

  QuicAckFrame outgoing_ack_;
  QuicCongestionFeedbackFrame outgoing_congestion_feedback_;

  // Track some client state so we can do less bookkeeping
  //
  QuicPacketSequenceNumber largest_seen_packet_with_ack_;
  QuicPacketSequenceNumber least_packet_awaiting_ack_;

  // When new packets are created which may be resent, they are added
  // to this map, which contains owning pointers.
  UnackedPacketMap unacked_packets_;

  // When packets could not be sent because the socket was not writable,
  // they are added to this list.  For packets that are not resendable, this
  // list contains owning pointers, since they are not added to
  // unacked_packets_.
  QueuedPacketList queued_packets_;

  // True when the socket becomes unwritable.
  bool write_blocked_;

  FecGroupMap group_map_;
  QuicPacketHeader revived_header_;
  scoped_array<char> revived_payload_;

  QuicConnectionVisitorInterface* visitor_;
  QuicPacketCreator packet_creator_;

  // Network idle time before we kill of this connection.
  const QuicTime::Delta timeout_;
  // The time that we got or tried to send a packet for this connection.
  QuicTime time_of_last_packet_;

  scoped_ptr<QuicReceiptMetricsCollector> collector_;

  // Scheduler which drives packet send rate.
  scoped_ptr<QuicSendScheduler> scheduler_;

  // The number of packets we resent since sending the last ack.
  uint8 packets_resent_since_last_ack_;

  // True by default.  False if we've received or sent an explicit connection
  // close.
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(QuicConnection);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CONNECTION_H_
