// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/trial_comparison_cert_verifier.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert/trial_comparison_cert_verifier_util.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"

namespace net {

namespace {

base::Value::Dict JobResultParams(bool trial_success) {
  base::Value::Dict results;
  results.Set("trial_success", trial_success);
  return results;
}

}  // namespace

// The Job represents the state machine for a trial cert verification.
// The Job is always owned by the TrialComparisonCertVerifier. However, a
// reference to the Job is given by the CertVerifier::Request returned by
// Start(), allowing the caller to indicate they're no longer interested in
// the Job if it's not yet completed.
//
// The Job may be deleted while processing the initial verification completion,
// by the client callback deleting the associated TrialComparisonCertVerifier.
class TrialComparisonCertVerifier::Job {
 public:
  Job(const CertVerifier::Config& config,
      const CertVerifier::RequestParams& params,
      const NetLogWithSource& source_net_log,
      TrialComparisonCertVerifier* parent);

  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  ~Job();

  // Start the Job, attempting first to verify with the parent's primary
  // verifier. |client_result|, |client_callback|, and |client_request| are
  // the parameters to the TrialComparisonCertVerifier::Verify(), allowing the
  // caller to register interest in the primary results. |client_request| will
  // be filled with a handle that the caller can use to abort the request.
  int Start(CertVerifyResult* client_result,
            CompletionOnceCallback client_callback,
            std::unique_ptr<CertVerifier::Request>* client_request);
  void OnConfigChanged();

 private:
  class Request;
  friend class Request;

  // If the Job has not yet completed the primary verification, this can be
  // called to indicate that the Request is no longer interested (e.g. the
  // Request is being deleted).
  void DetachRequest();

  void Finish(bool is_success, TrialComparisonResult result_code);
  void FinishSuccess(TrialComparisonResult result_code);
  void FinishWithError();

  // Called when the primary verifier is completed.
  // DANGER: |this| may be deleted when calling this.
  void OnPrimaryJobCompleted(int result);

  // Called when the initial trial comparison is completed.
  void OnTrialJobCompleted(int result);

  // The primary (system) and trial (built-in) verifiers may both construct
  // valid chains, but they use different paths. If that happens, a second
  // verification with the system verifier is used, using the path that the
  // built-in verifier constructed, to compare results. This is called when
  // that re-verification completes.
  void OnPrimaryReverifyWithSecondaryChainCompleted(int result);

  const CertVerifier::Config config_;
  bool config_changed_ = false;
  const CertVerifier::RequestParams params_;
  const NetLogWithSource net_log_;

  raw_ptr<TrialComparisonCertVerifier> parent_ = nullptr;  // Non-owned.
  raw_ptr<Request> request_ = nullptr;                     // Non-owned.

  // Results from the primary verification.
  base::TimeTicks primary_start_;
  int primary_error_;
  CertVerifyResult primary_result_;
  std::unique_ptr<CertVerifier::Request> primary_request_;

  // Results from the trial verification.
  base::TimeTicks trial_start_;
  int trial_error_;
  CertVerifyResult trial_result_;
  std::unique_ptr<CertVerifier::Request> trial_request_;

  // Results from the re-verification attempt.
  CertVerifyResult reverification_result_;
  std::unique_ptr<CertVerifier::Request> reverification_request_;

  base::WeakPtrFactory<Job> weak_factory_{this};
};

// The Request is vended to the TrialComparisonCertVerifier::Verify() callers,
// which they fully own and will ultimately destroy. It's used to coordinate
// state with the Job.
//
// If the Job has not yet completed the primary verification request, deleting
// this will abort that Job, ultimately leading to the Job being deleted.
// However, if the primary verification has completed, deleting the Request
// simply becomes a no-op.
class TrialComparisonCertVerifier::Job::Request : public CertVerifier::Request {
 public:
  Request(TrialComparisonCertVerifier::Job* parent,
          CertVerifyResult* client_result,
          CompletionOnceCallback client_callback);

  Request(const Request&) = delete;
  Request& operator=(const Request&) = delete;

  ~Request() override;

  // Called when the Job has completed, and used to invoke the client
  // callback.
  // Note: |this| may be deleted after calling this method.
  void OnJobComplete(int result, const CertVerifyResult& verify_result);

  // Called when the Job is aborted (e.g. the underlying
  // TrialComparisonCertVerifier is being deleted).
  // Note: |this| may be deleted after calling this method.
  void OnJobAborted();

 private:
  raw_ptr<TrialComparisonCertVerifier::Job> parent_;
  raw_ptr<CertVerifyResult> client_result_;
  CompletionOnceCallback client_callback_;
};

TrialComparisonCertVerifier::Job::Job(const CertVerifier::Config& config,
                                      const CertVerifier::RequestParams& params,
                                      const NetLogWithSource& source_net_log,
                                      TrialComparisonCertVerifier* parent)
    : config_(config),
      params_(params),
      net_log_(
          NetLogWithSource::Make(source_net_log.net_log(),
                                 NetLogSourceType::TRIAL_CERT_VERIFIER_JOB)),
      parent_(parent) {
  net_log_.BeginEvent(NetLogEventType::TRIAL_CERT_VERIFIER_JOB);
  source_net_log.AddEventReferencingSource(
      NetLogEventType::TRIAL_CERT_VERIFIER_JOB_COMPARISON_STARTED,
      net_log_.source());
}

TrialComparisonCertVerifier::Job::~Job() {
  if (request_) {
    // Note: May delete |request_|.
    request_->OnJobAborted();
    request_ = nullptr;
  }

  if (parent_) {
    net_log_.AddEvent(NetLogEventType::CANCELLED);
    net_log_.EndEvent(NetLogEventType::TRIAL_CERT_VERIFIER_JOB);
  }
}

int TrialComparisonCertVerifier::Job::Start(
    CertVerifyResult* client_result,
    CompletionOnceCallback client_callback,
    std::unique_ptr<CertVerifier::Request>* client_request) {
  DCHECK(!request_);
  DCHECK(parent_);

  primary_start_ = base::TimeTicks::Now();

  // Unretained is safe because |primary_request_| will cancel the
  // callback on destruction.
  primary_error_ = parent_->primary_verifier()->Verify(
      params_, &primary_result_,
      base::BindOnce(&Job::OnPrimaryJobCompleted, base::Unretained(this)),
      &primary_request_, net_log_);

  if (primary_error_ != ERR_IO_PENDING) {
    *client_result = primary_result_;
    int result = primary_error_;

    // NOTE: |this| may be deleted here, in the event that every resulting
    // trial comparison also completes synchronously.
    OnPrimaryJobCompleted(result);
    return result;
  }

  // Create a new Request that will be used to manage the state for the
  // primary verification and allow cancellation.
  auto request = std::make_unique<Request>(this, client_result,
                                           std::move(client_callback));
  request_ = request.get();
  *client_request = std::move(request);
  return ERR_IO_PENDING;
}

void TrialComparisonCertVerifier::Job::OnConfigChanged() {
  config_changed_ = true;
}

void TrialComparisonCertVerifier::Job::DetachRequest() {
  // This should only be called while waiting for the primary verification.
  DCHECK(primary_request_);
  DCHECK(request_);

  request_ = nullptr;
}

void TrialComparisonCertVerifier::Job::Finish(
    bool is_success,
    TrialComparisonResult result_code) {
  // There should never be a pending initial verification.
  DCHECK(!request_);
  DCHECK(!primary_request_);

  UMA_HISTOGRAM_ENUMERATION("Net.CertVerifier_TrialComparisonResult",
                            result_code);

  net_log_.EndEvent(NetLogEventType::TRIAL_CERT_VERIFIER_JOB,
                    [&] { return JobResultParams(is_success); });

  // Reset |parent_| to indicate the Job successfully completed (i.e. it was
  // not deleted by the TrialComparisonCertVerifier while still waiting for
  // results).
  TrialComparisonCertVerifier* parent = parent_;
  parent_ = nullptr;

  // Invoking the report callback may result in the
  // TrialComparisonCertVerifier being deleted, which will delete this Job.
  // Guard against this by grabbing a WeakPtr to |this|.
  base::WeakPtr<Job> weak_this = weak_factory_.GetWeakPtr();
  if (!is_success) {
    parent->report_callback_.Run(
        params_.hostname(), params_.certificate(), config_.enable_rev_checking,
        config_.require_rev_checking_local_anchors,
        config_.enable_sha1_local_anchors, config_.disable_symantec_enforcement,
        params_.ocsp_response(), params_.sct_list(), primary_result_,
        trial_result_);
  }

  if (weak_this) {
    // If the Job is still alive, delete it now.
    parent->RemoveJob(this);
    return;
  }
}

void TrialComparisonCertVerifier::Job::FinishSuccess(
    TrialComparisonResult result_code) {
  Finish(/*is_success=*/true, result_code);
}

void TrialComparisonCertVerifier::Job::FinishWithError() {
  DCHECK(trial_error_ != primary_error_ ||
         !CertVerifyResultEqual(trial_result_, primary_result_));

  TrialComparisonResult result_code = TrialComparisonResult::kInvalid;

  if (primary_error_ == OK && trial_error_ == OK) {
    result_code = TrialComparisonResult::kBothValidDifferentDetails;
  } else if (primary_error_ == OK) {
    result_code = TrialComparisonResult::kPrimaryValidSecondaryError;
  } else if (trial_error_ == OK) {
    result_code = TrialComparisonResult::kPrimaryErrorSecondaryValid;
  } else {
    result_code = TrialComparisonResult::kBothErrorDifferentDetails;
  }
  Finish(/*is_success=*/false, result_code);
}

void TrialComparisonCertVerifier::Job::OnPrimaryJobCompleted(int result) {
  base::TimeDelta primary_latency = base::TimeTicks::Now() - primary_start_;

  primary_error_ = result;
  primary_request_.reset();

  // Notify the original requestor that the primary verification has now
  // completed. This may result in |this| being deleted (if the associated
  // TrialComparisonCertVerifier is deleted); to detect this situation, grab
  // a WeakPtr to |this|.
  base::WeakPtr<Job> weak_this = weak_factory_.GetWeakPtr();
  if (request_) {
    Request* request = request_;
    request_ = nullptr;

    // Note: May delete |this|.
    request->OnJobComplete(primary_error_, primary_result_);
  }

  if (!weak_this)
    return;

  if (config_changed_ || !parent_->trial_allowed()) {
    // If the trial will not be run, then delete |this|.
    parent_->RemoveJob(this);
    return;
  }

  // Only record the TrialPrimary histograms for the same set of requests
  // that TrialSecondary histograms will be recorded for, in order to get a
  // direct comparison.
  UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_Job_Latency_TrialPrimary",
                             primary_latency, base::Milliseconds(1),
                             base::Minutes(10), 100);

  trial_start_ = base::TimeTicks::Now();
  int rv = parent_->trial_verifier()->Verify(
      params_, &trial_result_,
      base::BindOnce(&Job::OnTrialJobCompleted, base::Unretained(this)),
      &trial_request_, net_log_);
  if (rv != ERR_IO_PENDING)
    OnTrialJobCompleted(rv);  // Note: May delete |this|.
}

void TrialComparisonCertVerifier::Job::OnTrialJobCompleted(int result) {
  DCHECK(primary_result_.verified_cert);
  DCHECK(trial_result_.verified_cert);

  base::TimeDelta latency = base::TimeTicks::Now() - trial_start_;
  trial_error_ = result;

  UMA_HISTOGRAM_CUSTOM_TIMES("Net.CertVerifier_Job_Latency_TrialSecondary",
                             latency, base::Milliseconds(1), base::Minutes(10),
                             100);

  bool errors_equal = trial_error_ == primary_error_;
  bool details_equal = CertVerifyResultEqual(trial_result_, primary_result_);
  bool trial_success = errors_equal && details_equal;

  if (trial_success) {
    // Note: Will delete |this|.
    FinishSuccess(TrialComparisonResult::kEqual);
    return;
  }

  TrialComparisonResult ignorable_difference =
      IsSynchronouslyIgnorableDifference(primary_error_, primary_result_,
                                         trial_error_, trial_result_,
                                         config_.enable_sha1_local_anchors);
  if (ignorable_difference != TrialComparisonResult::kInvalid) {
    FinishSuccess(ignorable_difference);  // Note: Will delete |this|.
    return;
  }

  const bool chains_equal = primary_result_.verified_cert->EqualsIncludingChain(
      trial_result_.verified_cert.get());

  if (!chains_equal && (trial_error_ == OK || primary_error_ != OK)) {
    if (config_changed_) {
      // Note: Will delete |this|.
      FinishSuccess(TrialComparisonResult::kIgnoredConfigurationChanged);
      return;
    }

    // Chains were different, reverify the trial_result_.verified_cert chain
    // using the platform verifier and compare results again.
    RequestParams reverification_params(
        trial_result_.verified_cert, params_.hostname(), params_.flags(),
        params_.ocsp_response(), params_.sct_list());

    int rv = parent_->primary_reverifier()->Verify(
        reverification_params, &reverification_result_,
        base::BindOnce(&Job::OnPrimaryReverifyWithSecondaryChainCompleted,
                       base::Unretained(this)),
        &reverification_request_, net_log_);
    if (rv != ERR_IO_PENDING) {
      // Note: May delete |this|.
      OnPrimaryReverifyWithSecondaryChainCompleted(rv);
    }
    return;
  }

  FinishWithError();  // Note: Will delete |this|.
}

void TrialComparisonCertVerifier::Job::
    OnPrimaryReverifyWithSecondaryChainCompleted(int result) {
  if (result == trial_error_ &&
      CertVerifyResultEqual(reverification_result_, trial_result_)) {
    // The new result matches the builtin verifier, so this was just a
    // difference in the platform's path-building ability.
    // Ignore the difference.
    //
    // Note: Will delete |this|.
    FinishSuccess(
        TrialComparisonResult::kIgnoredDifferentPathReVerifiesEquivalent);
    return;
  }

  if (IsSynchronouslyIgnorableDifference(result, reverification_result_,
                                         trial_error_, trial_result_,
                                         config_.enable_sha1_local_anchors) !=
      TrialComparisonResult::kInvalid) {
    // The new result matches if ignoring differences. Still use the
    // |kIgnoredDifferentPathReVerifiesEquivalent| code rather than the result
    // of IsSynchronouslyIgnorableDifference, since it's the higher level
    // description of what the difference is in this case.
    //
    // Note: Will delete |this|.
    FinishSuccess(
        TrialComparisonResult::kIgnoredDifferentPathReVerifiesEquivalent);
    return;
  }

  // Note: Will delete |this|.
  FinishWithError();
}

TrialComparisonCertVerifier::Job::Request::Request(
    TrialComparisonCertVerifier::Job* parent,
    CertVerifyResult* client_result,
    CompletionOnceCallback client_callback)
    : parent_(parent),
      client_result_(client_result),
      client_callback_(std::move(client_callback)) {}

TrialComparisonCertVerifier::Job::Request::~Request() {
  if (parent_)
    parent_->DetachRequest();
}

void TrialComparisonCertVerifier::Job::Request::OnJobComplete(
    int result,
    const CertVerifyResult& verify_result) {
  DCHECK(parent_);
  parent_ = nullptr;

  *client_result_ = verify_result;

  // DANGER: |this| may be deleted when this callback is run (as well as
  // |parent_|, but that's been reset above).
  std::move(client_callback_).Run(result);
}

void TrialComparisonCertVerifier::Job::Request::OnJobAborted() {
  DCHECK(parent_);
  parent_ = nullptr;

  // DANGER: |this| may be deleted when this callback is destroyed.
  client_callback_.Reset();
}

TrialComparisonCertVerifier::TrialComparisonCertVerifier(
    scoped_refptr<CertVerifyProcFactory> verify_proc_factory,
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    const CertVerifyProcFactory::ImplParams& impl_params,
    ReportCallback report_callback)
    : report_callback_(std::move(report_callback)) {
  auto [primary_impl_params, trial_impl_params] =
      ProcessImplParams(impl_params);

  primary_verifier_ = std::make_unique<MultiThreadedCertVerifier>(
      verify_proc_factory->CreateCertVerifyProc(cert_net_fetcher,
                                                primary_impl_params),
      verify_proc_factory);

  primary_reverifier_ = std::make_unique<MultiThreadedCertVerifier>(
      verify_proc_factory->CreateCertVerifyProc(cert_net_fetcher,
                                                primary_impl_params),
      verify_proc_factory);

  trial_verifier_ = std::make_unique<MultiThreadedCertVerifier>(
      verify_proc_factory->CreateCertVerifyProc(cert_net_fetcher,
                                                trial_impl_params),
      verify_proc_factory);

  primary_verifier_->AddObserver(this);
  primary_reverifier_->AddObserver(this);
  trial_verifier_->AddObserver(this);
}

TrialComparisonCertVerifier::~TrialComparisonCertVerifier() {
  primary_verifier_->RemoveObserver(this);
  primary_reverifier_->RemoveObserver(this);
  trial_verifier_->RemoveObserver(this);
}

int TrialComparisonCertVerifier::Verify(const RequestParams& params,
                                        CertVerifyResult* verify_result,
                                        CompletionOnceCallback callback,
                                        std::unique_ptr<Request>* out_req,
                                        const NetLogWithSource& net_log) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Technically, the trial could still be active when
  // actual_use_chrome_root_store_=true, just by reversing everything. (Doing
  // the trial verifier first and returning that result, then doing the primary
  // verifier and comparing.) Probably not worth the trouble though.
  if (!trial_allowed() || actual_use_chrome_root_store_) {
    if (actual_use_chrome_root_store_) {
      return trial_verifier_->Verify(params, verify_result, std::move(callback),
                                     out_req, net_log);
    } else {
      return primary_verifier_->Verify(params, verify_result,
                                       std::move(callback), out_req, net_log);
    }
  }

  std::unique_ptr<Job> job =
      std::make_unique<Job>(config_, params, net_log, this);
  Job* job_ptr = job.get();
  jobs_.insert(std::move(job));

  return job_ptr->Start(verify_result, std::move(callback), out_req);
}

void TrialComparisonCertVerifier::SetConfig(const Config& config) {
  config_ = config;

  primary_verifier_->SetConfig(config);
  primary_reverifier_->SetConfig(config);
  trial_verifier_->SetConfig(config);

  // Notify all in-process jobs that the underlying configuration has changed.
  NotifyJobsOfConfigChange();
}

void TrialComparisonCertVerifier::AddObserver(
    CertVerifier::Observer* observer) {
  // Delegate the notifications to the wrapped verifiers. We add the observer
  // to both `primary_verifier_` and `trial_verifier_` since either one might
  // be the one that matters depending on the configuration at the time. This
  // does mean the caller will get double notified, but shouldn't really matter
  // that much. It would be possible to avoid this by making a notification
  // proxy that only forwards notifications from the verifier that is currently
  // the "main" one, but it's probably not worth the trouble.
  //
  // Also this assumes that there is never a case where the
  // TrialComparisonCertVerifier itself would change something without the
  // wrapped verifiers also generating a notification.
  primary_verifier_->AddObserver(observer);
  trial_verifier_->AddObserver(observer);
}

void TrialComparisonCertVerifier::RemoveObserver(
    CertVerifier::Observer* observer) {
  trial_verifier_->RemoveObserver(observer);
  primary_verifier_->RemoveObserver(observer);
}

void TrialComparisonCertVerifier::UpdateVerifyProcData(
    scoped_refptr<CertNetFetcher> cert_net_fetcher,
    const CertVerifyProcFactory::ImplParams& impl_params) {
  bool previous_actual_use_chrome_root_store = actual_use_chrome_root_store_;
  auto [primary_impl_params, trial_impl_params] =
      ProcessImplParams(impl_params);

  // If the only change in the params was to switch the
  // `actual_use_chrome_root_store_` value, updating the underlying verifiers
  // is unnecessary, but it's probably not worth the trouble to try to avoid
  // it.
  primary_verifier_->UpdateVerifyProcData(cert_net_fetcher,
                                          primary_impl_params);
  primary_reverifier_->UpdateVerifyProcData(cert_net_fetcher,
                                            primary_impl_params);

  trial_verifier_->UpdateVerifyProcData(cert_net_fetcher, trial_impl_params);

  // The TrialComparisonCertVerifier is registered as an observer of the
  // underlying verifiers, and so the OnCertVerifierChanged method should be
  // triggered to call NotifyJobsOfConfigChange, so it isn't explicitly called
  // here for cases other than the `actual_use_chrome_root_store_` changing.

  if (previous_actual_use_chrome_root_store != actual_use_chrome_root_store_) {
    NotifyJobsOfConfigChange();
  }
}

void TrialComparisonCertVerifier::RemoveJob(Job* job_ptr) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = jobs_.find(job_ptr);
  DCHECK(it != jobs_.end());
  jobs_.erase(it);
}

std::tuple<CertVerifyProcFactory::ImplParams, CertVerifyProcFactory::ImplParams>
TrialComparisonCertVerifier::ProcessImplParams(
    const CertVerifyProcFactory::ImplParams& impl_params) {
  actual_use_chrome_root_store_ = impl_params.use_chrome_root_store;

  CertVerifyProcFactory::ImplParams primary_impl_params(impl_params);
  primary_impl_params.use_chrome_root_store = false;
  CertVerifyProcFactory::ImplParams trial_impl_params(impl_params);
  trial_impl_params.use_chrome_root_store = true;
  return {std::move(primary_impl_params), std::move(trial_impl_params)};
}

void TrialComparisonCertVerifier::NotifyJobsOfConfigChange() {
  for (auto& job : jobs_) {
    job->OnConfigChanged();
  }
}

void TrialComparisonCertVerifier::OnCertVerifierChanged() {
  NotifyJobsOfConfigChange();
}

}  // namespace net
