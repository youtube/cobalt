// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/trial_comparison_cert_verifier.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/trial_comparison_cert_verifier_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;

namespace net {

namespace {

MATCHER_P(CertChainMatches, expected_cert, "") {
  CertificateList actual_certs =
      X509Certificate::CreateCertificateListFromBytes(
          base::as_bytes(base::make_span(arg)),
          X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  if (actual_certs.empty()) {
    *result_listener << "failed to parse arg";
    return false;
  }
  std::vector<std::string> actual_der_certs;
  for (const auto& cert : actual_certs) {
    actual_der_certs.emplace_back(
        x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()));
  }

  std::vector<std::string> expected_der_certs;
  expected_der_certs.emplace_back(
      x509_util::CryptoBufferAsStringPiece(expected_cert->cert_buffer()));
  for (const auto& buffer : expected_cert->intermediate_buffers()) {
    expected_der_certs.emplace_back(
        x509_util::CryptoBufferAsStringPiece(buffer.get()));
  }

  return actual_der_certs == expected_der_certs;
}

// Like TestClosure, but handles multiple closure.Run()/WaitForResult()
// calls correctly regardless of ordering.
class RepeatedTestClosure {
 public:
  RepeatedTestClosure()
      : closure_(base::BindRepeating(&RepeatedTestClosure::DidSetResult,
                                     base::Unretained(this))) {}

  const base::RepeatingClosure& closure() const { return closure_; }

  void WaitForResult() {
    DCHECK(!run_loop_);
    if (!have_result_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
      run_loop_.reset();
      DCHECK(have_result_);
    }
    have_result_--;  // Auto-reset for next callback.
  }

 private:
  void DidSetResult() {
    have_result_++;
    if (run_loop_)
      run_loop_->Quit();
  }

  // RunLoop.  Only non-NULL during the call to WaitForResult, so the class is
  // reusable.
  std::unique_ptr<base::RunLoop> run_loop_;

  unsigned int have_result_ = 0;

  base::RepeatingClosure closure_;
};

// Fake CertVerifyProc that sets the CertVerifyResult to a given value for
// all certificates that are Verify()'d
class FakeCertVerifyProc : public CertVerifyProc {
 public:
  FakeCertVerifyProc(const int result_error, const CertVerifyResult& result)
      : CertVerifyProc(CRLSet::BuiltinCRLSet()),
        result_error_(result_error),
        result_(result),
        main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

  FakeCertVerifyProc(const FakeCertVerifyProc&) = delete;
  FakeCertVerifyProc& operator=(const FakeCertVerifyProc&) = delete;

  void WaitForVerifyCall() { verify_called_.WaitForResult(); }
  int num_verifications() const { return num_verifications_; }

  // CertVerifyProc implementation:
  bool SupportsAdditionalTrustAnchors() const override { return false; }

 protected:
  ~FakeCertVerifyProc() override = default;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     const CertificateList& additional_trust_anchors,
                     CertVerifyResult* verify_result,
                     const NetLogWithSource& net_log) override;

  // Runs on the main thread
  void VerifyCalled();

  const int result_error_;
  const CertVerifyResult result_;
  int num_verifications_ = 0;
  RepeatedTestClosure verify_called_;
  scoped_refptr<base::TaskRunner> main_task_runner_;
};

int FakeCertVerifyProc::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result,
    const NetLogWithSource& net_log) {
  *verify_result = result_;
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&FakeCertVerifyProc::VerifyCalled, this));
  return result_error_;
}

void FakeCertVerifyProc::VerifyCalled() {
  ++num_verifications_;
  verify_called_.closure().Run();
}

// Fake CertVerifyProc that causes a failure if it is called.
class NotCalledCertVerifyProc : public CertVerifyProc {
 public:
  NotCalledCertVerifyProc() : CertVerifyProc(CRLSet::BuiltinCRLSet()) {}
  NotCalledCertVerifyProc(const NotCalledCertVerifyProc&) = delete;
  NotCalledCertVerifyProc& operator=(const NotCalledCertVerifyProc&) = delete;

  // CertVerifyProc implementation:
  bool SupportsAdditionalTrustAnchors() const override { return false; }

 protected:
  ~NotCalledCertVerifyProc() override = default;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     const CertificateList& additional_trust_anchors,
                     CertVerifyResult* verify_result,
                     const NetLogWithSource& net_log) override;
};

int NotCalledCertVerifyProc::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result,
    const NetLogWithSource& net_log) {
  ADD_FAILURE() << "NotCalledCertVerifyProc was called!";
  return ERR_UNEXPECTED;
}

scoped_refptr<CertVerifyProc> MakeNotCalledProc() {
  return base::MakeRefCounted<NotCalledCertVerifyProc>();
}

void NotCalledCallback(int error) {
  ADD_FAILURE() << "NotCalledCallback was called with error code " << error;
}

class MockCertVerifyProc : public CertVerifyProc {
 public:
  MockCertVerifyProc() : CertVerifyProc(CRLSet::BuiltinCRLSet()) {}

  MockCertVerifyProc(const MockCertVerifyProc&) = delete;
  MockCertVerifyProc& operator=(const MockCertVerifyProc&) = delete;

  // CertVerifyProc implementation:
  bool SupportsAdditionalTrustAnchors() const override { return false; }
  MOCK_METHOD8(VerifyInternal,
               int(X509Certificate* cert,
                   const std::string& hostname,
                   const std::string& ocsp_response,
                   const std::string& sct_list,
                   int flags,
                   const CertificateList& additional_trust_anchors,
                   CertVerifyResult* verify_result,
                   const NetLogWithSource& net_log));

 protected:
  ~MockCertVerifyProc() override = default;
};

class TestProcFactory : public CertVerifyProcFactory {
 public:
  explicit TestProcFactory(
      std::deque<scoped_refptr<CertVerifyProc>> primary_verify_procs,
      std::deque<scoped_refptr<CertVerifyProc>> trial_verify_procs)
      : primary_verify_procs_(std::move(primary_verify_procs)),
        trial_verify_procs_(std::move(trial_verify_procs)) {}

  scoped_refptr<net::CertVerifyProc> CreateCertVerifyProc(
      scoped_refptr<CertNetFetcher> cert_net_fetcher,
      const ImplParams& impl_params) override {
    std::deque<scoped_refptr<CertVerifyProc>>* procs =
        impl_params.use_chrome_root_store ? &trial_verify_procs_
                                          : &primary_verify_procs_;
    if (procs->empty()) {
      ADD_FAILURE() << "procs is empty for crs="
                    << impl_params.use_chrome_root_store;
      return MakeNotCalledProc();
    }
    scoped_refptr<CertVerifyProc> r = procs->front();
    procs->pop_front();
    return r;
  }

 protected:
  ~TestProcFactory() override = default;

  std::deque<scoped_refptr<CertVerifyProc>> primary_verify_procs_;
  std::deque<scoped_refptr<CertVerifyProc>> trial_verify_procs_;
};

scoped_refptr<CertVerifyProcFactory> ProcFactory(
    std::deque<scoped_refptr<CertVerifyProc>> primary_verify_procs,
    std::deque<scoped_refptr<CertVerifyProc>> trial_verify_procs) {
  return base::MakeRefCounted<TestProcFactory>(std::move(primary_verify_procs),
                                               std::move(trial_verify_procs));
}

struct TrialReportInfo {
  TrialReportInfo(const std::string& hostname,
                  const scoped_refptr<X509Certificate>& unverified_cert,
                  bool enable_rev_checking,
                  bool require_rev_checking_local_anchors,
                  bool enable_sha1_local_anchors,
                  bool disable_symantec_enforcement,
                  const std::string& stapled_ocsp,
                  const std::string& sct_list,
                  const CertVerifyResult& primary_result,
                  const CertVerifyResult& trial_result)
      : hostname(hostname),
        unverified_cert(unverified_cert),
        enable_rev_checking(enable_rev_checking),
        require_rev_checking_local_anchors(require_rev_checking_local_anchors),
        enable_sha1_local_anchors(enable_sha1_local_anchors),
        disable_symantec_enforcement(disable_symantec_enforcement),
        stapled_ocsp(stapled_ocsp),
        sct_list(sct_list),
        primary_result(primary_result),
        trial_result(trial_result) {}

  std::string hostname;
  scoped_refptr<X509Certificate> unverified_cert;
  bool enable_rev_checking;
  bool require_rev_checking_local_anchors;
  bool enable_sha1_local_anchors;
  bool disable_symantec_enforcement;
  std::string stapled_ocsp;
  std::string sct_list;
  CertVerifyResult primary_result;
  CertVerifyResult trial_result;
};

void RecordTrialReport(std::vector<TrialReportInfo>* reports,
                       const std::string& hostname,
                       const scoped_refptr<X509Certificate>& unverified_cert,
                       bool enable_rev_checking,
                       bool require_rev_checking_local_anchors,
                       bool enable_sha1_local_anchors,
                       bool disable_symantec_enforcement,
                       const std::string& stapled_ocsp,
                       const std::string& sct_list,
                       const CertVerifyResult& primary_result,
                       const CertVerifyResult& trial_result) {
  TrialReportInfo report(hostname, unverified_cert, enable_rev_checking,
                         require_rev_checking_local_anchors,
                         enable_sha1_local_anchors,
                         disable_symantec_enforcement, stapled_ocsp, sct_list,
                         primary_result, trial_result);
  reports->push_back(report);
}

}  // namespace

class TrialComparisonCertVerifierTest : public TestWithTaskEnvironment {
  void SetUp() override {
    cert_chain_1_ = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), "multi-root-chain1.pem",
        X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(cert_chain_1_);
    leaf_cert_1_ = X509Certificate::CreateFromBuffer(
        bssl::UpRef(cert_chain_1_->cert_buffer()), {});
    ASSERT_TRUE(leaf_cert_1_);
    cert_chain_2_ = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), "multi-root-chain2.pem",
        X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(cert_chain_2_);

    lets_encrypt_dst_x3_ = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), "lets-encrypt-dst-x3-root.pem",
        X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(lets_encrypt_dst_x3_);
    lets_encrypt_isrg_x1_ = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), "lets-encrypt-isrg-x1-root.pem",
        X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(lets_encrypt_isrg_x1_);

    no_crs_impl_params_.use_chrome_root_store = false;
    yes_crs_impl_params_.use_chrome_root_store = true;
  }

 protected:
  scoped_refptr<X509Certificate> cert_chain_1_;
  scoped_refptr<X509Certificate> cert_chain_2_;
  scoped_refptr<X509Certificate> leaf_cert_1_;
  scoped_refptr<X509Certificate> lets_encrypt_dst_x3_;
  scoped_refptr<X509Certificate> lets_encrypt_isrg_x1_;
  net::CertVerifyProcFactory::ImplParams no_crs_impl_params_;
  net::CertVerifyProcFactory::ImplParams yes_crs_impl_params_;
  base::HistogramTester histograms_;
};

TEST_F(TrialComparisonCertVerifierTest, ObserverIsCalledOnVerifierUpdate) {
  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({MakeNotCalledProc(), MakeNotCalledProc(),
                   MakeNotCalledProc(), MakeNotCalledProc()},
                  {MakeNotCalledProc(), MakeNotCalledProc()}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));

  CertVerifierObserverCounter observer_(&verifier);
  EXPECT_EQ(observer_.change_count(), 0u);
  verifier.UpdateVerifyProcData(nullptr, no_crs_impl_params_);
  // Observer is called twice since the TrialComparisonCertVerifier currently
  // forwards notifications from both the primary and secondary verifiers.
  EXPECT_EQ(observer_.change_count(), 2u);
}

TEST_F(TrialComparisonCertVerifierTest, InitiallyDisallowed) {
  CertVerifyResult dummy_result;
  dummy_result.verified_cert = cert_chain_1_;

  auto verify_proc = base::MakeRefCounted<FakeCertVerifyProc>(OK, dummy_result);
  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc, MakeNotCalledProc()}, {MakeNotCalledProc()}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  // Primary verifier should have ran, trial verifier should not have.
  EXPECT_EQ(1, verify_proc->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, InitiallyDisallowedThenAllowed) {
  // Certificate that has multiple subjectAltName entries. This allows easily
  // confirming which verification attempt the report was generated for without
  // having to mock different CertVerifyProc results for each.
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII("many-names");
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "ok-all-types.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  scoped_refptr<X509Certificate> leaf = X509Certificate::CreateFromBuffer(
      bssl::UpRef(cert_chain->cert_buffer()), {});
  ASSERT_TRUE(leaf);

  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier returns an error status.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));

  CertVerifier::RequestParams params(leaf, "t0.test", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  // Enable the trial and do another verification.
  verifier.set_trial_allowed(true);
  CertVerifier::RequestParams params2(leaf, "t1.test", /*flags=*/0,
                                      /*ocsp_response=*/std::string(),
                                      /*sct_list=*/std::string());
  CertVerifyResult result2;
  TestCompletionCallback callback2;
  std::unique_ptr<CertVerifier::Request> request2;
  error = verifier.Verify(params2, &result2, callback2.callback(), &request2,
                          NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  error = callback2.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Primary verifier should have run twice, trial verifier should run once.
  EXPECT_EQ(2, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryValidSecondaryError, 1);

  // Expect a report from the second verification.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];
  EXPECT_EQ("t1.test", report.hostname);
}

TEST_F(TrialComparisonCertVerifierTest, InitiallyAllowedThenDisallowed) {
  // Certificate that has multiple subjectAltName entries. This allows easily
  // confirming which verification attempt the report was generated for without
  // having to mock different CertVerifyProc results for each.
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII("many-names");
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "ok-all-types.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  scoped_refptr<X509Certificate> leaf = X509Certificate::CreateFromBuffer(
      bssl::UpRef(cert_chain->cert_buffer()), {});
  ASSERT_TRUE(leaf);

  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier returns an error status.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf, "t0.test", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  // Disable the trial and do another verification.
  verifier.set_trial_allowed(false);
  CertVerifier::RequestParams params2(leaf, "t1.test", /*flags=*/0,
                                      /*ocsp_response=*/std::string(),
                                      /*sct_list=*/std::string());
  CertVerifyResult result2;
  TestCompletionCallback callback2;
  std::unique_ptr<CertVerifier::Request> request2;
  error = verifier.Verify(params2, &result2, callback2.callback(), &request2,
                          NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  error = callback2.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Primary verifier should have run twice, trial verifier should run once.
  EXPECT_EQ(2, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryValidSecondaryError, 1);

  // Expect a report from the first verification.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];
  EXPECT_EQ("t0.test", report.hostname);
}

TEST_F(TrialComparisonCertVerifierTest, InitiallyCRSEnabledThenDisabled) {
  // Certificate that has multiple subjectAltName entries. This allows easily
  // confirming which verification attempt the report was generated for without
  // having to mock different CertVerifyProc results for each.
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII("many-names");
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "ok-all-types.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  scoped_refptr<X509Certificate> leaf = X509Certificate::CreateFromBuffer(
      bssl::UpRef(cert_chain->cert_buffer()), {});
  ASSERT_TRUE(leaf);

  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier returns an error status.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  // Verifier created with ImplParams that have use_chrome_root_store=true.
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc(), verify_proc1,
                   MakeNotCalledProc()},
                  {verify_proc2, verify_proc2}),
      nullptr, yes_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf, "t0.test", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  error = callback.WaitForResult();
  // The actual result returned should be from the secondary verifier.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));

  // Turn chrome root store off and verify again.
  verifier.UpdateVerifyProcData(nullptr, no_crs_impl_params_);
  CertVerifier::RequestParams params2(leaf, "t1.test", /*flags=*/0,
                                      /*ocsp_response=*/std::string(),
                                      /*sct_list=*/std::string());
  CertVerifyResult result2;
  TestCompletionCallback callback2;
  std::unique_ptr<CertVerifier::Request> request2;
  error = verifier.Verify(params2, &result2, callback2.callback(), &request2,
                          NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  // The actual result returned should now be from the primary verifier.
  error = callback2.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Primary verifier should have run once, secondary verifier should run twice.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(2, verify_proc2->num_verifications());
  // Trial comparison was only run once.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryValidSecondaryError, 1);

  // Expect a report from the second verification.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];
  EXPECT_EQ("t1.test", report.hostname);
}

TEST_F(TrialComparisonCertVerifierTest, InitiallyAllowedThenCRSEnabled) {
  // Certificate that has multiple subjectAltName entries. This allows easily
  // confirming which verification attempt the report was generated for without
  // having to mock different CertVerifyProc results for each.
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII("many-names");
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "ok-all-types.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  scoped_refptr<X509Certificate> leaf = X509Certificate::CreateFromBuffer(
      bssl::UpRef(cert_chain->cert_buffer()), {});
  ASSERT_TRUE(leaf);

  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier returns an error status.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc(), verify_proc1,
                   MakeNotCalledProc()},
                  {verify_proc2, verify_proc2}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf, "t0.test", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);
  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  // Turn chrome root store on and verify again.
  verifier.UpdateVerifyProcData(nullptr, yes_crs_impl_params_);
  CertVerifier::RequestParams params2(leaf, "t1.test", /*flags=*/0,
                                      /*ocsp_response=*/std::string(),
                                      /*sct_list=*/std::string());
  CertVerifyResult result2;
  TestCompletionCallback callback2;
  std::unique_ptr<CertVerifier::Request> request2;
  error = verifier.Verify(params2, &result2, callback2.callback(), &request2,
                          NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request2);

  // The actual result returned should now be from the secondary verifier.
  error = callback2.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Primary verifier should have run once, secondary verifier should run twice.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(2, verify_proc2->num_verifications());
  // Trial comparison was only run once.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryValidSecondaryError, 1);

  // Expect a report from the first verification.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];
  EXPECT_EQ("t0.test", report.hostname);
}

TEST_F(TrialComparisonCertVerifierTest,
       ConfigChangedDuringPrimaryVerification) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {MakeNotCalledProc()}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Change the verifier config before the primary verification finishes.
  CertVerifier::Config config;
  config.enable_sha1_local_anchors = true;
  verifier.SetConfig(config);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  RunUntilIdle();

  // Since the config changed, trial verifier should not run.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);

  // Expect no report.
  EXPECT_TRUE(reports.empty());
}

TEST_F(TrialComparisonCertVerifierTest, ConfigChangedBeforeVerification) {
  // Primary verifier returns an error status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  // Both verifiers are initially NotCalledCertVerifyProc, but should swap to
  // verify_proc1 and verify_proc2 when UpdateVerifyProcData is called.
  TrialComparisonCertVerifier verifier(
      ProcFactory({MakeNotCalledProc(), MakeNotCalledProc(), verify_proc1,
                   MakeNotCalledProc()},
                  {MakeNotCalledProc(), verify_proc2}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));

  verifier.set_trial_allowed(true);

  // Change the verifier Chrome Root Store data before verification, so the
  // Verify should call the verifiers that were swapped in by the factories
  // instead of the initial ones.
  verifier.UpdateVerifyProcData(nullptr, no_crs_impl_params_);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(CERT_STATUS_DATE_INVALID, report.primary_result.cert_status);
  EXPECT_EQ(0U, report.trial_result.cert_status);

  EXPECT_TRUE(report.primary_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.trial_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.unverified_cert->EqualsIncludingChain(leaf_cert_1_.get()));

  EXPECT_FALSE(report.enable_rev_checking);
  EXPECT_FALSE(report.require_rev_checking_local_anchors);
  EXPECT_FALSE(report.enable_sha1_local_anchors);
  EXPECT_FALSE(report.disable_symantec_enforcement);

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryErrorSecondaryValid, 1);
}

TEST_F(TrialComparisonCertVerifierTest,
       CertVerifyProcChangedDuringPrimaryVerification) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc(), MakeNotCalledProc(),
                   MakeNotCalledProc()},
                  {MakeNotCalledProc(), MakeNotCalledProc()}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Change the verifier Chrome Root Store data before the primary verification
  // finishes.
  verifier.UpdateVerifyProcData(nullptr, no_crs_impl_params_);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  RunUntilIdle();

  // Since the config changed, trial verifier should not run.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);

  // Expect no report.
  EXPECT_TRUE(reports.empty());
}

TEST_F(TrialComparisonCertVerifierTest, ConfigChangedDuringTrialVerification) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier returns an error status.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  // Change the verifier config during the trial verification.
  CertVerifier::Config config;
  config.enable_sha1_local_anchors = true;
  verifier.SetConfig(config);

  RunUntilIdle();

  // Since the config was the same when both primary and trial verification
  // started, the result should still be reported.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryValidSecondaryError, 1);

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(0U, report.primary_result.cert_status);
  EXPECT_EQ(CERT_STATUS_DATE_INVALID, report.trial_result.cert_status);
}

TEST_F(TrialComparisonCertVerifierTest,
       CertVerifyProcChangedDuringTrialVerification) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier returns an error status.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc(), MakeNotCalledProc(),
                   MakeNotCalledProc()},
                  {verify_proc2, MakeNotCalledProc()}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  // Change the verifier Chrome Root Store data during the trial verification.
  verifier.UpdateVerifyProcData(nullptr, no_crs_impl_params_);

  RunUntilIdle();

  // Since the config was the same when both primary and trial verification
  // started, the result should still be reported.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryValidSecondaryError, 1);

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(0U, report.primary_result.cert_status);
  EXPECT_EQ(CERT_STATUS_DATE_INVALID, report.trial_result.cert_status);
}

TEST_F(TrialComparisonCertVerifierTest, SameResult) {
  CertVerifyResult dummy_result;
  dummy_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, dummy_result);
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, dummy_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample("Net.CertVerifier_TrialComparisonResult",
                                 TrialComparisonResult::kEqual, 1);
}

TEST_F(TrialComparisonCertVerifierTest, PrimaryVerifierErrorSecondaryOk) {
  // Primary verifier returns an error status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(CERT_STATUS_DATE_INVALID, report.primary_result.cert_status);
  EXPECT_EQ(0U, report.trial_result.cert_status);

  EXPECT_TRUE(report.primary_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.trial_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.unverified_cert->EqualsIncludingChain(leaf_cert_1_.get()));

  EXPECT_FALSE(report.enable_rev_checking);
  EXPECT_FALSE(report.require_rev_checking_local_anchors);
  EXPECT_FALSE(report.enable_sha1_local_anchors);
  EXPECT_FALSE(report.disable_symantec_enforcement);

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryErrorSecondaryValid, 1);
}

TEST_F(TrialComparisonCertVerifierTest, PrimaryVerifierOkSecondaryError) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier returns an error status.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     "ocsp", "sct");
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(0U, report.primary_result.cert_status);
  EXPECT_EQ(CERT_STATUS_DATE_INVALID, report.trial_result.cert_status);

  EXPECT_TRUE(report.primary_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.trial_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.unverified_cert->EqualsIncludingChain(leaf_cert_1_.get()));
  EXPECT_EQ("ocsp", report.stapled_ocsp);
  EXPECT_EQ("sct", report.sct_list);

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryValidSecondaryError, 1);
}

TEST_F(TrialComparisonCertVerifierTest, BothVerifiersDifferentErrors) {
  // Primary verifier returns an error status.
  CertVerifyResult primary_result;
  primary_result.cert_status = CERT_STATUS_VALIDITY_TOO_LONG;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_VALIDITY_TOO_LONG,
                                               primary_result);

  // Trial verifier returns a different error status.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_VALIDITY_TOO_LONG));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(CERT_STATUS_VALIDITY_TOO_LONG, report.primary_result.cert_status);
  EXPECT_EQ(CERT_STATUS_DATE_INVALID, report.trial_result.cert_status);

  EXPECT_TRUE(report.primary_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.trial_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.unverified_cert->EqualsIncludingChain(leaf_cert_1_.get()));

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kBothErrorDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest,
       BothVerifiersOkDifferentVerifiedChains) {
  // Primary verifier returns chain1 regardless of arguments.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier returns a different verified cert chain.
  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_2_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, verify_proc1}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(0U, report.primary_result.cert_status);
  EXPECT_EQ(0U, report.trial_result.cert_status);

  EXPECT_TRUE(report.primary_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.trial_result.verified_cert->EqualsIncludingChain(
      cert_chain_2_.get()));
  EXPECT_TRUE(report.unverified_cert->EqualsIncludingChain(leaf_cert_1_.get()));

  // The primary verifier should be used twice (first with the initial chain,
  // then with the results of the trial verifier).
  EXPECT_EQ(2, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kBothValidDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest,
       DifferentVerifiedChainsAndConfigHasChanged) {
  // Primary verifier returns chain1 regardless of arguments.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_REVOKED,
                                               primary_result);

  // Trial verifier returns a different verified cert chain.
  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_2_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  // Change the configuration. The trial verification should complete, but
  // the difference in verified chains should prevent a trial reverification.
  CertVerifier::Config config;
  config.enable_sha1_local_anchors = true;
  verifier.SetConfig(config);

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report, since the configuration changed and the primary
  // verifier could not be used to retry.
  ASSERT_EQ(0U, reports.size());

  // The primary verifier should only be used once, as the configuration
  // changes after the trial verification is started.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredConfigurationChanged, 1);
}

TEST_F(TrialComparisonCertVerifierTest,
       BothVerifiersOkDifferentVerifiedChainsEqualAfterReverification) {
  CertVerifyResult chain1_result;
  chain1_result.verified_cert = cert_chain_1_;
  CertVerifyResult chain2_result;
  chain2_result.verified_cert = cert_chain_2_;

  scoped_refptr<MockCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<MockCertVerifyProc>();
  // Primary verifier returns ok status and chain1 if verifying the leaf alone.
  EXPECT_CALL(*verify_proc1,
              VerifyInternal(leaf_cert_1_.get(), _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<6>(chain1_result), Return(OK)));
  // Primary verifier returns ok status and chain2 if verifying chain2.
  EXPECT_CALL(*verify_proc1,
              VerifyInternal(cert_chain_2_.get(), _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<6>(chain2_result), Return(OK)));

  // Trial verifier returns ok status and chain2.
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, chain2_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, verify_proc1}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  testing::Mock::VerifyAndClear(verify_proc1.get());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredDifferentPathReVerifiesEquivalent, 1);
}

TEST_F(TrialComparisonCertVerifierTest,
       DifferentVerifiedChainsIgnorableDifferenceAfterReverification) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("trial_comparison_cert_verifier_unittest")
          .AppendASCII("target-multiple-policies");
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  scoped_refptr<X509Certificate> leaf = X509Certificate::CreateFromBuffer(
      bssl::UpRef(cert_chain->cert_buffer()), {});
  ASSERT_TRUE(leaf);

  // Chain with the same leaf and different root. This is not a valid chain, but
  // doesn't matter for the unittest since this uses mock CertVerifyProcs.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(
      bssl::UpRef(cert_chain_1_->intermediate_buffers().back().get()));
  scoped_refptr<X509Certificate> different_chain =
      X509Certificate::CreateFromBuffer(bssl::UpRef(cert_chain->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(different_chain);

  CertVerifyResult different_chain_result_no_known_root;
  different_chain_result_no_known_root.verified_cert = different_chain;

  CertVerifyResult different_chain_result_known_root;
  different_chain_result_known_root.verified_cert = different_chain;
  different_chain_result_known_root.is_issued_by_known_root = true;

  CertVerifyResult chain_result;
  chain_result.verified_cert = cert_chain;
  chain_result.is_issued_by_known_root = true;

  SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(x509_util::CryptoBufferAsStringPiece(
                               cert_chain->intermediate_buffers().back().get()),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  scoped_refptr<MockCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<MockCertVerifyProc>();
  // Primary verifier returns ok status and different_chain if verifying leaf
  // alone, but not is_known_root.
  EXPECT_CALL(*verify_proc1, VerifyInternal(leaf.get(), _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<6>(different_chain_result_no_known_root),
                      Return(OK)));
  // Primary verifier returns ok status and different_chain if verifying
  // cert_chain and with is_known_root..
  EXPECT_CALL(*verify_proc1,
              VerifyInternal(cert_chain.get(), _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<6>(different_chain_result_known_root),
                      Return(OK)));

  // Trial verifier returns ok status and chain_result.
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, chain_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, verify_proc1}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf, "test.example", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  // Primary verifier should be used twice, the second time with the chain
  // from the trial verifier. Even so, it only should be counted once in
  // metrics.
  testing::Mock::VerifyAndClear(verify_proc1.get());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredDifferentPathReVerifiesEquivalent, 1);
}

TEST_F(TrialComparisonCertVerifierTest, BothVerifiersOkDifferentCertStatus) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status =
      CERT_STATUS_IS_EV | CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  secondary_result.cert_status = CERT_STATUS_CT_COMPLIANCE_FAILED;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::Config config;
  config.enable_rev_checking = true;
  config.enable_sha1_local_anchors = true;
  verifier.SetConfig(config);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", 0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(CERT_STATUS_IS_EV | CERT_STATUS_REV_CHECKING_ENABLED,
            report.primary_result.cert_status);
  EXPECT_EQ(CERT_STATUS_CT_COMPLIANCE_FAILED, report.trial_result.cert_status);

  EXPECT_TRUE(report.enable_rev_checking);
  EXPECT_FALSE(report.require_rev_checking_local_anchors);
  EXPECT_TRUE(report.enable_sha1_local_anchors);
  EXPECT_FALSE(report.disable_symantec_enforcement);

  EXPECT_TRUE(report.primary_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.trial_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.unverified_cert->EqualsIncludingChain(leaf_cert_1_.get()));

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kBothValidDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest, CancelledDuringPrimaryVerification) {
  // Primary verifier returns an error status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  // Trial verifier has ok status.
  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  std::unique_ptr<CertVerifier::Request> request;
  int error =
      verifier.Verify(params, &result, base::BindOnce(&NotCalledCallback),
                      &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Delete the request, cancelling it.
  request.reset();

  // The callback to the main verifier does not run. However, the verification
  // still completes in the background and triggers the trial verification.

  // Trial verifier should still run.
  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());
  const TrialReportInfo& report = reports[0];

  EXPECT_EQ(CERT_STATUS_DATE_INVALID, report.primary_result.cert_status);
  EXPECT_EQ(0U, report.trial_result.cert_status);

  EXPECT_TRUE(report.primary_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.trial_result.verified_cert->EqualsIncludingChain(
      cert_chain_1_.get()));
  EXPECT_TRUE(report.unverified_cert->EqualsIncludingChain(leaf_cert_1_.get()));

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryErrorSecondaryValid, 1);
}

TEST_F(TrialComparisonCertVerifierTest, DeletedDuringPrimaryVerification) {
  // Primary verifier returns an error status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  std::vector<TrialReportInfo> reports;
  auto verifier = std::make_unique<TrialComparisonCertVerifier>(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {MakeNotCalledProc()}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));
  verifier->set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  std::unique_ptr<CertVerifier::Request> request;
  int error =
      verifier->Verify(params, &result, base::BindOnce(&NotCalledCallback),
                       &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Delete the TrialComparisonCertVerifier.
  verifier.reset();

  // The callback to the main verifier does not run. The verification task
  // still completes in the background, but since the CertVerifier has been
  // deleted, the result is ignored.

  // Wait for any tasks to finish.
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  // The trial verifier should never be called, nor histograms recorded.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, DeletedDuringVerificationResult) {
  // Primary verifier returns an error status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  std::vector<TrialReportInfo> reports;
  auto verifier = std::make_unique<TrialComparisonCertVerifier>(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {MakeNotCalledProc()}),
      nullptr, no_crs_impl_params_,
      base::BindRepeating(&RecordTrialReport, &reports));
  verifier->set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier->Verify(
      params, &result,
      base::BindLambdaForTesting([&callback, &verifier](int result) {
        // Delete the verifier while processing the result. This should not
        // start a trial verification.
        verifier.reset();
        callback.callback().Run(result);
      }),
      &request, NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Wait for primary verifier to finish.
  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));

  // The callback to the trial verifier does not run. No verification task
  // should start, as the verifier was deleted before the trial verification
  // was started.

  // Wait for any tasks to finish.
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  // Histograms for the primary or trial verification should not be recorded,
  // as the trial verification was cancelled by deleting the verifier.
  EXPECT_EQ(1, verify_proc1->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, DeletedDuringTrialReport) {
  // Primary verifier returns an error status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  // Trial verifier has ok status.
  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  bool was_report_callback_called = false;
  std::unique_ptr<TrialComparisonCertVerifier> verifier;
  verifier = std::make_unique<TrialComparisonCertVerifier>(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_,
      base::BindLambdaForTesting(
          [&verifier, &was_report_callback_called](
              const std::string& hostname,
              const scoped_refptr<X509Certificate>& unverified_cert,
              bool enable_rev_checking, bool require_rev_checking_local_anchors,
              bool enable_sha1_local_anchors, bool disable_symantec_enforcement,
              const std::string& stapled_ocsp, const std::string& sct_list,
              const net::CertVerifyResult& primary_result,
              const net::CertVerifyResult& trial_result) {
            // During processing of a report, delete the underlying verifier.
            // This should not cause any issues.
            was_report_callback_called = true;
            verifier.reset();
          }));
  verifier->set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier->Verify(params, &result, callback.callback(), &request,
                               NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // The callback should be notified of the primary result.
  ASSERT_THAT(callback.WaitForResult(), IsError(ERR_CERT_DATE_INVALID));

  // Wait for the verification task to complete in the background. This
  // should ultimately call the ReportCallback that will delete the
  // verifier.
  RunUntilIdle();

  EXPECT_TRUE(was_report_callback_called);

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryErrorSecondaryValid, 1);
}

TEST_F(TrialComparisonCertVerifierTest, DeletedAfterTrialVerificationStarted) {
  // Primary verifier returns an error status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  // Trial verifier has ok status.
  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  auto verifier = std::make_unique<TrialComparisonCertVerifier>(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier->set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier->Verify(params, &result, callback.callback(), &request,
                               NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Wait for primary verifier to finish.
  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));

  // Delete the TrialComparisonCertVerifier. The trial verification is still
  // running on the task scheduler (or, depending on timing, has posted back
  // to the IO thread after the Quit event).
  verifier.reset();

  // The callback to the trial verifier does not run. The verification task
  // still completes in the background, but since the CertVerifier has been
  // deleted, the result is ignored.

  // Wait for any tasks to finish.
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  // Histograms for trial verifier should not be recorded.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, PrimaryRevokedSecondaryOk) {
  CertVerifyResult revoked_result;
  revoked_result.verified_cert = cert_chain_1_;
  revoked_result.cert_status = CERT_STATUS_REVOKED;

  CertVerifyResult ok_result;
  ok_result.verified_cert = cert_chain_1_;

  // Primary verifier returns an error status.
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_REVOKED,
                                               revoked_result);

  // Secondary verifier returns ok status regardless of whether
  // REV_CHECKING_ENABLED was passed.
  scoped_refptr<MockCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<MockCertVerifyProc>();
  EXPECT_CALL(*verify_proc2, VerifyInternal(_, _, _, _, _, _, _, _))
      .Times(1)
      .WillRepeatedly(DoAll(SetArgPointee<6>(ok_result), Return(OK)));

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  RunUntilIdle();

  EXPECT_EQ(1, verify_proc1->num_verifications());
  testing::Mock::VerifyAndClear(verify_proc2.get());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryErrorSecondaryValid, 1);

  // Expect a report.
  EXPECT_EQ(1U, reports.size());
}

#if defined(PLATFORM_USES_CHROMIUM_EV_METADATA)
TEST_F(TrialComparisonCertVerifierTest, MultipleEVPolicies) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("trial_comparison_cert_verifier_unittest")
          .AppendASCII("target-multiple-policies");
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(x509_util::CryptoBufferAsStringPiece(
                               cert_chain->intermediate_buffers().back().get()),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  // Both policies in the target are EV policies, but only 1.2.6.7 is valid for
  // the root in this chain.
  ScopedTestEVPolicy scoped_ev_policy_1(EVRootCAMetadata::GetInstance(),
                                        root_fingerprint, "1.2.6.7");
  ScopedTestEVPolicy scoped_ev_policy_2(EVRootCAMetadata::GetInstance(),
                                        SHA256HashValue(), "1.2.3.4");

  // Both verifiers return OK, but secondary returns EV status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain;
  secondary_result.cert_status =
      CERT_STATUS_IS_EV | CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredMultipleEVPoliciesAndOneMatchesRoot, 1);
}

TEST_F(TrialComparisonCertVerifierTest, MultipleEVPoliciesNoneValidForRoot) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("trial_comparison_cert_verifier_unittest")
          .AppendASCII("target-multiple-policies");
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);

  // Both policies in the target are EV policies, but neither is valid for the
  // root in this chain.
  ScopedTestEVPolicy scoped_ev_policy_1(EVRootCAMetadata::GetInstance(), {1},
                                        "1.2.6.7");
  ScopedTestEVPolicy scoped_ev_policy_2(EVRootCAMetadata::GetInstance(), {2},
                                        "1.2.3.4");

  // Both verifiers return OK, but secondary returns EV status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain;
  secondary_result.cert_status =
      CERT_STATUS_IS_EV | CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kBothValidDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest, MultiplePoliciesOnlyOneIsEV) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("trial_comparison_cert_verifier_unittest")
          .AppendASCII("target-multiple-policies");
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(x509_util::CryptoBufferAsStringPiece(
                               cert_chain->intermediate_buffers().back().get()),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  // One policy in the target is an EV policy and is valid for the root.
  ScopedTestEVPolicy scoped_ev_policy_1(EVRootCAMetadata::GetInstance(),
                                        root_fingerprint, "1.2.6.7");

  // Both verifiers return OK, but secondary returns EV status.
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain;
  secondary_result.cert_status =
      CERT_STATUS_IS_EV | CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  ASSERT_EQ(1U, reports.size());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kBothValidDifferentDetails, 1);
}
#endif

TEST_F(TrialComparisonCertVerifierTest, LocallyTrustedLeaf) {
  // Platform verifier verifies the leaf directly.
  CertVerifyResult primary_result;
  primary_result.verified_cert = leaf_cert_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, primary_result);

  // Trial verifier does not support directly-trusted leaf certs.
  CertVerifyResult secondary_result;
  secondary_result.cert_status = CERT_STATUS_AUTHORITY_INVALID;
  secondary_result.verified_cert = leaf_cert_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_AUTHORITY_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsOk());

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect a report.
  EXPECT_EQ(1U, reports.size());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kPrimaryValidSecondaryError, 1);
}

// Ignore results where both primary and trial verifier report SHA-1 signatures.
TEST_F(TrialComparisonCertVerifierTest, SHA1Ignored) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status =
      CERT_STATUS_SHA1_SIGNATURE_PRESENT | CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(
          ERR_CERT_WEAK_SIGNATURE_ALGORITHM, primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  secondary_result.cert_status = CERT_STATUS_SHA1_SIGNATURE_PRESENT;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(
          ERR_CERT_WEAK_SIGNATURE_ALGORITHM, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredSHA1SignaturePresent, 1);
}

// Ignore results where both primary and trial verifier report AUHORITY_INVALID
// errors.
TEST_F(TrialComparisonCertVerifierTest, BothAuthorityInvalidIgnored) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status =
      CERT_STATUS_SHA1_SIGNATURE_PRESENT | CERT_STATUS_AUTHORITY_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_AUTHORITY_INVALID,
                                               primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  secondary_result.cert_status = CERT_STATUS_AUTHORITY_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_AUTHORITY_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredBothAuthorityInvalid, 1);
}

// Ignore results where both primary and trial verifier terminate in
// known roots (with the same error and status codes).
TEST_F(TrialComparisonCertVerifierTest, BothKnownRootsIgnored) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  primary_result.is_issued_by_known_root = true;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(
          ERR_CERT_WEAK_SIGNATURE_ALGORITHM, primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_2_;
  secondary_result.cert_status = CERT_STATUS_DATE_INVALID;
  secondary_result.is_issued_by_known_root = true;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(
          ERR_CERT_WEAK_SIGNATURE_ALGORITHM, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample("Net.CertVerifier_TrialComparisonResult",
                                 TrialComparisonResult::kIgnoredBothKnownRoot,
                                 1);
}

// Ignore results where trial reports ERR_CERT_AUTHORITY_INVALID and the primary
// reports any error with a cert status of CERT_STATUS_SYMANTEC_LEGACY
TEST_F(TrialComparisonCertVerifierTest,
       IgnoreAuthInvalidBuiltinWhenPrimaryReportSymantec) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status =
      CERT_STATUS_DATE_INVALID | CERT_STATUS_SYMANTEC_LEGACY;
  primary_result.is_issued_by_known_root = true;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_COMMON_NAME_INVALID,
                                               primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_2_;
  secondary_result.cert_status = CERT_STATUS_AUTHORITY_INVALID;
  secondary_result.is_issued_by_known_root = true;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_AUTHORITY_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredBuiltinAuthorityInvalidPlatformSymantec,
      1);
}

TEST_F(TrialComparisonCertVerifierTest, LetsEncryptSpecialCase) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = lets_encrypt_dst_x3_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = lets_encrypt_isrg_x1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(OK, secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredLetsEncryptExpiredRoot, 1);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(TrialComparisonCertVerifierTest, AndroidPreferDate) {
  CertVerifyResult primary_result;
  primary_result.verified_cert = lets_encrypt_dst_x3_;
  primary_result.cert_status = CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_DATE_INVALID,
                                               primary_result);

  CertVerifyResult secondary_result;
  secondary_result.verified_cert = lets_encrypt_isrg_x1_;
  secondary_result.cert_status = CERT_STATUS_AUTHORITY_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(ERR_CERT_AUTHORITY_INVALID,
                                               secondary_result);

  std::vector<TrialReportInfo> reports;
  TrialComparisonCertVerifier verifier(
      ProcFactory({verify_proc1, MakeNotCalledProc()}, {verify_proc2}), nullptr,
      no_crs_impl_params_, base::BindRepeating(&RecordTrialReport, &reports));
  verifier.set_trial_allowed(true);

  CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", /*flags=*/0,
                                     /*ocsp_response=*/std::string(),
                                     /*sct_list=*/std::string());
  CertVerifyResult result;
  TestCompletionCallback callback;
  std::unique_ptr<CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              NetLogWithSource());
  ASSERT_THAT(error, IsError(ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));

  verify_proc2->WaitForVerifyCall();
  RunUntilIdle();

  // Expect no report.
  EXPECT_TRUE(reports.empty());

  EXPECT_EQ(1, verify_proc1->num_verifications());
  EXPECT_EQ(1, verify_proc2->num_verifications());
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonResult::kIgnoredAndroidErrorDatePriority, 1);
}
#endif

}  // namespace net
