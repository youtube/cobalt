/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/channel.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/cleanup/cleanup.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "api/crypto/crypto_options.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "call/rtp_demuxer.h"
#include "media/base/codec.h"
#include "media/base/media_channel.h"
#include "media/base/rid_description.h"
#include "media/base/rtp_utils.h"
#include "media/base/stream_params.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "p2p/dtls/dtls_transport_internal.h"
#include "pc/rtp_media_utils.h"
#include "pc/rtp_transport_internal.h"
#include "pc/session_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/containers/flat_set.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/strings/string_format.h"
#include "rtc_base/thread.h"
#include "rtc_base/trace_event.h"
#include "rtc_base/unique_id_generator.h"

namespace webrtc {
namespace {

// Finds a stream based on target's Primary SSRC or RIDs.
// This struct is used in BaseChannel::UpdateLocalStreams_w.
struct StreamFinder {
  explicit StreamFinder(const StreamParams* target) : target_(target) {
    RTC_DCHECK(target);
  }

  bool operator()(const StreamParams& sp) const {
    if (target_->has_ssrcs() && sp.has_ssrcs()) {
      return sp.has_ssrc(target_->first_ssrc());
    }

    if (!target_->has_rids() && !sp.has_rids()) {
      return false;
    }

    const std::vector<RidDescription>& target_rids = target_->rids();
    const std::vector<RidDescription>& source_rids = sp.rids();
    if (source_rids.size() != target_rids.size()) {
      return false;
    }

    // Check that all RIDs match.
    return std::equal(source_rids.begin(), source_rids.end(),
                      target_rids.begin(),
                      [](const RidDescription& lhs, const RidDescription& rhs) {
                        return lhs.rid == rhs.rid;
                      });
  }

  const StreamParams* target_;
};

void MediaChannelParametersFromMediaDescription(
    const MediaContentDescription* desc,
    const RtpHeaderExtensions& extensions,
    bool is_stream_active,
    MediaChannelParameters* params) {
  RTC_DCHECK(desc->type() == MediaType::AUDIO ||
             desc->type() == MediaType::VIDEO);
  params->is_stream_active = is_stream_active;
  params->codecs = desc->codecs();
  params->extensions = extensions;
  params->rtcp.reduced_size = desc->rtcp_reduced_size();
  params->rtcp.remote_estimate = desc->remote_estimate();
  params->rtcp_cc_ack_type = desc->preferred_rtcp_cc_ack_type();
}

void RtpSendParametersFromMediaDescription(
    const MediaContentDescription* desc,
    RtpExtension::Filter extensions_filter,
    SenderParameters* send_params) {
  RtpHeaderExtensions extensions = RtpExtension::DeduplicateHeaderExtensions(
      desc->rtp_header_extensions(), extensions_filter);
  const bool is_stream_active =
      RtpTransceiverDirectionHasRecv(desc->direction());
  MediaChannelParametersFromMediaDescription(desc, extensions, is_stream_active,
                                             send_params);
  send_params->max_bandwidth_bps = desc->bandwidth();
  send_params->extmap_allow_mixed = desc->extmap_allow_mixed();
}

// Ensure that there is a matching packetization for each codec in
// new_params. If old_params had a special packetization but the
// response in new_params has no special packetization we amend
// old_params by ignoring the packetization and fall back to standard
// packetization instead.
RTCError MaybeIgnorePacketization(const MediaChannelParameters& new_params,
                                  MediaChannelParameters& old_params) {
  flat_set<const Codec*> matched_codecs;
  for (Codec& codec : old_params.codecs) {
    if (absl::c_any_of(matched_codecs,
                       [&](const Codec* c) { return codec.Matches(*c); })) {
      continue;
    }

    std::vector<const Codec*> new_codecs =
        FindAllMatchingCodecs(new_params.codecs, codec);
    if (new_codecs.empty()) {
      continue;
    }

    bool may_ignore_packetization = false;
    bool has_matching_packetization = false;
    for (const Codec* new_codec : new_codecs) {
      if (!new_codec->packetization.has_value() &&
          codec.packetization.has_value()) {
        may_ignore_packetization = true;
      } else if (new_codec->packetization == codec.packetization) {
        has_matching_packetization = true;
        break;
      }
    }

    if (may_ignore_packetization) {
      // Note: this writes into old_params
      codec.packetization = std::nullopt;
    } else if (!has_matching_packetization) {
      std::string error_desc = absl::StrFormat(
          "Failed to set local answer due to incompatible codec "
          "packetization for pt='%v' specified.",
          codec.id);
      return RTCError(RTCErrorType::INTERNAL_ERROR, error_desc);
    }

    if (has_matching_packetization) {
      matched_codecs.insert(&codec);
    }
  }
  return RTCError::OK();
}

}  // namespace

BaseChannel::BaseChannel(
    TaskQueueBase* worker_thread,
    Thread* network_thread,
    TaskQueueBase* signaling_thread,
    std::unique_ptr<MediaSendChannelInterface> send_media_channel_impl,
    std::unique_ptr<MediaReceiveChannelInterface> receive_media_channel_impl,
    absl::string_view mid,
    bool srtp_required,
    CryptoOptions crypto_options,
    UniqueRandomIdGenerator* ssrc_generator)
    : media_send_channel_(std::move(send_media_channel_impl)),
      media_receive_channel_(std::move(receive_media_channel_impl)),
      worker_thread_(worker_thread),
      network_thread_(network_thread),
      signaling_thread_(signaling_thread),
      alive_(PendingTaskSafetyFlag::Create()),
      srtp_required_(srtp_required),
      extensions_filter_(
          crypto_options.srtp.enable_encrypted_rtp_header_extensions
              ? RtpExtension::kPreferEncryptedExtension
              : RtpExtension::kDiscardEncryptedExtension),
      mid_(std::string(mid)),
      ssrc_generator_(ssrc_generator) {
  RTC_DCHECK_RUN_ON(worker_thread_);
  RTC_DCHECK(media_send_channel_);
  RTC_DCHECK(media_receive_channel_);
  RTC_DCHECK(ssrc_generator_);
  RTC_DLOG(LS_INFO) << "Created channel: " << ToString();
}

BaseChannel::~BaseChannel() {
  TRACE_EVENT0("webrtc", "BaseChannel::~BaseChannel");
  RTC_DCHECK_RUN_ON(worker_thread_);

  // Eats any outstanding messages or packets.
  alive_->SetNotAlive();
  // The media channel is destroyed at the end of the destructor, since it
  // is a std::unique_ptr. The transport channel (rtp_transport) must outlive
  // the media channel.
}

std::string BaseChannel::ToString() const {
  return StringFormat(
      "{mid: %s, media_type: %s}", mid().c_str(),
      MediaTypeToString(media_send_channel_->media_type()).c_str());
}

bool BaseChannel::ConnectToRtpTransport_n(RtpTransportInternal* rtp_transport) {
  RTC_DCHECK(!rtp_transport_);
  RTC_DCHECK(rtp_transport);
  RTC_DCHECK(media_send_channel());

  // We don't need to call OnDemuxerCriteriaUpdatePending/Complete because
  // there's no previous criteria to worry about.
  RtpDemuxerCriteria criteria = demuxer_criteria();
  if (!rtp_transport->RegisterRtpDemuxerSink(criteria, this)) {
    return false;
  }
  rtp_transport_ = rtp_transport;
  rtp_transport_->SubscribeReadyToSend(
      this, [this](bool ready) { OnTransportReadyToSend(ready); });
  rtp_transport_->SubscribeNetworkRouteChanged(
      this, [this](std::optional<NetworkRoute> route) {
        OnNetworkRouteChanged(route);
      });
  rtp_transport_->SubscribeWritableState(
      this, [this](bool state) { OnWritableState(state); });
  rtp_transport_->SubscribeSentPacket(
      this,
      [this](const SentPacketInfo& packet) { SignalSentPacket_n(packet); });
  return true;
}

void BaseChannel::DisconnectFromRtpTransport_n() {
  RTC_DCHECK(rtp_transport_);
  RTC_DCHECK(media_send_channel());
  rtp_transport_->UnregisterRtpDemuxerSink(this);
  rtp_transport_->UnsubscribeReadyToSend(this);
  rtp_transport_->UnsubscribeNetworkRouteChanged(this);
  rtp_transport_->UnsubscribeWritableState(this);
  rtp_transport_->UnsubscribeSentPacket(this);
  rtp_transport_ = nullptr;
  media_send_channel()->SetInterface(nullptr);
  media_receive_channel()->SetInterface(nullptr);
}

bool BaseChannel::SetRtpTransport(RtpTransportInternal* rtp_transport) {
  TRACE_EVENT0("webrtc", "BaseChannel::SetRtpTransport");
  RTC_DCHECK_RUN_ON(network_thread());
  if (rtp_transport == rtp_transport_) {
    return true;
  }

  if (rtp_transport_) {
    DisconnectFromRtpTransport_n();
  }

  RTC_DCHECK(!rtp_transport_);

  if (!rtp_transport) {
    return true;  // We're done.
  }

  if (!ConnectToRtpTransport_n(rtp_transport)) {
    RTC_DCHECK(!rtp_transport_);
    return false;
  }

  RTC_DCHECK_EQ(rtp_transport_, rtp_transport);

  RTC_DCHECK(!media_send_channel()->HasNetworkInterface());
  media_send_channel()->SetInterface(this);
  media_receive_channel()->SetInterface(this);

  media_send_channel()->OnReadyToSend(rtp_transport_->IsReadyToSend());
  UpdateWritableState_n();

  // Set the cached socket options.
  for (const auto& pair : socket_options_) {
    rtp_transport_->SetRtpOption(pair.first, pair.second);
  }
  if (!rtp_transport_->rtcp_mux_enabled()) {
    for (const auto& pair : rtcp_socket_options_) {
      rtp_transport_->SetRtcpOption(pair.first, pair.second);
    }
  }

  return true;
}

void BaseChannel::Enable(bool enable) {
  RTC_DCHECK_RUN_ON(signaling_thread());

  if (enable == enabled_s_)
    return;

  enabled_s_ = enable;

  worker_thread_->PostTask(SafeTask(alive_, [this, enable] {
    RTC_DCHECK_RUN_ON(worker_thread());
    // Sanity check to make sure that enabled_ and enabled_s_
    // stay in sync.
    RTC_DCHECK_NE(enabled_, enable);
    if (enable) {
      EnableMedia_w();
    } else {
      DisableMedia_w();
    }
  }));
}

RTCError BaseChannel::SetLocalContent(const MediaContentDescription* content,
                                      SdpType type) {
  RTC_DCHECK_RUN_ON(worker_thread());
  TRACE_EVENT0("webrtc", "BaseChannel::SetLocalContent");
  return SetLocalContent_w(content, type);
}

RTCError BaseChannel::SetRemoteContent(const MediaContentDescription* content,
                                       SdpType type) {
  RTC_DCHECK_RUN_ON(worker_thread());
  TRACE_EVENT0("webrtc", "BaseChannel::SetRemoteContent");
  return SetRemoteContent_w(content, type);
}


bool BaseChannel::IsReadyToSendMedia_w() const {
  // Send outgoing data if we are enabled, have local and remote content,
  // and we have had some form of connectivity.
  return enabled_ &&
         RtpTransceiverDirectionHasRecv(remote_content_direction_) &&
         RtpTransceiverDirectionHasSend(local_content_direction_) &&
         was_ever_writable_;
}

bool BaseChannel::SendPacket(CopyOnWriteBuffer* packet,
                             const AsyncSocketPacketOptions& options) {
  return SendPacket(false, packet, options);
}

bool BaseChannel::SendRtcp(CopyOnWriteBuffer* packet,
                           const AsyncSocketPacketOptions& options) {
  return SendPacket(true, packet, options);
}

int BaseChannel::SetOption(SocketType type, Socket::Option opt, int value) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(network_initialized());
  RTC_DCHECK(rtp_transport_);
  switch (type) {
    case ST_RTP:
      socket_options_.push_back(std::pair<Socket::Option, int>(opt, value));
      return rtp_transport_->SetRtpOption(opt, value);
    case ST_RTCP:
      rtcp_socket_options_.push_back(
          std::pair<Socket::Option, int>(opt, value));
      return rtp_transport_->SetRtcpOption(opt, value);
  }
  return -1;
}

void BaseChannel::OnWritableState(bool writable) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(network_initialized());
  if (writable) {
    ChannelWritable_n();
  } else {
    ChannelNotWritable_n();
  }
}

void BaseChannel::OnNetworkRouteChanged(
    std::optional<NetworkRoute> network_route) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(network_initialized());

  RTC_LOG(LS_INFO) << "Network route changed for " << ToString();

  NetworkRoute new_route;
  if (network_route) {
    new_route = *(network_route);
  }
  // Note: When the RTCP-muxing is not enabled, RTCP transport and RTP transport
  // use the same transport name and MediaChannel::OnNetworkRouteChanged cannot
  // work correctly. Intentionally leave it broken to simplify the code and
  // encourage the users to stop using non-muxing RTCP.
  media_send_channel()->OnNetworkRouteChanged(transport_name(), new_route);
}

void BaseChannel::SetFirstPacketReceivedCallback_n(
    absl::AnyInvocable<void(const RtpPacketReceived&) &&> callback) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(!on_first_packet_received_ || !callback);

  // TODO(bugs.webrtc.org/11992): Rename SetFirstPacketReceivedCallback_n to
  // something that indicates network thread initialization/uninitialization and
  // call Init_n() / Deinit_n() respectively.
  // if (!callback)
  //   Deinit_n();

  on_first_packet_received_ = std::move(callback);
}

void BaseChannel::SetFirstPacketSentCallback_n(
    absl::AnyInvocable<void() &&> callback) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(!on_first_packet_sent_ || !callback);

  on_first_packet_sent_ = std::move(callback);
}

void BaseChannel::SetPacketReceivedCallback_n(
    absl::AnyInvocable<void(const RtpPacketReceived&)> callback) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(!on_packet_received_n_ || !callback);

  on_packet_received_n_ = std::move(callback);
}

void BaseChannel::OnTransportReadyToSend(bool ready) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(network_initialized());
  media_send_channel()->OnReadyToSend(ready);
}

bool BaseChannel::SendPacket(bool rtcp,
                             CopyOnWriteBuffer* packet,
                             const AsyncSocketPacketOptions& options) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(network_initialized());
  TRACE_EVENT0("webrtc", "BaseChannel::SendPacket");

  // Until all the code is migrated to use RtpPacketType instead of bool.
  RtpPacketType packet_type = rtcp ? RtpPacketType::kRtcp : RtpPacketType::kRtp;

  // Ensure we have a place to send this packet before doing anything. We might
  // get RTCP packets that we don't intend to send. If we've negotiated RTCP
  // mux, send RTCP over the RTP transport.
  if (!rtp_transport_ || !rtp_transport_->IsWritable(rtcp)) {
    return false;
  }

  // Protect ourselves against crazy data.
  if (!IsValidRtpPacketSize(packet_type, packet->size())) {
    RTC_LOG(LS_ERROR) << "Dropping outgoing " << ToString() << " "
                      << RtpPacketTypeToString(packet_type)
                      << " packet: wrong size=" << packet->size();
    return false;
  }

  if (!srtp_active()) {
    if (srtp_required_) {
      // The audio/video engines may attempt to send RTCP packets as soon as the
      // streams are created, so don't treat this as an error for RTCP.
      // See: https://bugs.chromium.org/p/webrtc/issues/detail?id=6809
      // However, there shouldn't be any RTP packets sent before SRTP is set
      // up (and SetSend(true) is called).
      RTC_DCHECK(rtcp) << "Can't send outgoing RTP packet for " << ToString()
                       << " when SRTP is inactive and crypto is required";
      return false;
    }

    RTC_DLOG(LS_WARNING) << "Sending an " << (rtcp ? "RTCP" : "RTP")
                         << " packet without encryption for " << ToString()
                         << ".";
  }

  if (on_first_packet_sent_ && options.info_signaled_after_sent.is_media) {
    std::move(on_first_packet_sent_)();
    on_first_packet_sent_ = nullptr;
  }

  return rtcp ? rtp_transport_->SendRtcpPacket(packet, options, PF_SRTP_BYPASS)
              : rtp_transport_->SendRtpPacket(packet, options, PF_SRTP_BYPASS);
}

void BaseChannel::OnRtpPacket(const RtpPacketReceived& parsed_packet) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(network_initialized());

  if (on_first_packet_received_) {
    std::move(on_first_packet_received_)(parsed_packet);
    on_first_packet_received_ = nullptr;
  }

  if (!srtp_active() && srtp_required_) {
    // Our session description indicates that SRTP is required, but we got a
    // packet before our SRTP filter is active. This means either that
    // a) we got SRTP packets before we received the SDES keys, in which case
    //    we can't decrypt it anyway, or
    // b) we got SRTP packets before DTLS completed on both the RTP and RTCP
    //    transports, so we haven't yet extracted keys, even if DTLS did
    //    complete on the transport that the packets are being sent on. It's
    //    really good practice to wait for both RTP and RTCP to be good to go
    //    before sending  media, to prevent weird failure modes, so it's fine
    //    for us to just eat packets here. This is all sidestepped if RTCP mux
    //    is used anyway.
    RTC_LOG(LS_WARNING) << "Can't process incoming RTP packet when "
                           "SRTP is inactive and crypto is required "
                        << ToString();
    return;
  }
  if (on_packet_received_n_) {
    on_packet_received_n_(parsed_packet);
  }
  media_receive_channel()->OnPacketReceived(parsed_packet);
}

RTCError BaseChannel::MaybeUpdateDemuxerAndRtpExtensions_w(
    bool update_demuxer,
    std::optional<flat_set<uint8_t>> payload_types,
    const RtpHeaderExtensions& extensions,
    std::optional<flat_set<uint32_t>> ssrcs) {
  const bool pending_update =
      update_demuxer || payload_types.has_value() || ssrcs.has_value();
  if (pending_update) {
    media_receive_channel()->OnDemuxerCriteriaUpdatePending();
  }
  absl::Cleanup cleanup = [this, pending_update] {
    if (pending_update) {
      media_receive_channel()->OnDemuxerCriteriaUpdateComplete();
    }
  };

  // TODO(bugs.webrtc.org/13536): See if we can do this asynchronously.
  RTCError error = network_thread()->BlockingCall([&]() -> RTCError {
    RTC_DCHECK_RUN_ON(network_thread());
    RTCError error =
        rtp_transport_
            ? rtp_transport_->RegisterRtpHeaderExtensionMap(mid(), extensions)
            : (RTCError::InvalidState() << "No transport assigned.");
    if (!error.ok()) {
      error.string_builder() << " (mid=" << mid() << ")";
      return LOG_ERROR(error);
    }

    if (payload_types) {
      if (payload_types_ != *payload_types) {
        payload_types_ = std::move(*payload_types);
        update_demuxer = true;
      }
    }

    if (ssrcs) {
      if (ssrcs_ != *ssrcs) {
        ssrcs_ = std::move(*ssrcs);
        update_demuxer = true;
      }
    }

    if (!update_demuxer)
      return RTCError::OK();

    RtpDemuxerCriteria criteria = demuxer_criteria();
    if (!rtp_transport_->RegisterRtpDemuxerSink(criteria, this)) {
      return RTCError::InvalidParameter()
             << "Failed to apply demuxer criteria for '" << mid() << "': '"
             << criteria.ToString() << "'.";
    }
    return RTCError::OK();
  });

  return error;
}

bool BaseChannel::RegisterRtpDemuxerSink_w(
    bool clear_payload_types,
    std::optional<flat_set<uint32_t>> ssrcs) {
  media_receive_channel()->OnDemuxerCriteriaUpdatePending();
  absl::Cleanup cleanup = [this] {
    media_receive_channel()->OnDemuxerCriteriaUpdateComplete();
  };
  bool ret = network_thread_->BlockingCall([&] {
    RTC_DCHECK_RUN_ON(network_thread());
    if (!rtp_transport_) {
      // Transport was disconnected before attempting to update the
      // criteria. This can happen while setting the remote description.
      // See chromium:1295469 for an example.
      return false;
    }

    bool needs_re_registration = false;

    if (clear_payload_types && !payload_types_.empty()) {
      payload_types_.clear();
      needs_re_registration = true;
    }

    if (ssrcs) {
      if (ssrcs_ != *ssrcs) {
        ssrcs_ = std::move(*ssrcs);
        needs_re_registration = true;
      }
    }

    if (!needs_re_registration) {
      return true;
    }

    // Note that RegisterRtpDemuxerSink first unregisters the sink if
    // already registered. So this will change the state of the class
    // whether the call succeeds or not.
    RtpDemuxerCriteria criteria = demuxer_criteria();
    return rtp_transport_->RegisterRtpDemuxerSink(criteria, this);
  });

  return ret;
}

RtpDemuxerCriteria BaseChannel::demuxer_criteria() const {
  RTC_DCHECK_RUN_ON(network_thread());
  RtpDemuxerCriteria criteria(mid_);
  criteria.payload_types() = payload_types_;
  criteria.ssrcs() = ssrcs_;
  return criteria;
}

void BaseChannel::EnableMedia_w() {
  if (enabled_)
    return;

  RTC_LOG(LS_INFO) << "Channel enabled: " << ToString();
  enabled_ = true;
  UpdateMediaSendRecvState_w();
}

void BaseChannel::DisableMedia_w() {
  if (!enabled_)
    return;

  RTC_LOG(LS_INFO) << "Channel disabled: " << ToString();
  enabled_ = false;
  UpdateMediaSendRecvState_w();
}

void BaseChannel::UpdateWritableState_n() {
  TRACE_EVENT0("webrtc", "BaseChannel::UpdateWritableState_n");
  if (rtp_transport_->IsWritable(/*rtcp=*/true) &&
      rtp_transport_->IsWritable(/*rtcp=*/false)) {
    ChannelWritable_n();
  } else {
    ChannelNotWritable_n();
  }
}

void BaseChannel::ChannelWritable_n() {
  TRACE_EVENT0("webrtc", "BaseChannel::ChannelWritable_n");
  if (writable_) {
    return;
  }
  writable_ = true;
  RTC_LOG(LS_INFO) << "Channel writable (" << ToString() << ")"
                   << (was_ever_writable_n_ ? "" : " for the first time");
  // We only have to do this PostTask once, when first transitioning to
  // writable.
  if (!was_ever_writable_n_) {
    worker_thread_->PostTask(SafeTask(alive_, [this] {
      RTC_DCHECK_RUN_ON(worker_thread());
      was_ever_writable_ = true;
      UpdateMediaSendRecvState_w();
    }));
  }
  was_ever_writable_n_ = true;
}

void BaseChannel::ChannelNotWritable_n() {
  TRACE_EVENT0("webrtc", "BaseChannel::ChannelNotWritable_n");
  if (!writable_) {
    return;
  }
  writable_ = false;
  RTC_LOG(LS_INFO) << "Channel not writable (" << ToString() << ")";
}

RTCError BaseChannel::UpdateLocalStreams_w(
    const std::vector<StreamParams>& streams,
    SdpType type) {
  // In the case of RIDs (where SSRCs are not negotiated), this method will
  // generate an SSRC for each layer in StreamParams. That representation will
  // be stored internally in `local_streams_`.
  // In subsequent offers, the same stream can appear in `streams` again
  // (without the SSRCs), so it should be looked up using RIDs (if available)
  // and then by primary SSRC.
  // In both scenarios, it is safe to assume that the media channel will be
  // created with a StreamParams object with SSRCs. However, it is not safe to
  // assume that `local_streams_` will always have SSRCs as there are scenarios
  // in which niether SSRCs or RIDs are negotiated.

  // Check for streams that have been removed.
  for (const StreamParams& old_stream : local_streams_) {
    if (!old_stream.has_ssrcs() ||
        GetStream(streams, StreamFinder(&old_stream))) {
      continue;
    }
    if (!media_send_channel()->RemoveSendStream(old_stream.first_ssrc())) {
      return RTCError::InvalidParameter()
             << "Failed to remove send stream with ssrc "
             << old_stream.first_ssrc() << " from m-section with mid='" << mid()
             << "'.";
    }
  }
  // Check for new streams.
  std::vector<StreamParams> all_streams;
  for (const StreamParams& stream : streams) {
    StreamParams* existing = GetStream(local_streams_, StreamFinder(&stream));
    if (existing) {
      // Parameters cannot change for an existing stream.
      all_streams.push_back(*existing);
      continue;
    }

    all_streams.push_back(stream);
    StreamParams& new_stream = all_streams.back();

    if (!new_stream.has_ssrcs() && !new_stream.has_rids()) {
      continue;
    }

    RTC_DCHECK(new_stream.has_ssrcs() || new_stream.has_rids());
    if (new_stream.has_ssrcs() && new_stream.has_rids()) {
      return RTCError::InvalidParameter()
             << "Failed to add send stream: " << new_stream.first_ssrc()
             << " into m-section with mid='" << mid()
             << "'. Stream has both SSRCs and RIDs.";
    }

    // At this point we use the legacy simulcast group in StreamParams to
    // indicate that we want multiple layers to the media channel.
    if (!new_stream.has_ssrcs()) {
      // TODO(bugs.webrtc.org/10250): Indicate if flex is desired here.
      new_stream.GenerateSsrcs(new_stream.rids().size(), /* rtx = */ true,
                               /* flex_fec = */ false, ssrc_generator_);
    }

    if (media_send_channel()->AddSendStream(new_stream)) {
      RTC_LOG(LS_INFO) << "Add send stream ssrc: " << new_stream.first_ssrc()
                       << " into " << ToString();
    } else {
      return RTCError::InvalidParameter()
             << "Failed to add send stream ssrc: " << new_stream.first_ssrc()
             << " into m-section with mid='" << mid() << "'";
    }
  }
  local_streams_ = all_streams;
  return RTCError::OK();
}

RTCError BaseChannel::UpdateRemoteStreams_w(
    const MediaContentDescription* content,
    SdpType type) {
  RTC_LOG_THREAD_BLOCK_COUNT();
  bool clear_payload_types = false;
  if (!RtpTransceiverDirectionHasSend(content->direction())) {
    RTC_DLOG(LS_VERBOSE) << "UpdateRemoteStreams_w: remote side will not send "
                            "- disable payload type demuxing for "
                         << ToString();
    clear_payload_types = true;
  }

  const std::vector<StreamParams>& streams = content->streams();
  const bool new_has_unsignaled_ssrcs = HasStreamWithNoSsrcs(streams);
  const bool old_has_unsignaled_ssrcs = HasStreamWithNoSsrcs(remote_streams_);

  // Check for streams that have been removed.
  for (const StreamParams& old_stream : remote_streams_) {
    // If we no longer have an unsignaled stream, we would like to remove
    // the unsignaled stream params that are cached.
    if (!old_stream.has_ssrcs() && !new_has_unsignaled_ssrcs) {
      media_receive_channel()->ResetUnsignaledRecvStream();
      RTC_LOG(LS_INFO) << "Reset unsignaled remote stream for " << ToString()
                       << ".";
    } else if (old_stream.has_ssrcs() &&
               !GetStreamBySsrc(streams, old_stream.first_ssrc())) {
      if (media_receive_channel()->RemoveRecvStream(old_stream.first_ssrc())) {
        RTC_LOG(LS_INFO) << "Remove remote ssrc: " << old_stream.first_ssrc()
                         << " from " << ToString() << ".";
      } else {
        return RTCError::InvalidParameter()
               << "Failed to remove remote stream with ssrc "
               << old_stream.first_ssrc() << " from m-section with mid='"
               << mid() << "'.";
      }
    }
  }

  // Check for new streams.
  flat_set<uint32_t> ssrcs;
  for (const StreamParams& new_stream : streams) {
    // We allow a StreamParams with an empty list of SSRCs, in which case the
    // MediaChannel will cache the parameters and use them for any unsignaled
    // stream received later.
    if ((!new_stream.has_ssrcs() && !old_has_unsignaled_ssrcs) ||
        !GetStreamBySsrc(remote_streams_, new_stream.first_ssrc())) {
      if (media_receive_channel()->AddRecvStream(new_stream)) {
        RTC_LOG(LS_INFO) << "Add remote ssrc: "
                         << (new_stream.has_ssrcs()
                                 ? std::to_string(new_stream.first_ssrc())
                                 : "unsignaled")
                         << " to " << ToString();
      } else {
        return RTCError::InvalidParameter()
               << "Failed to add remote stream ssrc: "
               << (new_stream.has_ssrcs()
                       ? std::to_string(new_stream.first_ssrc())
                       : "unsignaled")
               << " to " << ToString();
      }
    }
    // Update the receiving SSRCs.
    ssrcs.insert(new_stream.ssrcs.begin(), new_stream.ssrcs.end());
  }

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(0);

  // Re-register the sink to update after changing the demuxer criteria.
  if (!RegisterRtpDemuxerSink_w(clear_payload_types, std::move(ssrcs))) {
    return RTCError::InvalidParameter()
           << "Failed to set up audio demuxing for mid='" << mid() << "'.";
  }

  remote_streams_ = streams;

  set_remote_content_direction(content->direction());
  UpdateMediaSendRecvState_w();

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(1);

  return RTCError::OK();
}

RtpHeaderExtensions BaseChannel::GetDeduplicatedRtpHeaderExtensions(
    const RtpHeaderExtensions& extensions) {
  return RtpExtension::DeduplicateHeaderExtensions(extensions,
                                                   extensions_filter_);
}

// TODO: webrtc:42222117 - Move header extension logic in the channel classes
// to the network thread. At the moment, this function does a BlockingCall
// to the network thread in order to delegate the check to the transport.
// The worker and network threads are commonly configured to map to the same
// actual thread, so a blocking call in those cases isn't expensive, although
// not ideal.
RTCError BaseChannel::CheckRtpExtensionValidity(
    const RtpHeaderExtensions& extensions) const {
  return network_thread()->BlockingCall([&]() -> RTCError {
    RTC_DCHECK_RUN_ON(network_thread());
    RTCError error =
        rtp_transport_ ? rtp_transport_->VerifyRtpHeaderExtensionMap(extensions)
                       : (RTCError::InvalidState() << "No transport assigned.");
    if (!error.ok()) {
      error.string_builder() << " (mid=" << mid() << ")";
    }
    return error;
  });
}

void BaseChannel::SignalSentPacket_n(const SentPacketInfo& sent_packet) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK(network_initialized());
  media_send_channel()->OnPacketSent(sent_packet);
}

VoiceChannel::VoiceChannel(
    TaskQueueBase* worker_thread,
    Thread* network_thread,
    TaskQueueBase* signaling_thread,
    std::unique_ptr<VoiceMediaSendChannelInterface> media_send_channel,
    std::unique_ptr<VoiceMediaReceiveChannelInterface> media_receive_channel,
    absl::string_view mid,
    bool srtp_required,
    CryptoOptions crypto_options,
    UniqueRandomIdGenerator* ssrc_generator)
    : BaseChannel(worker_thread,
                  network_thread,
                  signaling_thread,
                  std::move(media_send_channel),
                  std::move(media_receive_channel),
                  mid,
                  srtp_required,
                  crypto_options,
                  ssrc_generator) {}

VoiceChannel::~VoiceChannel() {
  TRACE_EVENT0("webrtc", "VoiceChannel::~VoiceChannel");
  // this can't be done in the base class, since it calls a virtual
  DisableMedia_w();
}

void VoiceChannel::UpdateMediaSendRecvState_w() {
  // Render incoming data if we're the active call, and we have the local
  // content. We receive data on the default channel and multiplexed streams.
  bool receive =
      enabled() && RtpTransceiverDirectionHasRecv(local_content_direction());
  media_receive_channel()->SetPlayout(receive);

  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool send = IsReadyToSendMedia_w();
  media_send_channel()->SetSend(send);

  RTC_LOG(LS_INFO) << "Changing voice state, recv=" << receive
                   << " send=" << send << " for " << ToString();
}

RTCError VoiceChannel::SetLocalContent_w(const MediaContentDescription* content,
                                         SdpType type) {
  TRACE_EVENT0("webrtc", "VoiceChannel::SetLocalContent_w");
  RTC_DLOG(LS_INFO) << "Setting local voice description for " << ToString();

  RTC_LOG_THREAD_BLOCK_COUNT();

  RtpHeaderExtensions header_extensions =
      GetDeduplicatedRtpHeaderExtensions(content->rtp_header_extensions());

  RTCError error = CheckRtpExtensionValidity(header_extensions);
  if (!error.ok()) {
    return error;
  }

  // TODO: issues.webrtc.org/383078466 - remove if pushdown on answer is enough.
  media_send_channel()->SetExtmapAllowMixed(content->extmap_allow_mixed());

  AudioReceiverParameters recv_params = last_recv_params_;
  MediaChannelParametersFromMediaDescription(
      content, header_extensions,
      RtpTransceiverDirectionHasRecv(content->direction()), &recv_params);
  recv_params.mid = mid();

  if (!media_receive_channel()->SetReceiverParameters(recv_params)) {
    return RTCError::InvalidParameter()
           << "Failed to set local audio description recv parameters for "
              "m-section with mid='"
           << mid() << "'.";
  }

  std::optional<flat_set<uint8_t>> payload_types;
  if (RtpTransceiverDirectionHasRecv(content->direction())) {
    payload_types.emplace();
    for (const Codec& codec : content->codecs()) {
      payload_types->insert(codec.id);
    }
  }

  last_recv_params_ = recv_params;

  if (type == SdpType::kAnswer || type == SdpType::kPrAnswer) {
    AudioSenderParameter send_params = last_send_params_;
    RTC_DCHECK(!send_params.mid.empty());
    send_params.extensions = header_extensions;
    send_params.extmap_allow_mixed = content->extmap_allow_mixed();
    if (!media_send_channel()->SetSenderParameters(send_params)) {
      return RTCError::InvalidParameter()
             << "Failed to set send parameters for m-section with mid='"
             << mid() << "'.";
    }
    last_send_params_ = send_params;
  }

  error = UpdateLocalStreams_w(content->streams(), type);
  if (!error.ok()) {
    return error;
  }

  set_local_content_direction(content->direction());
  UpdateMediaSendRecvState_w();

  // Disabled because suggeting PTs takes thread jumps.
  // TODO: https://issues.webrtc.org/360058654 - reenable after cleanup
  // RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(0);

  return MaybeUpdateDemuxerAndRtpExtensions_w(
      /*update_demuxer=*/false, std::move(payload_types),
      recv_params.extensions,
      /*ssrcs=*/std::nullopt);
}

RTCError VoiceChannel::SetRemoteContent_w(
    const MediaContentDescription* content,
    SdpType type) {
  TRACE_EVENT0("webrtc", "VoiceChannel::SetRemoteContent_w");
  RTC_LOG(LS_INFO) << "Setting remote voice description for " << ToString();

  AudioSenderParameter send_params = last_send_params_;
  RtpSendParametersFromMediaDescription(content, extensions_filter(),
                                        &send_params);
  send_params.mid = mid();

  RTCError error = CheckRtpExtensionValidity(send_params.extensions);
  if (!error.ok()) {
    return error;
  }

  if (!media_send_channel()->SetSenderParameters(send_params)) {
    return RTCError::InvalidParameter()
           << "Failed to set remote audio description send parameters for "
              "m-section with mid='"
           << mid() << "'.";
  }

  if (type == SdpType::kAnswer || type == SdpType::kPrAnswer) {
    AudioReceiverParameters recv_params = last_recv_params_;
    recv_params.extensions = send_params.extensions;
    if (!media_receive_channel()->SetReceiverParameters(recv_params)) {
      return RTCError::InvalidParameter()
             << "Failed to set recv parameters for m-section with mid='"
             << mid() << "'.";
    }
    last_recv_params_ = recv_params;
  }
  // The receive channel can send RTCP packets in the reverse direction. It
  // should use the reduced size mode if a peer has requested it through the
  // remote content.
  media_receive_channel()->SetRtcpMode(content->rtcp_reduced_size()
                                           ? RtcpMode::kReducedSize
                                           : RtcpMode::kCompound);
  // Update Receive channel based on Send channel's codec information.
  // TODO(bugs.webrtc.org/14911): This is silly. Stop doing it.
  media_receive_channel()->SetReceiveNackEnabled(
      media_send_channel()->SenderNackEnabled());
  media_receive_channel()->SetReceiveNonSenderRttEnabled(
      media_send_channel()->SenderNonSenderRttEnabled());
  last_send_params_ = send_params;

  error = UpdateRemoteStreams_w(content, type);
  if (error.ok() && (type == SdpType::kAnswer || type == SdpType::kPrAnswer)) {
    error = MaybeUpdateDemuxerAndRtpExtensions_w(
        /*update_demuxer=*/false, /*payload_types=*/std::nullopt,
        last_recv_params_.extensions, /*ssrcs=*/std::nullopt);
  }

  return error;
}

VideoChannel::VideoChannel(
    TaskQueueBase* worker_thread,
    Thread* network_thread,
    TaskQueueBase* signaling_thread,
    std::unique_ptr<VideoMediaSendChannelInterface> media_send_channel,
    std::unique_ptr<VideoMediaReceiveChannelInterface> media_receive_channel,
    absl::string_view mid,
    bool srtp_required,
    CryptoOptions crypto_options,
    UniqueRandomIdGenerator* ssrc_generator)
    : BaseChannel(worker_thread,
                  network_thread,
                  signaling_thread,
                  std::move(media_send_channel),
                  std::move(media_receive_channel),
                  mid,
                  srtp_required,
                  crypto_options,
                  ssrc_generator) {
}

VideoChannel::~VideoChannel() {
  TRACE_EVENT0("webrtc", "VideoChannel::~VideoChannel");
  // this can't be done in the base class, since it calls a virtual
  DisableMedia_w();
}

void VideoChannel::UpdateMediaSendRecvState_w() {
  // Send outgoing data if we're the active call, we have the remote content,
  // and we have had some form of connectivity.
  bool receive =
      enabled() && RtpTransceiverDirectionHasRecv(local_content_direction());
  media_receive_channel()->SetReceive(receive);

  bool send = IsReadyToSendMedia_w();
  media_send_channel()->SetSend(send);
  RTC_LOG(LS_INFO) << "Changing video state, recv=" << receive
                   << " send=" << send << " for " << ToString();
}

RTCError VideoChannel::SetLocalContent_w(const MediaContentDescription* content,
                                         SdpType type) {
  TRACE_EVENT0("webrtc", "VideoChannel::SetLocalContent_w");

  RtpHeaderExtensions header_extensions =
      GetDeduplicatedRtpHeaderExtensions(content->rtp_header_extensions());

  RTCError error = CheckRtpExtensionValidity(header_extensions);
  if (!error.ok()) {
    return error;
  }

  RTC_LOG_THREAD_BLOCK_COUNT();

  // TODO: issues.webrtc.org/383078466 - remove if pushdown on answer is enough.
  media_send_channel()->SetExtmapAllowMixed(content->extmap_allow_mixed());

  VideoReceiverParameters recv_params = last_recv_params_;
  MediaChannelParametersFromMediaDescription(
      content, header_extensions,
      RtpTransceiverDirectionHasRecv(content->direction()), &recv_params);

  VideoSenderParameters send_params = last_send_params_;
  send_params.extensions = header_extensions;
  send_params.extmap_allow_mixed = content->extmap_allow_mixed();

  // Ensure that there is a matching packetization for each send codec. If the
  // other peer offered to exclusively send non-standard packetization but we
  // only accept to receive standard packetization we effectively amend their
  // offer by ignoring the packetiztion and fall back to standard packetization
  // instead.
  if (type == SdpType::kAnswer || type == SdpType::kPrAnswer) {
    error = MaybeIgnorePacketization(recv_params, send_params);
    if (!error.ok()) {
      return error;
    }
  }

  if (!media_receive_channel()->SetReceiverParameters(recv_params)) {
    return RTCError::InvalidParameter()
           << "Failed to set local video description recv parameters for "
              "m-section with mid='"
           << mid() << "'.";
  }

  std::optional<flat_set<uint8_t>> payload_types;
  if (RtpTransceiverDirectionHasRecv(content->direction())) {
    payload_types.emplace();
    for (const Codec& codec : content->codecs()) {
      payload_types->insert(codec.id);
    }
  }

  last_recv_params_ = recv_params;

  if (type == SdpType::kAnswer || type == SdpType::kPrAnswer) {
    if (!media_send_channel()->SetSenderParameters(send_params)) {
      return RTCError::InvalidParameter()
             << "Failed to set send parameters for m-section with mid='"
             << mid() << "'.";
    }
    last_send_params_ = send_params;
  }

  error = UpdateLocalStreams_w(content->streams(), type);
  if (!error.ok()) {
    return error;
  }

  set_local_content_direction(content->direction());
  UpdateMediaSendRecvState_w();

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(0);

  error = MaybeUpdateDemuxerAndRtpExtensions_w(
      /*update_demuxer=*/false, std::move(payload_types),
      recv_params.extensions, /*ssrcs=*/std::nullopt);

  RTC_DCHECK_BLOCK_COUNT_NO_MORE_THAN(1);

  return error;
}

RTCError VideoChannel::SetRemoteContent_w(
    const MediaContentDescription* content,
    SdpType type) {
  TRACE_EVENT0("webrtc", "VideoChannel::SetRemoteContent_w");
  RTC_LOG(LS_INFO) << "Setting remote video description for " << ToString();

  VideoSenderParameters send_params = last_send_params_;
  RtpSendParametersFromMediaDescription(content, extensions_filter(),
                                        &send_params);
  send_params.mid = mid();
  send_params.conference_mode = content->conference_mode();

  RTCError error = CheckRtpExtensionValidity(send_params.extensions);
  if (!error.ok()) {
    return error;
  }

  VideoReceiverParameters recv_params = last_recv_params_;

  // Ensure that there is a matching packetization for each receive codec. If we
  // offered to exclusively receive a non-standard packetization but the other
  // peer only accepts to send standard packetization we effectively amend our
  // offer by ignoring the packetiztion and fall back to standard packetization
  // instead.
  if (type == SdpType::kAnswer || type == SdpType::kPrAnswer) {
    error = MaybeIgnorePacketization(send_params, recv_params);
    if (!error.ok()) {
      return error;
    }
  }

  if (!media_send_channel()->SetSenderParameters(send_params)) {
    return RTCError::InvalidParameter()
           << "Failed to set remote video description send parameters for "
              "m-section with mid='"
           << mid() << "'.";
  }
  if (type == SdpType::kAnswer || type == SdpType::kPrAnswer) {
    recv_params.extensions = send_params.extensions;
    recv_params.rtcp.reduced_size = send_params.rtcp.reduced_size;
    if (!media_receive_channel()->SetReceiverParameters(recv_params)) {
      return RTCError::InvalidParameter()
             << "Failed to set recv parameters for m-section with mid='"
             << mid() << "'.";
    }
    last_recv_params_ = recv_params;
  }
  last_send_params_ = send_params;

  error = UpdateRemoteStreams_w(content, type);
  if (error.ok() && (type == SdpType::kAnswer || type == SdpType::kPrAnswer)) {
    error = MaybeUpdateDemuxerAndRtpExtensions_w(
        /*update_demuxer=*/false, /*payload_types=*/std::nullopt,
        last_recv_params_.extensions, /*ssrcs=*/std::nullopt);
  }

  return error;
}

}  // namespace webrtc
