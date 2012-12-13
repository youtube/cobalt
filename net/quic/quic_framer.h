// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_FRAMER_H_
#define NET_QUIC_QUIC_FRAMER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"

namespace net {

class QuicDataReader;
class QuicDataWriter;
class QuicDecrypter;
class QuicEncrypter;
class QuicFramer;

// This class receives callbacks from the framer when packets
// are processed.
class NET_EXPORT_PRIVATE QuicFramerVisitorInterface {
 public:
  virtual ~QuicFramerVisitorInterface() {}

  // Called if an error is detected in the QUIC protocol.
  virtual void OnError(QuicFramer* framer) = 0;

  // Called when a new packet has been recieved, before it
  // has been validated or processed.
  virtual void OnPacket(const IPEndPoint& self_address,
                        const IPEndPoint& peer_address) = 0;

  // Called when a lost packet has been recovered via FEC,
  // before it has been processed.
  virtual void OnRevivedPacket() = 0;

  // Called when the header of a packet had been parsed.
  // If OnPacketHeader returns false, framing for this packet will cease.
  virtual bool OnPacketHeader(const QuicPacketHeader& header) = 0;

  // Called when a data packet is parsed that is part of an FEC group.
  // |payload| is the non-encrypted FEC protected payload of the packet.
  virtual void OnFecProtectedPayload(base::StringPiece payload) = 0;

  // Called when a StreamFrame has been parsed.
  virtual void OnStreamFrame(const QuicStreamFrame& frame) = 0;

  // Called when a AckFrame has been parsed.
  virtual void OnAckFrame(const QuicAckFrame& frame) = 0;

  // Called when a CongestionFeedbackFrame has been parsed.
  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) = 0;

  // Called when a RstStreamFrame has been parsed.
  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) = 0;

  // Called when a ConnectionCloseFrame has been parsed.
  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) = 0;

  // Called when FEC data has been parsed.
  virtual void OnFecData(const QuicFecData& fec) = 0;

  // Called when a packet has been completely processed.
  virtual void OnPacketComplete() = 0;
};

class NET_EXPORT_PRIVATE QuicFecBuilderInterface {
 public:
  virtual ~QuicFecBuilderInterface() {}

  // Called when a data packet is constructed that is part of an FEC group.
  // |payload| is the non-encrypted FEC protected payload of the packet.
  virtual void OnBuiltFecProtectedPayload(const QuicPacketHeader& header,
                                          base::StringPiece payload) = 0;
};

// Class for parsing and constructing QUIC packets.  It has a
// QuicFramerVisitorInterface that is called when packets are parsed.
// It also has a QuicFecBuilder that is called when packets are constructed
// in order to generate FEC data for subsequently building FEC packets.
class NET_EXPORT_PRIVATE QuicFramer {
 public:
  // Constructs a new framer that will own |decrypter| and |encrypter|.
  QuicFramer(QuicDecrypter* decrypter, QuicEncrypter* encrypter);

  virtual ~QuicFramer();

  // Set callbacks to be called from the framer.  A visitor must be set, or
  // else the framer will likely crash.  It is acceptable for the visitor
  // to do nothing.  If this is called multiple times, only the last visitor
  // will be used.
  void set_visitor(QuicFramerVisitorInterface* visitor) {
    visitor_ = visitor;
  }

  // Set a builder to be called from the framer when building FEC protected
  // packets.  If this is called multiple times, only the last builder
  // will be used.  The builder need not be set.
  void set_fec_builder(QuicFecBuilderInterface* builder) {
    fec_builder_ = builder;
  }

  QuicErrorCode error() const {
    return error_;
  }

  // Pass a UDP packet into the framer for parsing.
  // Return true if the packet was processed succesfully. |packet| must be a
  // single, complete UDP packet (not a frame of a packet).  This packet
  // might be null padded past the end of the payload, which will be correctly
  // ignored.
  bool ProcessPacket(const IPEndPoint& self_address,
                     const IPEndPoint& peer_address,
                     const QuicEncryptedPacket& packet);

  // Pass a data packet that was revived from FEC data into the framer
  // for parsing.
  // Return true if the packet was processed succesfully. |payload| must be
  // the complete DECRYPTED payload of the revived packet.
  bool ProcessRevivedPacket(const QuicPacketHeader& header,
                            base::StringPiece payload);

  // Creates a new QuicPacket populated with the fields in |header| and
  // |frames|.  Assigns |*packet| to the address of the new object.
  // Returns true upon success.
  bool ConstructFrameDataPacket(const QuicPacketHeader& header,
                                const QuicFrames& frames,
                                QuicPacket** packet);

  // Creates a new QuicPacket populated with the fields in |header| and
  // |fec|.  Assigns |*packet| to the address of the new object.
  // Returns true upon success.
  bool ConstructFecPacket(const QuicPacketHeader& header,
                          const QuicFecData& fec,
                          QuicPacket** packet);

  void WriteSequenceNumber(QuicPacketSequenceNumber sequence_number,
                           QuicPacket* packet);

  void WriteFecGroup(QuicFecGroupNumber fec_group, QuicPacket* packet);

  // Returns a new encrypted packet, owned by the caller.
  QuicEncryptedPacket* EncryptPacket(const QuicPacket& packet);

  // Returns the maximum length of plaintext that can be encrypted
  // to ciphertext no larger than |ciphertext_size|.
  size_t GetMaxPlaintextSize(size_t ciphertext_size);

  const std::string& detailed_error() { return detailed_error_; }

 private:
  bool WritePacketHeader(const QuicPacketHeader& header,
                         QuicDataWriter* builder);

  bool ProcessPacketHeader(QuicPacketHeader* header,
                           const QuicEncryptedPacket& packet);

  bool ProcessFrameData();
  bool ProcessStreamFrame();
  bool ProcessPDUFrame();
  bool ProcessAckFrame(QuicAckFrame* frame);
  bool ProcessReceivedInfo(ReceivedPacketInfo* received_info);
  bool ProcessSentInfo(SentPacketInfo* sent_info);
  bool ProcessQuicCongestionFeedbackFrame(
      QuicCongestionFeedbackFrame* congestion_feedback);
  bool ProcessRstStreamFrame();
  bool ProcessConnectionCloseFrame();

  bool DecryptPayload(const QuicEncryptedPacket& packet);

  // Computes the wire size in bytes of the payload of |frame|.
  size_t ComputeFramePayloadLength(const QuicFrame& frame);

  bool AppendStreamFramePayload(const QuicStreamFrame& frame,
                                QuicDataWriter* builder);
  bool AppendAckFramePayload(const QuicAckFrame& frame,
                             QuicDataWriter* builder);
  bool AppendQuicCongestionFeedbackFramePayload(
      const QuicCongestionFeedbackFrame& frame,
      QuicDataWriter* builder);
  bool AppendRstStreamFramePayload(const QuicRstStreamFrame& frame,
                                   QuicDataWriter* builder);
  bool AppendConnectionCloseFramePayload(
      const QuicConnectionCloseFrame& frame,
      QuicDataWriter* builder);
  bool RaiseError(QuicErrorCode error);

  void set_error(QuicErrorCode error) {
    error_ = error;
  }

  void set_detailed_error(const char* error) {
    detailed_error_ = error;
  }

  std::string detailed_error_;
  scoped_ptr<QuicDataReader> reader_;
  QuicFramerVisitorInterface* visitor_;
  QuicFecBuilderInterface* fec_builder_;
  QuicErrorCode error_;
  // Buffer containing decrypted payload data during parsing.
  scoped_ptr<QuicData> decrypted_;
  // Decrypter used to decrypt packets during parsing.
  scoped_ptr<QuicDecrypter> decrypter_;
  // Encrypter used to encrypt packets via EncryptPacket().
  scoped_ptr<QuicEncrypter> encrypter_;

  DISALLOW_COPY_AND_ASSIGN(QuicFramer);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_FRAMER_H_
