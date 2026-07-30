/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/client/secagg_runner.h"

#include <memory>

#include "absl/status/status.h"

namespace fcp::client {

namespace {

class DummySecAggRunner : public SecAggRunner {
 public:
  absl::Status Run(ComputationResults results) override {
    return absl::UnimplementedError("Secure aggregation is not supported in Chromium.");
  }
};

}  // namespace

std::unique_ptr<SecAggRunner> SecAggRunnerFactoryImpl::CreateSecAggRunner(
    std::unique_ptr<SecAggSendToServerBase> send_to_server_impl,
    std::unique_ptr<SecAggProtocolDelegate> protocol_delegate,
    SecAggEventPublisher* secagg_event_publisher, LogManager* log_manager,
    InterruptibleRunner* interruptible_runner,
    int64_t expected_number_of_clients,
    int64_t minimum_surviving_clients_for_reconstruction) {
  return std::make_unique<DummySecAggRunner>();
}

}  // namespace fcp::client
