// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/metrics_reporting_observer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"

class MetricsServiceProxyImpl
    : public MetricsReportingObserver::MetricsServiceProxy {
 public:
  explicit MetricsServiceProxyImpl(metrics::MetricsService* metrics_service)
      : metrics_service_(metrics_service) {}

  MetricsServiceProxyImpl(const MetricsServiceProxyImpl&) = delete;
  MetricsServiceProxyImpl& operator=(const MetricsReportingObserver&) = delete;

  ~MetricsServiceProxyImpl() override = default;

  void SetReportingEnabled(bool enabled) override {
    ChangeMetricsReportingState(enabled);
  }

  void SetExternalClientId(const std::string& client_id) override {
    metrics_service_->SetExternalClientId(client_id);
  }

  void RecreateClientIdIfNecessary() override {
    if (metrics_service_->IsMetricsReportingEnabled())
      metrics_service_->ResetClientId();
  }

 private:
  raw_ptr<metrics::MetricsService, DanglingUntriaged> metrics_service_;
};

std::unique_ptr<MetricsReportingObserver>
MetricsReportingObserver::CreateObserver(
    metrics::MetricsService* metrics_service) {
  return std::make_unique<MetricsReportingObserver>(
      std::make_unique<MetricsServiceProxyImpl>(metrics_service));
}

MetricsReportingObserver::MetricsReportingObserver(
    std::unique_ptr<MetricsServiceProxy> metrics_service)
    : metrics_service_(std::move(metrics_service)) {
  DCHECK(metrics_service_);
  auto* lacros_service = chromeos::LacrosService::Get();

  if (lacros_service->IsMetricsReportingAvailable()) {
    lacros_service->BindMetricsReporting(
        metrics_reporting_remote_.BindNewPipeAndPassReceiver());
    metrics_reporting_remote_->AddObserver(
        receiver_.BindNewPipeAndPassRemoteWithVersion());
  }
}

MetricsReportingObserver::~MetricsReportingObserver() = default;

void MetricsReportingObserver::InitSettingsFromAsh() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsMetricsReportingAvailable()) {
    LOG(WARNING) << "MetricsReporting API not available";
    return;
  }

  // Set the initial state.
  ChangeMetricsReportingState(
      chromeos::BrowserParamsProxy::Get()->AshMetricsEnabled());
}

void MetricsReportingObserver::OnMetricsReportingChanged(
    bool enabled,
    const absl::optional<std::string>& client_id) {
  if (enabled) {
    if (client_id) {
      metrics_service_->SetExternalClientId(client_id.value());
    } else {
      LOG(WARNING) << "UMA client id should have been provided, but wasn't. "
                      "A random id will be used.";
    }
  }
  metrics_service_->SetReportingEnabled(enabled);
  if (enabled) {
    metrics_service_->RecreateClientIdIfNecessary();
  }
}
