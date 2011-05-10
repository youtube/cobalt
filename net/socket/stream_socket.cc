// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/stream_socket.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/values.h"

namespace net {

namespace {

// Parameters for SOCKET_BYTES_RECEIVED and SOCKET_BYTES_SENT events.
// Includes bytes transferred and, if |bytes| is not NULL, the bytes themselves.
class NetLogBytesTransferredParameter : public NetLog::EventParameters {
 public:
  NetLogBytesTransferredParameter(int byte_count, const char* bytes);

  virtual Value* ToValue() const;

 private:
  const int byte_count_;
  std::string hex_encoded_bytes_;
  bool has_bytes_;
};

NetLogBytesTransferredParameter::NetLogBytesTransferredParameter(
    int byte_count, const char* transferred_bytes)
    : byte_count_(byte_count),
      has_bytes_(false) {
  if (transferred_bytes) {
    hex_encoded_bytes_ = base::HexEncode(transferred_bytes, byte_count);
    has_bytes_ = true;
  }
}

Value* NetLogBytesTransferredParameter::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("byte_count", byte_count_);
  if (has_bytes_)
    dict->SetString("hex_encoded_bytes", hex_encoded_bytes_);
  return dict;
}

}  // namespace

StreamSocket::UseHistory::UseHistory()
    : was_ever_connected_(false),
      was_used_to_convey_data_(false),
      omnibox_speculation_(false),
      subresource_speculation_(false) {
}

StreamSocket::UseHistory::~UseHistory() {
  EmitPreconnectionHistograms();
}

void StreamSocket::UseHistory::Reset() {
  EmitPreconnectionHistograms();
  was_ever_connected_ = false;
  was_used_to_convey_data_ = false;
  // omnibox_speculation_ and subresource_speculation_ values
  // are intentionally preserved.
}

void StreamSocket::UseHistory::set_was_ever_connected() {
  DCHECK(!was_used_to_convey_data_);
  was_ever_connected_ = true;
}

void StreamSocket::UseHistory::set_was_used_to_convey_data() {
  DCHECK(was_ever_connected_);
  was_used_to_convey_data_ = true;
}


void StreamSocket::UseHistory::set_subresource_speculation() {
  DCHECK(was_ever_connected_);
  // TODO(jar): We should transition to marking a socket (or stream) at
  // construction time as being created for speculative reasons.  This current
  // approach of trying to track use of a socket to convey data can make
  // mistakes when other sockets (such as ones sitting in the pool for a long
  // time) are issued.  Unused sockets can be left over when a when a set of
  // connections to a host are made, and one is "unlucky" and takes so long to
  // complete a connection, that another socket is used, and recycled before a
  // second connection comes available.  Similarly, re-try connections can leave
  // an original (slow to connect socket) in the pool, and that can be issued
  // to a speculative requester. In any cases such old sockets will fail when an
  // attempt is made to used them!... and then it will look like a speculative
  // socket was discarded without any user!?!?!
  if (was_used_to_convey_data_)
    return;
  subresource_speculation_ = true;
}

void StreamSocket::UseHistory::set_omnibox_speculation() {
  DCHECK(was_ever_connected_);
  if (was_used_to_convey_data_)
    return;
  omnibox_speculation_ = true;
}

bool StreamSocket::UseHistory::was_used_to_convey_data() const {
  DCHECK(!was_used_to_convey_data_ || was_ever_connected_);
  return was_used_to_convey_data_;
}

void StreamSocket::UseHistory::EmitPreconnectionHistograms() const {
  DCHECK(!subresource_speculation_ || !omnibox_speculation_);
  // 0 ==> non-speculative, never connected.
  // 1 ==> non-speculative never used (but connected).
  // 2 ==> non-speculative and used.
  // 3 ==> omnibox_speculative never connected.
  // 4 ==> omnibox_speculative never used (but connected).
  // 5 ==> omnibox_speculative and used.
  // 6 ==> subresource_speculative never connected.
  // 7 ==> subresource_speculative never used (but connected).
  // 8 ==> subresource_speculative and used.
  int result;
  if (was_used_to_convey_data_)
    result = 2;
  else if (was_ever_connected_)
    result = 1;
  else
    result = 0;  // Never used, and not really connected.

  if (omnibox_speculation_)
    result += 3;
  else if (subresource_speculation_)
    result += 6;
  UMA_HISTOGRAM_ENUMERATION("Net.PreconnectUtilization2", result, 9);

  static const bool connect_backup_jobs_fieldtrial =
      base::FieldTrialList::TrialExists("ConnnectBackupJobs");
  if (connect_backup_jobs_fieldtrial) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("Net.PreconnectUtilization2",
                                   "ConnnectBackupJobs"),
        result, 9);
  }
}

void StreamSocket::LogByteTransfer(const BoundNetLog& net_log,
                                   NetLog::EventType event_type,
                                   int byte_count,
                                   char* bytes) const {
  scoped_refptr<NetLog::EventParameters> params;
  if (net_log.IsLoggingBytes()) {
    params = new NetLogBytesTransferredParameter(byte_count, bytes);
  } else {
    params = new NetLogBytesTransferredParameter(byte_count, NULL);
  }
  net_log.AddEvent(event_type, params);
}

}  // namespace net
