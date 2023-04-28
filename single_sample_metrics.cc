// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/single_sample_metrics.h"

#include <memory>
#include <utility>

#include "base/metrics/single_sample_metrics.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/single_sample_metrics_factory_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace metrics {

namespace {

class MojoSingleSampleMetric : public mojom::SingleSampleMetric {
 public:
  MojoSingleSampleMetric(const std::string& histogram_name,
                         base::HistogramBase::Sample min,
                         base::HistogramBase::Sample max,
                         uint32_t bucket_count,
                         int32_t flags)
      : metric_(histogram_name, min, max, bucket_count, flags) {}
  ~MojoSingleSampleMetric() override {}

 private:
  // mojom::SingleSampleMetric:
  void SetSample(base::HistogramBase::Sample sample) override {
    metric_.SetSample(sample);
  }

  base::DefaultSingleSampleMetric metric_;

  DISALLOW_COPY_AND_ASSIGN(MojoSingleSampleMetric);
};

class MojoSingleSampleMetricsProvider
    : public mojom::SingleSampleMetricsProvider {
 public:
  MojoSingleSampleMetricsProvider() {}
  ~MojoSingleSampleMetricsProvider() override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

 private:
  // mojom::SingleSampleMetricsProvider:
  void AcquireSingleSampleMetric(
      const std::string& histogram_name,
      base::HistogramBase::Sample min,
      base::HistogramBase::Sample max,
      uint32_t bucket_count,
      int32_t flags,
      mojom::SingleSampleMetricRequest request) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    mojo::MakeStrongBinding(std::make_unique<MojoSingleSampleMetric>(
                                histogram_name, min, max, bucket_count, flags),
                            std::move(request));
  }

  // Providers must be created, used on, and destroyed on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MojoSingleSampleMetricsProvider);
};

}  // namespace

// static
void InitializeSingleSampleMetricsFactory(CreateProviderCB create_provider_cb) {
  base::SingleSampleMetricsFactory::SetFactory(
      std::make_unique<SingleSampleMetricsFactoryImpl>(
          std::move(create_provider_cb)));
}

// static
void CreateSingleSampleMetricsProvider(
    mojom::SingleSampleMetricsProviderRequest request) {
  mojo::MakeStrongBinding(std::make_unique<MojoSingleSampleMetricsProvider>(),
                          std::move(request));
}

}  // namespace metrics
