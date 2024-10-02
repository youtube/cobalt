// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/network_quality_estimator_provider_impl.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"

namespace metrics {

NetworkQualityEstimatorProviderImpl::NetworkQualityEstimatorProviderImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

NetworkQualityEstimatorProviderImpl::~NetworkQualityEstimatorProviderImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (callback_) {
    g_browser_process->network_quality_tracker()
        ->RemoveEffectiveConnectionTypeObserver(this);
  }
}

void NetworkQualityEstimatorProviderImpl::PostReplyOnNetworkQualityChanged(
    base::RepeatingCallback<void(net::EffectiveConnectionType)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!content::BrowserThread::IsThreadInitialized(
          content::BrowserThread::IO)) {
    // IO thread is not yet initialized. Try again in the next message pump.
    bool task_posted =
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&NetworkQualityEstimatorProviderImpl::
                                          PostReplyOnNetworkQualityChanged,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(callback)));
    DCHECK(task_posted);
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(tbansal): https://crbug.com/898304: Tasks posted at BEST_EFFORT
  // may take up to ~20 seconds to execute. Figure out a way to call
  // g_browser_process->network_quality_tracker earlier rather than waiting for
  // BEST_EFFORT to run (which happens sometime after startup is completed)
  content::BrowserThread::PostBestEffortTask(
      FROM_HERE, base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&NetworkQualityEstimatorProviderImpl::
                         AddEffectiveConnectionTypeObserverNow,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return;
#else
  bool task_posted =
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&NetworkQualityEstimatorProviderImpl::
                             AddEffectiveConnectionTypeObserverNow,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  DCHECK(task_posted);
#endif
}

void NetworkQualityEstimatorProviderImpl::AddEffectiveConnectionTypeObserverNow(
    base::RepeatingCallback<void(net::EffectiveConnectionType)> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!callback_);

  callback_ = callback;

  g_browser_process->network_quality_tracker()
      ->AddEffectiveConnectionTypeObserver(this);
}

void NetworkQualityEstimatorProviderImpl::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  callback_.Run(type);
}

}  // namespace metrics
