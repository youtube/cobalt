// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_CLIENT_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"

#include "components/metrics/structured/delegating_events_processor.h"
#include "components/metrics/structured/event.h"
#include "components/metrics/structured/events_processor_interface.h"

namespace metrics::structured {

// Singleton to interact with StructuredMetrics.
//
// It allows a delegate to be set to control the recording logic as different
// embedders have different requirements (ie ash vs lacros).
class StructuredMetricsClient {
 public:
  class RecordingDelegate {
   public:
    virtual ~RecordingDelegate() = default;

    // Return true when the delegate is ready to write events.
    virtual bool IsReadyToRecord() const = 0;

    // Recording logic.
    virtual void RecordEvent(Event&& event) = 0;
  };

  StructuredMetricsClient(const StructuredMetricsClient& client) = delete;
  StructuredMetricsClient& operator=(const StructuredMetricsClient& client) =
      delete;

  // Provides access to global StructuredMetricsClient instance to record
  // metrics. This is typically used in the codegen.
  static StructuredMetricsClient* Get();

  // Forwards to |delegate_|. If no delegate has been set, then no-op.
  void Record(Event&& event);

  // Sets the delegate for the client's recording logic. Should be called before
  // anything else. |this| does not take ownership of |delegate| and assumes
  // that the caller will properly manage the lifetime of delegate and call
  // |UnsetDelegate| before |delegate| is destructed.
  void SetDelegate(RecordingDelegate* delegate);
  void UnsetDelegate();

 private:
  friend class base::NoDestructor<StructuredMetricsClient>;

  StructuredMetricsClient();
  ~StructuredMetricsClient();

  // Not owned. Assumes that the delegate's lifetime will exceed |this|.
  raw_ptr<RecordingDelegate> delegate_ = nullptr;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_CLIENT_H_
