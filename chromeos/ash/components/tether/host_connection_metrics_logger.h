// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/tether/active_host.h"

namespace base {
class Clock;
}  // namespace base

namespace ash {

namespace tether {

// Wrapper around metrics reporting for host connection results. Clients are
// expected to report the result of a host connection attempt once it has
// concluded.
class HostConnectionMetricsLogger : public ActiveHost::Observer {
 public:
  enum ConnectionToHostResult {
    CONNECTION_RESULT_PROVISIONING_FAILED,
    CONNECTION_RESULT_SUCCESS,
    CONNECTION_RESULT_FAILURE_UNKNOWN_ERROR,
    CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_TIMEOUT,
    CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_CANCELED_BY_USER,
    CONNECTION_RESULT_FAILURE_CLIENT_CONNECTION_INTERNAL_ERROR,
    CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_REQUIRED,
    CONNECTION_RESULT_FAILURE_TETHERING_TIMED_OUT_FIRST_TIME_SETUP_WAS_NOT_REQUIRED,
    CONNECTION_RESULT_FAILURE_TETHERING_UNSUPPORTED,
    CONNECTION_RESULT_FAILURE_NO_CELL_DATA,
    CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_FAILED,
    CONNECTION_RESULT_FAILURE_ENABLING_HOTSPOT_TIMEOUT,
    CONNECTION_RESULT_FAILURE_NO_RESPONSE,
    CONNECTION_RESULT_FAILURE_INVALID_HOTSPOT_CREDENTIALS,
    CONNECTION_RESULT_FAILURE_SUCCESSFUL_REQUEST_BUT_NO_RESPONSE,
    CONNECTION_RESULT_FAILURE_UNRECOGNIZED_RESPONSE_ERROR,
  };

  // Record the result of an attempted host connection.
  virtual void RecordConnectionToHostResult(ConnectionToHostResult result,
                                            const std::string& device_id);

  HostConnectionMetricsLogger(ActiveHost* active_host);

  HostConnectionMetricsLogger(const HostConnectionMetricsLogger&) = delete;
  HostConnectionMetricsLogger& operator=(const HostConnectionMetricsLogger&) =
      delete;

  virtual ~HostConnectionMetricsLogger();

 protected:
  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  friend class HostConnectionMetricsLoggerTest;
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultProvisioningFailure);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultSuccess);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultSuccess_Background);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultSuccess_MultiDeviceApiEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultSuccess_Background_DifferentDevice);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailure);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailure_Background);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailure_MultiDeviceApiEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailure_Background_DifferentDevice);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_Timeout);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_CanceledByUser);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureClientConnection_InternalError);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureTetheringTimeout_SetupRequired);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureTetheringTimeout_SetupNotRequired);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureTetheringUnsupported);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureNoCellData);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureEnablingHotspotFailed);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureEnablingHotspotTimeout);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectToHostDuration);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectToHostDuration_Background);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectToHostDuration_MultiDeviceApiEnabled);
  FRIEND_TEST_ALL_PREFIXES(HostConnectionMetricsLoggerTest,
                           RecordConnectionResultFailureNoResponse);
  FRIEND_TEST_ALL_PREFIXES(
      HostConnectionMetricsLoggerTest,
      RecordConnectionResultFailureInvalidHotspotCredentials);

  // An Instant Tethering connection can fail for several different reasons.
  // Though traditionally success and each failure case would be logged to a
  // single enum, we have chosen to start at a top-level of view of simply
  // "success vs failure", and then iteratively breaking down the failure count
  // into its types (and possibly sub-types). Because of this cascading nature,
  // when a failure sub-type occurs, the code path in question must report that
  // sub-type as well as all the super-types above it. This would be cumbersome,
  // and thus HostConnectionMetricsLogger exists to provide a simple API which
  // handles all the other metrics that need to be reported.
  //
  // The most top-level metric is
  // InstantTethering.ConnectionToHostResult.ProvisioningFailureRate. Its
  // breakdown, and the breakdown of its subsquent metrics, is described in
  // tools/metrics/histograms/histograms.xml.
  enum ConnectionToHostResult_ProvisioningFailureEventType {
    PROVISIONING_FAILED = 0,
    OTHER = 1,
    PROVISIONING_FAILURE_MAX
  };

  enum ConnectionToHostResult_SuccessEventType {
    SUCCESS = 0,
    FAILURE = 1,
    SUCCESS_MAX
  };

  enum ConnectionToHostResult_FailureEventType {
    UNKNOWN_ERROR = 0,
    TETHERING_TIMED_OUT = 1,
    CLIENT_CONNECTION_ERROR = 2,
    TETHERING_UNSUPPORTED = 3,
    NO_CELL_DATA = 4,
    ENABLING_HOTSPOT_FAILED = 5,
    ENABLING_HOTSPOT_TIMEOUT = 6,
    NO_RESPONSE = 7,
    INVALID_HOTSPOT_CREDENTIALS = 8,
    SUCCESSFUL_REQUEST_BUT_NO_RESPONSE = 9,
    UNRECOGNIZED_RESPONSE_ERROR = 10,
    FAILURE_MAX
  };

  enum ConnectionToHostResult_FailureClientConnectionEventType {
    TIMEOUT = 0,
    CANCELED_BY_USER = 1,
    INTERNAL_ERROR = 2,
    FAILURE_CLIENT_CONNECTION_MAX
  };

  enum ConnectionToHostResult_FailureTetheringTimeoutEventType {
    FIRST_TIME_SETUP_WAS_REQUIRED = 0,
    FIRST_TIME_SETUP_WAS_NOT_REQUIRED = 1,
    FAILURE_TETHERING_TIMEOUT_MAX
  };

  // Record if a host connection attempt never went through due to provisioning
  // failure, or otherwise continued.
  void RecordConnectionResultProvisioningFailure(
      ConnectionToHostResult_ProvisioningFailureEventType event_type);

  // Record if a host connection attempt succeeded or failed. Failure is
  // covered by the RecordConnectionResultFailure() method.
  void RecordConnectionResultSuccess(
      ConnectionToHostResult_SuccessEventType event_type);

  // Record how a host connection attempt failed. Failure due to client error or
  // tethering timeout is covered by the
  // RecordConnectionResultFailureClientConnection() or
  // RecordConnectionResultFailureTetheringTimeout() methods, respectively.
  void RecordConnectionResultFailure(
      ConnectionToHostResult_FailureEventType event_type);

  // Record how a host connection attempt failed due to a client error.
  void RecordConnectionResultFailureClientConnection(
      ConnectionToHostResult_FailureClientConnectionEventType event_type);

  // Record the conditions of when host connection attempt failed due to
  // the host timing out during tethering.
  void RecordConnectionResultFailureTetheringTimeout(
      ConnectionToHostResult_FailureTetheringTimeoutEventType event_type);

  void RecordConnectToHostDuration(const std::string device_id);

  void SetClockForTesting(base::Clock* test_clock);

  raw_ptr<ActiveHost, ExperimentalAsh> active_host_;
  raw_ptr<base::Clock, ExperimentalAsh> clock_;

  base::Time connect_to_host_start_time_;
  std::string active_host_device_id_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_HOST_CONNECTION_METRICS_LOGGER_H_
