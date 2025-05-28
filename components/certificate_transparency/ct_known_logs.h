// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_KNOWN_LOGS_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_KNOWN_LOGS_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace certificate_transparency {

struct PreviousOperatorEntry {
  // Name of the previous operator.
  const char* const name;
  // Time when the operator stopped operating this log, expressed as a TimeDelta
  // from the Unix Epoch.
  const base::Time end_time;
};

enum class LogType {
  kUnspecified = 0,
  kRFC6962 = 1,
  kStaticCTAPI = 2,
};

struct CTLogInfo {
  // The DER-encoded SubjectPublicKeyInfo for the log.  Note that this is not
  // the same as a "log ID": a log ID is the SHA-256 hash of this value.
  const char* const log_key;
  // The length, in bytes, of |log_key|.
  const size_t log_key_length;
  // The user-friendly log name.
  // Note: This will not be translated.
  const char* const log_name;

  // Spec type of the log.
  LogType log_type;

  // The current operator of the log.
  const char* const current_operator;
  // Previous operators (if any) of the log, ordered in chronological order.
  // This field is not a raw_ptr<> because it only ever points at statically-
  // allocated memory (in log_list-inc.cc) which is never freed, and hence
  // the pointer can never dangle.
  RAW_PTR_EXCLUSION const PreviousOperatorEntry* previous_operators;
  const size_t previous_operators_length;
};

// Returns the time at which the log list was last updated.
COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY) base::Time GetLogListTimestamp();

// Returns information about all known logs, which includes those that are
// presently qualified for inclusion and logs which were previously qualified,
// but have since been disqualified. To determine the status of a given log
// (via its log ID), use |GetDisqualifiedLogs()|.
COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY)
std::vector<CTLogInfo> GetKnownLogs();

// Returns pairs of (log ID, disqualification date) for all disqualified logs,
// where the log ID is the SHA-256 hash of the log's |log_key|).  The list is
// sorted by log ID.  The disqualification date is expressed as seconds since
// the Unix epoch.
//
// Any SCTs that are embedded in certificates issued after the disqualification
// date should not be trusted, nor contribute to any uniqueness or freshness
// requirements.
COMPONENT_EXPORT(CERTIFICATE_TRANSPARENCY)
std::vector<std::pair<std::string, base::Time>> GetDisqualifiedLogs();

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_CT_KNOWN_LOGS_H_
