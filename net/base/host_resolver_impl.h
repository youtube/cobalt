// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_RESOLVER_IMPL_H_
#define NET_BASE_HOST_RESOLVER_IMPL_H_

#include <string>
#include <vector>

#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_proc.h"

namespace net {

// For each hostname that is requested, HostResolver creates a
// HostResolverImpl::Job. This job gets dispatched to a thread in the global
// WorkerPool, where it runs SystemHostResolverProc(). If requests for that same
// host are made while the job is already outstanding, then they are attached
// to the existing job rather than creating a new one. This avoids doing
// parallel resolves for the same host.
//
// The way these classes fit together is illustrated by:
//
//
//            +----------- HostResolverImpl -------------+
//            |                    |                     |
//           Job                  Job                   Job
//    (for host1, fam1)    (for host2, fam2)     (for hostx, famx)
//       /    |   |            /   |   |             /   |   |
//   Request ... Request  Request ... Request   Request ... Request
//  (port1)     (port2)  (port3)      (port4)  (port5)      (portX)
//
//
// When a HostResolverImpl::Job finishes its work in the threadpool, the
// callbacks of each waiting request are run on the origin thread.
//
// Thread safety: This class is not threadsafe, and must only be called
// from one thread!
//
class HostResolverImpl : public HostResolver {
 public:
  // Creates a HostResolver that caches up to |max_cache_entries| for
  // |cache_duration_ms| milliseconds. |resolver_proc| is used to perform
  // the actual resolves; it must be thread-safe since it is run from
  // multiple worker threads. If |resolver_proc| is NULL then the default
  // host resolver procedure is used (which is SystemHostResolverProc except
  // if overridden)
  HostResolverImpl(HostResolverProc* resolver_proc,
                   int max_cache_entries,
                   int cache_duration_ms);

  // If any completion callbacks are pending when the resolver is destroyed,
  // the host resolutions are cancelled, and the completion callbacks will not
  // be called.
  virtual ~HostResolverImpl();

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      LoadLog* load_log);
  virtual void CancelRequest(RequestHandle req);
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual HostCache* GetHostCache();

  // TODO(eroman): temp hack for http://crbug.com/15513
  virtual void Shutdown();

  virtual void SetDefaultAddressFamily(AddressFamily address_family) {
    default_address_family_ = address_family;
  }

 private:
  class Job;
  class Request;
  typedef std::vector<Request*> RequestsList;
  typedef HostCache::Key Key;
  typedef std::map<Key, scoped_refptr<Job> > JobMap;
  typedef std::vector<Observer*> ObserversList;

  // Returns the HostResolverProc to use for this instance.
  HostResolverProc* effective_resolver_proc() const {
    return resolver_proc_ ?
        resolver_proc_.get() : HostResolverProc::GetDefault();
  }

  // Adds a job to outstanding jobs list.
  void AddOutstandingJob(Job* job);

  // Returns the outstanding job for |key|, or NULL if there is none.
  Job* FindOutstandingJob(const Key& key);

  // Removes |job| from the outstanding jobs list.
  void RemoveOutstandingJob(Job* job);

  // Callback for when |job| has completed with |error| and |addrlist|.
  void OnJobComplete(Job* job, int error, const AddressList& addrlist);

  // Called when a request has just been started.
  void OnStartRequest(LoadLog* load_log,
                      int request_id,
                      const RequestInfo& info);

  // Called when a request has just completed (before its callback is run).
  void OnFinishRequest(LoadLog* load_log,
                       int request_id,
                       const RequestInfo& info,
                       int error);

  // Called when a request has been cancelled.
  void OnCancelRequest(LoadLog* load_log,
                       int request_id,
                       const RequestInfo& info);

  // Cache of host resolution results.
  HostCache cache_;

  // Map from hostname to outstanding job.
  JobMap jobs_;

  // The job that OnJobComplete() is currently processing (needed in case
  // HostResolver gets deleted from within the callback).
  scoped_refptr<Job> cur_completing_job_;

  // The observers to notify when a request starts/ends.
  ObserversList observers_;

  // Monotonically increasing ID number to assign to the next request.
  // Observers are the only consumers of this ID number.
  int next_request_id_;

  // The procedure to use for resolving host names. This will be NULL, except
  // in the case of unit-tests which inject custom host resolving behaviors.
  scoped_refptr<HostResolverProc> resolver_proc_;

  // Address family to use when the request doesn't specify one.
  AddressFamily default_address_family_;

  // TODO(eroman): temp hack for http://crbug.com/15513
  bool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(HostResolverImpl);
};

}  // namespace net

#endif  // NET_BASE_HOST_RESOLVER_IMPL_H_
