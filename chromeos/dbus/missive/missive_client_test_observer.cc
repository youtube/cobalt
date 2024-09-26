// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/missive/missive_client_test_observer.h"

#include <tuple>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/test/repeating_test_future.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace {

// Absent destination means any destination is okay.
bool RecordHasDestination(absl::optional<::reporting::Destination> destination,
                          const ::reporting::Record& record) {
  return !destination.has_value() ||
         record.destination() == destination.value();
}
}  // namespace

MissiveClientTestObserver::MissiveClientTestObserver(
    absl::optional<::reporting::Destination> destination)
    : MissiveClientTestObserver(
          base::BindRepeating(&RecordHasDestination, destination)) {}

MissiveClientTestObserver::MissiveClientTestObserver(
    RecordFilterCb record_filter_cb)
    : record_filter_cb_(std::move(record_filter_cb)) {
  DCHECK(MissiveClient::Get());
  DCHECK(MissiveClient::Get()->GetTestInterface());

  MissiveClient::Get()->GetTestInterface()->AddObserver(this);
}

MissiveClientTestObserver::~MissiveClientTestObserver() {
  MissiveClient::Get()->GetTestInterface()->RemoveObserver(this);
}

void MissiveClientTestObserver::OnRecordEnqueued(
    ::reporting::Priority priority,
    const ::reporting::Record& record) {
  if (!record_filter_cb_.Run(record)) {
    return;
  }

  enqueued_records_.AddValue(priority, record);
}

std::tuple<::reporting::Priority, ::reporting::Record>
MissiveClientTestObserver::GetNextEnqueuedRecord() {
  return enqueued_records_.Take();
}

bool MissiveClientTestObserver::HasNewEnqueuedRecords() {
  return !enqueued_records_.IsEmpty();
}

}  // namespace chromeos
