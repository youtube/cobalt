// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/server_bound_cert_service.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "crypto/ec_private_key.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domain.h"
#include "net/base/server_bound_cert_store.h"
#include "net/base/x509_certificate.h"
#include "net/base/x509_util.h"

#if defined(USE_NSS)
#include <private/pprthred.h>  // PR_DetachThread
#endif

namespace net {

namespace {

const int kKeySizeInBits = 1024;
const int kValidityPeriodInDays = 365;

bool IsSupportedCertType(uint8 type) {
  switch(type) {
    case CLIENT_CERT_ECDSA_SIGN:
      return true;
    default:
      return false;
  }
}

// Used by the GetDomainBoundCertResult histogram to record the final
// outcome of each GetDomainBoundCert call.  Do not re-use values.
enum GetCertResult {
  // Synchronously found and returned an existing domain bound cert.
  SYNC_SUCCESS = 0,
  // Generated and returned a domain bound cert asynchronously.
  ASYNC_SUCCESS = 1,
  // Generation request was cancelled before the cert generation completed.
  ASYNC_CANCELLED = 2,
  // Cert generation failed.
  ASYNC_FAILURE_KEYGEN = 3,
  ASYNC_FAILURE_CREATE_CERT = 4,
  ASYNC_FAILURE_EXPORT_KEY = 5,
  ASYNC_FAILURE_UNKNOWN = 6,
  // GetDomainBoundCert was called with invalid arguments.
  INVALID_ARGUMENT = 7,
  // We don't support any of the cert types the server requested.
  UNSUPPORTED_TYPE = 8,
  // Server asked for a different type of certs while we were generating one.
  TYPE_MISMATCH = 9,
  // Couldn't start a worker to generate a cert.
  WORKER_FAILURE = 10,
  GET_CERT_RESULT_MAX
};

void RecordGetDomainBoundCertResult(GetCertResult result) {
  UMA_HISTOGRAM_ENUMERATION("DomainBoundCerts.GetDomainBoundCertResult", result,
                            GET_CERT_RESULT_MAX);
}

void RecordGetCertTime(base::TimeDelta request_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GetCertTime",
                             request_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(5),
                             50);
}

}  // namespace

// Represents the output and result callback of a request.
class ServerBoundCertServiceRequest {
 public:
  ServerBoundCertServiceRequest(base::TimeTicks request_start,
                                const CompletionCallback& callback,
                                SSLClientCertType* type,
                                std::string* private_key,
                                std::string* cert)
      : request_start_(request_start),
        callback_(callback),
        type_(type),
        private_key_(private_key),
        cert_(cert) {
  }

  // Ensures that the result callback will never be made.
  void Cancel() {
    RecordGetDomainBoundCertResult(ASYNC_CANCELLED);
    callback_.Reset();
    type_ = NULL;
    private_key_ = NULL;
    cert_ = NULL;
  }

  // Copies the contents of |private_key| and |cert| to the caller's output
  // arguments and calls the callback.
  void Post(int error,
            SSLClientCertType type,
            const std::string& private_key,
            const std::string& cert) {
    switch (error) {
      case OK: {
        base::TimeDelta request_time = base::TimeTicks::Now() - request_start_;
        UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GetCertTimeAsync",
                                   request_time,
                                   base::TimeDelta::FromMilliseconds(1),
                                   base::TimeDelta::FromMinutes(5),
                                   50);
        RecordGetCertTime(request_time);
        RecordGetDomainBoundCertResult(ASYNC_SUCCESS);
        break;
      }
      case ERR_KEY_GENERATION_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_KEYGEN);
        break;
      case ERR_ORIGIN_BOUND_CERT_GENERATION_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_CREATE_CERT);
        break;
      case ERR_PRIVATE_KEY_EXPORT_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_EXPORT_KEY);
        break;
      default:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_UNKNOWN);
        break;
    }
    if (!callback_.is_null()) {
      *type_ = type;
      *private_key_ = private_key;
      *cert_ = cert;
      callback_.Run(error);
    }
    delete this;
  }

  bool canceled() const { return callback_.is_null(); }

 private:
  base::TimeTicks request_start_;
  CompletionCallback callback_;
  SSLClientCertType* type_;
  std::string* private_key_;
  std::string* cert_;
};

// ServerBoundCertServiceWorker runs on a worker thread and takes care of the
// blocking process of performing key generation. Deletes itself eventually
// if Start() succeeds.
// TODO(mattm): don't use locking and explicit cancellation.
class ServerBoundCertServiceWorker {
 public:
  ServerBoundCertServiceWorker(
      const std::string& server_identifier,
      SSLClientCertType type,
      ServerBoundCertService* server_bound_cert_service)
      : server_identifier_(server_identifier),
        type_(type),
        serial_number_(base::RandInt(0, std::numeric_limits<int>::max())),
        origin_loop_(MessageLoop::current()),
        server_bound_cert_service_(server_bound_cert_service),
        canceled_(false),
        error_(ERR_FAILED) {
  }

  bool Start(const scoped_refptr<base::TaskRunner>& task_runner) {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);

    return task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ServerBoundCertServiceWorker::Run, base::Unretained(this)));
  }

  // Cancel is called from the origin loop when the ServerBoundCertService is
  // getting deleted.
  void Cancel() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);
    base::AutoLock locked(lock_);
    canceled_ = true;
  }

 private:
  void Run() {
    // Runs on a worker thread.
    error_ = ServerBoundCertService::GenerateCert(server_identifier_,
                                                  type_,
                                                  serial_number_,
                                                  &creation_time_,
                                                  &expiration_time_,
                                                  &private_key_,
                                                  &cert_);
#if defined(USE_NSS)
    // Detach the thread from NSPR.
    // Calling NSS functions attaches the thread to NSPR, which stores
    // the NSPR thread ID in thread-specific data.
    // The threads in our thread pool terminate after we have called
    // PR_Cleanup. Unless we detach them from NSPR, net_unittests gets
    // segfaults on shutdown when the threads' thread-specific data
    // destructors run.
    PR_DetachThread();
#endif
    Finish();
  }

  // DoReply runs on the origin thread.
  void DoReply() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);
    {
      // We lock here because the worker thread could still be in Finished,
      // after the PostTask, but before unlocking |lock_|. If we do not lock in
      // this case, we will end up deleting a locked Lock, which can lead to
      // memory leaks or worse errors.
      base::AutoLock locked(lock_);
      if (!canceled_) {
        server_bound_cert_service_->HandleResult(
            server_identifier_, error_, type_, creation_time_, expiration_time_,
            private_key_, cert_);
      }
    }
    delete this;
  }

  void Finish() {
    // Runs on the worker thread.
    // We assume that the origin loop outlives the ServerBoundCertService. If
    // the ServerBoundCertService is deleted, it will call Cancel on us. If it
    // does so before the Acquire, we'll delete ourselves and return. If it's
    // trying to do so concurrently, then it'll block on the lock and we'll
    // call PostTask while the ServerBoundCertService (and therefore the
    // MessageLoop) is still alive. If it does so after this function, we
    // assume that the MessageLoop will process pending tasks. In which case
    // we'll notice the |canceled_| flag in DoReply.

    bool canceled;
    {
      base::AutoLock locked(lock_);
      canceled = canceled_;
      if (!canceled) {
        origin_loop_->PostTask(
            FROM_HERE, base::Bind(&ServerBoundCertServiceWorker::DoReply,
                                  base::Unretained(this)));
      }
    }
    if (canceled)
      delete this;
  }

  const std::string server_identifier_;
  const SSLClientCertType type_;
  // Note that serial_number_ must be initialized on a non-worker thread
  // (see documentation for ServerBoundCertService::GenerateCert).
  uint32 serial_number_;
  MessageLoop* const origin_loop_;
  ServerBoundCertService* const server_bound_cert_service_;

  // lock_ protects canceled_.
  base::Lock lock_;

  // If canceled_ is true,
  // * origin_loop_ cannot be accessed by the worker thread,
  // * server_bound_cert_service_ cannot be accessed by any thread.
  bool canceled_;

  int error_;
  base::Time creation_time_;
  base::Time expiration_time_;
  std::string private_key_;
  std::string cert_;

  DISALLOW_COPY_AND_ASSIGN(ServerBoundCertServiceWorker);
};

// A ServerBoundCertServiceJob is a one-to-one counterpart of an
// ServerBoundCertServiceWorker. It lives only on the ServerBoundCertService's
// origin message loop.
class ServerBoundCertServiceJob {
 public:
  ServerBoundCertServiceJob(ServerBoundCertServiceWorker* worker,
                            SSLClientCertType type)
      : worker_(worker), type_(type) {
  }

  ~ServerBoundCertServiceJob() {
    if (worker_) {
      worker_->Cancel();
      DeleteAllCanceled();
    }
  }

  SSLClientCertType type() const { return type_; }

  void AddRequest(ServerBoundCertServiceRequest* request) {
    requests_.push_back(request);
  }

  void HandleResult(int error,
                    SSLClientCertType type,
                    const std::string& private_key,
                    const std::string& cert) {
    worker_ = NULL;
    PostAll(error, type, private_key, cert);
  }

 private:
  void PostAll(int error,
               SSLClientCertType type,
               const std::string& private_key,
               const std::string& cert) {
    std::vector<ServerBoundCertServiceRequest*> requests;
    requests_.swap(requests);

    for (std::vector<ServerBoundCertServiceRequest*>::iterator
         i = requests.begin(); i != requests.end(); i++) {
      (*i)->Post(error, type, private_key, cert);
      // Post() causes the ServerBoundCertServiceRequest to delete itself.
    }
  }

  void DeleteAllCanceled() {
    for (std::vector<ServerBoundCertServiceRequest*>::iterator
         i = requests_.begin(); i != requests_.end(); i++) {
      if ((*i)->canceled()) {
        delete *i;
      } else {
        LOG(DFATAL) << "ServerBoundCertServiceRequest leaked!";
      }
    }
  }

  std::vector<ServerBoundCertServiceRequest*> requests_;
  ServerBoundCertServiceWorker* worker_;
  SSLClientCertType type_;
};

// static
const char ServerBoundCertService::kEPKIPassword[] = "";

ServerBoundCertService::ServerBoundCertService(
    ServerBoundCertStore* server_bound_cert_store,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : server_bound_cert_store_(server_bound_cert_store),
      task_runner_(task_runner),
      requests_(0),
      cert_store_hits_(0),
      inflight_joins_(0) {}

ServerBoundCertService::~ServerBoundCertService() {
  STLDeleteValues(&inflight_);
}

//static
std::string ServerBoundCertService::GetDomainForHost(const std::string& host) {
  std::string domain =
      RegistryControlledDomainService::GetDomainAndRegistry(host);
  if (domain.empty())
    return host;
  return domain;
}

int ServerBoundCertService::GetDomainBoundCert(
    const std::string& origin,
    const std::vector<uint8>& requested_types,
    SSLClientCertType* type,
    std::string* private_key,
    std::string* cert,
    const CompletionCallback& callback,
    RequestHandle* out_req) {
  DCHECK(CalledOnValidThread());
  base::TimeTicks request_start = base::TimeTicks::Now();

  *out_req = NULL;

  if (callback.is_null() || !private_key || !cert || origin.empty() ||
      requested_types.empty()) {
    RecordGetDomainBoundCertResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  std::string domain = GetDomainForHost(GURL(origin).host());
  if (domain.empty()) {
    RecordGetDomainBoundCertResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  SSLClientCertType preferred_type = CLIENT_CERT_INVALID_TYPE;
  for (size_t i = 0; i < requested_types.size(); ++i) {
    if (IsSupportedCertType(requested_types[i])) {
      preferred_type = static_cast<SSLClientCertType>(requested_types[i]);
      break;
    }
  }
  if (preferred_type == CLIENT_CERT_INVALID_TYPE) {
    RecordGetDomainBoundCertResult(UNSUPPORTED_TYPE);
    // None of the requested types are supported.
    return ERR_CLIENT_AUTH_CERT_TYPE_UNSUPPORTED;
  }

  requests_++;

  // Check if a domain bound cert of an acceptable type already exists for this
  // domain, and that it has not expired.
  base::Time now = base::Time::Now();
  base::Time creation_time;
  base::Time expiration_time;
  if (server_bound_cert_store_->GetServerBoundCert(domain,
                                                   type,
                                                   &creation_time,
                                                   &expiration_time,
                                                   private_key,
                                                   cert)) {
    if (expiration_time < now) {
      DVLOG(1) << "Cert store had expired cert for " << domain;
    } else if (!IsSupportedCertType(*type) ||
               std::find(requested_types.begin(), requested_types.end(),
                         *type) == requested_types.end()) {
      DVLOG(1) << "Cert store had cert of wrong type " << *type << " for "
               << domain;
    } else {
      cert_store_hits_++;
      RecordGetDomainBoundCertResult(SYNC_SUCCESS);
      base::TimeDelta request_time = base::TimeTicks::Now() - request_start;
      UMA_HISTOGRAM_TIMES("DomainBoundCerts.GetCertTimeSync", request_time);
      RecordGetCertTime(request_time);
      return OK;
    }
  }

  // |server_bound_cert_store_| has no cert for this domain. See if an
  // identical request is currently in flight.
  ServerBoundCertServiceJob* job = NULL;
  std::map<std::string, ServerBoundCertServiceJob*>::const_iterator j;
  j = inflight_.find(domain);
  if (j != inflight_.end()) {
    // An identical request is in flight already. We'll just attach our
    // callback.
    job = j->second;
    // Check that the job is for an acceptable type of cert.
    if (std::find(requested_types.begin(), requested_types.end(), job->type())
        == requested_types.end()) {
      DVLOG(1) << "Found inflight job of wrong type " << job->type()
               << " for " << domain;
      // If we get here, the server is asking for different types of certs in
      // short succession.  This probably means the server is broken or
      // misconfigured.  Since we only store one type of cert per domain, we
      // are unable to handle this well.  Just return an error and let the first
      // job finish.
      RecordGetDomainBoundCertResult(TYPE_MISMATCH);
      return ERR_ORIGIN_BOUND_CERT_GENERATION_TYPE_MISMATCH;
    }
    inflight_joins_++;
  } else {
    // Need to make a new request.
    ServerBoundCertServiceWorker* worker = new ServerBoundCertServiceWorker(
            domain,
            preferred_type,
            this);
    job = new ServerBoundCertServiceJob(worker, preferred_type);
    if (!worker->Start(task_runner_)) {
      delete job;
      delete worker;
      // TODO(rkn): Log to the NetLog.
      LOG(ERROR) << "ServerBoundCertServiceWorker couldn't be started.";
      RecordGetDomainBoundCertResult(WORKER_FAILURE);
      return ERR_INSUFFICIENT_RESOURCES;  // Just a guess.
    }
    inflight_[domain] = job;
  }

  ServerBoundCertServiceRequest* request = new ServerBoundCertServiceRequest(
      request_start, callback, type, private_key, cert);
  job->AddRequest(request);
  *out_req = request;
  return ERR_IO_PENDING;
}

ServerBoundCertStore* ServerBoundCertService::GetCertStore() {
  return server_bound_cert_store_.get();
}

// static
int ServerBoundCertService::GenerateCert(const std::string& server_identifier,
                                         SSLClientCertType type,
                                         uint32 serial_number,
                                         base::Time* creation_time,
                                         base::Time* expiration_time,
                                         std::string* private_key,
                                         std::string* cert) {
  base::TimeTicks start = base::TimeTicks::Now();
  base::Time not_valid_before = base::Time::Now();
  base::Time not_valid_after =
      not_valid_before + base::TimeDelta::FromDays(kValidityPeriodInDays);
  std::string der_cert;
  std::vector<uint8> private_key_info;
  switch (type) {
    case CLIENT_CERT_ECDSA_SIGN: {
      scoped_ptr<crypto::ECPrivateKey> key(crypto::ECPrivateKey::Create());
      if (!key.get()) {
        DLOG(ERROR) << "Unable to create key pair for client";
        return ERR_KEY_GENERATION_FAILED;
      }
      if (!x509_util::CreateDomainBoundCertEC(
          key.get(),
          server_identifier,
          serial_number,
          not_valid_before,
          not_valid_after,
          &der_cert)) {
        DLOG(ERROR) << "Unable to create x509 cert for client";
        return ERR_ORIGIN_BOUND_CERT_GENERATION_FAILED;
      }

      if (!key->ExportEncryptedPrivateKey(
          kEPKIPassword, 1, &private_key_info)) {
        DLOG(ERROR) << "Unable to export private key";
        return ERR_PRIVATE_KEY_EXPORT_FAILED;
      }
      break;
    }
    default:
      NOTREACHED();
      return ERR_INVALID_ARGUMENT;
  }

  // TODO(rkn): Perhaps ExportPrivateKey should be changed to output a
  // std::string* to prevent this copying.
  std::string key_out(private_key_info.begin(), private_key_info.end());

  private_key->swap(key_out);
  cert->swap(der_cert);
  *creation_time = not_valid_before;
  *expiration_time = not_valid_after;
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GenerateCertTime",
                             base::TimeTicks::Now() - start,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(5),
                             50);
  return OK;
}

void ServerBoundCertService::CancelRequest(RequestHandle req) {
  DCHECK(CalledOnValidThread());
  ServerBoundCertServiceRequest* request =
      reinterpret_cast<ServerBoundCertServiceRequest*>(req);
  request->Cancel();
}

// HandleResult is called by ServerBoundCertServiceWorker on the origin message
// loop. It deletes ServerBoundCertServiceJob.
void ServerBoundCertService::HandleResult(const std::string& server_identifier,
                                          int error,
                                          SSLClientCertType type,
                                          base::Time creation_time,
                                          base::Time expiration_time,
                                          const std::string& private_key,
                                          const std::string& cert) {
  DCHECK(CalledOnValidThread());

  if (error == OK) {
    server_bound_cert_store_->SetServerBoundCert(
        server_identifier, type, creation_time, expiration_time, private_key,
        cert);
  }

  std::map<std::string, ServerBoundCertServiceJob*>::iterator j;
  j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  ServerBoundCertServiceJob* job = j->second;
  inflight_.erase(j);

  job->HandleResult(error, type, private_key, cert);
  delete job;
}

int ServerBoundCertService::cert_count() {
  return server_bound_cert_store_->GetCertCount();
}

}  // namespace net
