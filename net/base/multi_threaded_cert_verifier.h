// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MULTI_THREADED_CERT_VERIFIER_H_
#define NET_BASE_MULTI_THREADED_CERT_VERIFIER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/cert_database.h"
#include "net/base/cert_verifier.h"
#include "net/base/cert_verify_result.h"
#include "net/base/completion_callback.h"
#include "net/base/expiring_cache.h"
#include "net/base/net_export.h"
#include "net/base/x509_cert_types.h"

namespace net {

class CertVerifierJob;
class CertVerifierRequest;
class CertVerifierWorker;
class CertVerifyProc;

// MultiThreadedCertVerifier is a CertVerifier implementation that runs
// synchronous CertVerifier implementations on worker threads.
class NET_EXPORT_PRIVATE MultiThreadedCertVerifier
    : public CertVerifier,
      NON_EXPORTED_BASE(public base::NonThreadSafe),
      public CertDatabase::Observer {
 public:
  MultiThreadedCertVerifier();

  // When the verifier is destroyed, all certificate verifications requests are
  // canceled, and their completion callbacks will not be called.
  virtual ~MultiThreadedCertVerifier();

  // CertVerifier implementation
  virtual int Verify(X509Certificate* cert,
                     const std::string& hostname,
                     int flags,
                     CRLSet* crl_set,
                     CertVerifyResult* verify_result,
                     const CompletionCallback& callback,
                     CertVerifier::RequestHandle* out_req,
                     const BoundNetLog& net_log) OVERRIDE;

  virtual void CancelRequest(CertVerifier::RequestHandle req) OVERRIDE;

 private:
  friend class CertVerifierWorker;  // Calls HandleResult.
  friend class CertVerifierRequest;
  friend class CertVerifierJob;
  friend class MultiThreadedCertVerifierTest;
  FRIEND_TEST_ALL_PREFIXES(MultiThreadedCertVerifierTest, CacheHit);
  FRIEND_TEST_ALL_PREFIXES(MultiThreadedCertVerifierTest, DifferentCACerts);
  FRIEND_TEST_ALL_PREFIXES(MultiThreadedCertVerifierTest, InflightJoin);
  FRIEND_TEST_ALL_PREFIXES(MultiThreadedCertVerifierTest, CancelRequest);
  FRIEND_TEST_ALL_PREFIXES(MultiThreadedCertVerifierTest,
                           RequestParamsComparators);

  // Input parameters of a certificate verification request.
  struct RequestParams {
    RequestParams(const SHA1Fingerprint& cert_fingerprint_arg,
                  const SHA1Fingerprint& ca_fingerprint_arg,
                  const std::string& hostname_arg,
                  int flags_arg)
        : cert_fingerprint(cert_fingerprint_arg),
          ca_fingerprint(ca_fingerprint_arg),
          hostname(hostname_arg),
          flags(flags_arg) {}

    bool operator<(const RequestParams& other) const {
      // |flags| is compared before |cert_fingerprint|, |ca_fingerprint|, and
      // |hostname| under assumption that integer comparisons are faster than
      // memory and string comparisons.
      if (flags != other.flags)
        return flags < other.flags;
      int rv = memcmp(cert_fingerprint.data, other.cert_fingerprint.data,
                      sizeof(cert_fingerprint.data));
      if (rv != 0)
        return rv < 0;
      rv = memcmp(ca_fingerprint.data, other.ca_fingerprint.data,
                  sizeof(ca_fingerprint.data));
      if (rv != 0)
        return rv < 0;
      return hostname < other.hostname;
    }

    SHA1Fingerprint cert_fingerprint;
    SHA1Fingerprint ca_fingerprint;
    std::string hostname;
    int flags;
  };

  // CachedResult contains the result of a certificate verification.
  struct CachedResult {
    CachedResult();
    ~CachedResult();

    int error;  // The return value of CertVerifier::Verify.
    CertVerifyResult result;  // The output of CertVerifier::Verify.
  };

  void HandleResult(X509Certificate* cert,
                    const std::string& hostname,
                    int flags,
                    int error,
                    const CertVerifyResult& verify_result);

  // CertDatabase::Observer methods:
  virtual void OnCertTrustChanged(const X509Certificate* cert) OVERRIDE;

  // For unit testing.
  void ClearCache() { cache_.Clear(); }
  size_t GetCacheSize() const { return cache_.size(); }
  uint64 cache_hits() const { return cache_hits_; }
  uint64 requests() const { return requests_; }
  uint64 inflight_joins() const { return inflight_joins_; }
  void SetCertVerifyProc(CertVerifyProc* verify_proc);

  // cache_ maps from a request to a cached result.
  typedef ExpiringCache<RequestParams, CachedResult> CertVerifierCache;
  CertVerifierCache cache_;

  // inflight_ maps from a request to an active verification which is taking
  // place.
  std::map<RequestParams, CertVerifierJob*> inflight_;

  uint64 requests_;
  uint64 cache_hits_;
  uint64 inflight_joins_;

  scoped_refptr<CertVerifyProc> verify_proc_;

  DISALLOW_COPY_AND_ASSIGN(MultiThreadedCertVerifier);
};

}  // namespace net

#endif  // NET_BASE_MULTI_THREADED_CERT_VERIFIER_H_
