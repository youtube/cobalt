// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A ClientSocketPoolBase is used to restrict the number of sockets open at
// a time.  It also maintains a list of idle persistent sockets for reuse.
// Subclasses of ClientSocketPool should compose ClientSocketPoolBase to handle
// the core logic of (1) restricting the number of active (connected or
// connecting) sockets per "group" (generally speaking, the hostname), (2)
// maintaining a per-group list of idle, persistent sockets for reuse, and (3)
// limiting the total number of active sockets in the system.
//
// ClientSocketPoolBase abstracts socket connection details behind ConnectJob,
// ConnectJobFactory, and SocketParams.  When a socket "slot" becomes available,
// the ClientSocketPoolBase will ask the ConnectJobFactory to create a
// ConnectJob with a SocketParams.  Subclasses of ClientSocketPool should
// implement their socket specific connection by subclassing ConnectJob and
// implementing ConnectJob::ConnectInternal().  They can control the parameters
// passed to each new ConnectJob instance via their ConnectJobFactory subclass
// and templated SocketParams parameter.
//
#ifndef NET_SOCKET_CLIENT_SOCKET_POOL_BASE_H_
#define NET_SOCKET_CLIENT_SOCKET_POOL_BASE_H_
#pragma once

#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/network_change_notifier.h"
#include "net/base/request_priority.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_pool.h"

namespace net {

class ClientSocketHandle;

// ConnectJob provides an abstract interface for "connecting" a socket.
// The connection may involve host resolution, tcp connection, ssl connection,
// etc.
class ConnectJob {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Alerts the delegate that the connection completed.
    virtual void OnConnectJobComplete(int result, ConnectJob* job) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // A |timeout_duration| of 0 corresponds to no timeout.
  ConnectJob(const std::string& group_name,
             base::TimeDelta timeout_duration,
             Delegate* delegate,
             const BoundNetLog& net_log);
  virtual ~ConnectJob();

  // Accessors
  const std::string& group_name() const { return group_name_; }
  const BoundNetLog& net_log() { return net_log_; }
  bool is_preconnect() const { return preconnect_state_ != NOT_PRECONNECT; }
  bool is_unused_preconnect() const {
    return preconnect_state_ == UNUSED_PRECONNECT;
  }

  // Initialized by the ClientSocketPoolBaseHelper.
  // TODO(willchan): Move most of the constructor arguments over here.  We
  // shouldn't give the ConnectJobFactory (subclasses) the ability to screw up
  // the initialization.
  void Initialize(bool is_preconnect);

  // Releases |socket_| to the client.  On connection error, this should return
  // NULL.
  ClientSocket* ReleaseSocket() { return socket_.release(); }

  // Begins connecting the socket.  Returns OK on success, ERR_IO_PENDING if it
  // cannot complete synchronously without blocking, or another net error code
  // on error.  In asynchronous completion, the ConnectJob will notify
  // |delegate_| via OnConnectJobComplete.  In both asynchronous and synchronous
  // completion, ReleaseSocket() can be called to acquire the connected socket
  // if it succeeded.
  int Connect();

  // Precondition: is_unused_preconnect() must be true.  Marks the job as a
  // used preconnect job.
  void UseForNormalRequest();

  virtual LoadState GetLoadState() const = 0;

  // If Connect returns an error (or OnConnectJobComplete reports an error
  // result) this method will be called, allowing the pool to add
  // additional error state to the ClientSocketHandle (post late-binding).
  virtual void GetAdditionalErrorState(ClientSocketHandle* handle) {}

  const BoundNetLog& net_log() const { return net_log_; }

 protected:
  void set_socket(ClientSocket* socket);
  ClientSocket* socket() { return socket_.get(); }
  void NotifyDelegateOfCompletion(int rv);
  void ResetTimer(base::TimeDelta remainingTime);

 private:
  enum PreconnectState {
    NOT_PRECONNECT,
    UNUSED_PRECONNECT,
    USED_PRECONNECT,
  };

  virtual int ConnectInternal() = 0;

  void LogConnectStart();
  void LogConnectCompletion(int net_error);

  // Alerts the delegate that the ConnectJob has timed out.
  void OnTimeout();

  const std::string group_name_;
  const base::TimeDelta timeout_duration_;
  // Timer to abort jobs that take too long.
  base::OneShotTimer<ConnectJob> timer_;
  Delegate* delegate_;
  scoped_ptr<ClientSocket> socket_;
  BoundNetLog net_log_;
  // A ConnectJob is idle until Connect() has been called.
  bool idle_;
  PreconnectState preconnect_state_;

  DISALLOW_COPY_AND_ASSIGN(ConnectJob);
};

namespace internal {

// ClientSocketPoolBaseHelper is an internal class that implements almost all
// the functionality from ClientSocketPoolBase without using templates.
// ClientSocketPoolBase adds templated definitions built on top of
// ClientSocketPoolBaseHelper.  This class is not for external use, please use
// ClientSocketPoolBase instead.
class ClientSocketPoolBaseHelper
    : public ConnectJob::Delegate,
      public NetworkChangeNotifier::IPAddressObserver {
 public:
  typedef uint32 Flags;

  // Used to specify specific behavior for the ClientSocketPool.
  enum Flag {
    NORMAL = 0,  // Normal behavior.
    NO_IDLE_SOCKETS = 0x1,  // Do not return an idle socket. Create a new one.
  };

  class Request {
   public:
    Request(ClientSocketHandle* handle,
            CompletionCallback* callback,
            RequestPriority priority,
            Flags flags,
            const BoundNetLog& net_log);

    virtual ~Request();

    ClientSocketHandle* handle() const { return handle_; }
    CompletionCallback* callback() const { return callback_; }
    RequestPriority priority() const { return priority_; }
    Flags flags() const { return flags_; }
    const BoundNetLog& net_log() const { return net_log_; }

   private:
    ClientSocketHandle* const handle_;
    CompletionCallback* const callback_;
    const RequestPriority priority_;
    const Flags flags_;
    BoundNetLog net_log_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  class ConnectJobFactory {
   public:
    ConnectJobFactory() {}
    virtual ~ConnectJobFactory() {}

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const Request& request,
        ConnectJob::Delegate* delegate) const = 0;

    virtual base::TimeDelta ConnectionTimeout() const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectJobFactory);
  };

  ClientSocketPoolBaseHelper(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      ConnectJobFactory* connect_job_factory);

  ~ClientSocketPoolBaseHelper();

  // See ClientSocketPool::RequestSocket for documentation on this function.
  // ClientSocketPoolBaseHelper takes ownership of |request|, which must be
  // heap allocated.
  int RequestSocket(const std::string& group_name, const Request* request);

  // See ClientSocketPool::RequestSocket for documentation on this function.
  void RequestSockets(const std::string& group_name,
                      const Request& request,
                      int num_sockets);

  // See ClientSocketPool::CancelRequest for documentation on this function.
  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle);

  // See ClientSocketPool::ReleaseSocket for documentation on this function.
  void ReleaseSocket(const std::string& group_name,
                     ClientSocket* socket,
                     int id);

  // See ClientSocketPool::Flush for documentation on this function.
  void Flush();

  // See ClientSocketPool::CloseIdleSockets for documentation on this function.
  void CloseIdleSockets();

  // See ClientSocketPool::IdleSocketCount() for documentation on this function.
  int idle_socket_count() const {
    return idle_socket_count_;
  }

  // See ClientSocketPool::IdleSocketCountInGroup() for documentation on this
  // function.
  int IdleSocketCountInGroup(const std::string& group_name) const;

  // See ClientSocketPool::GetLoadState() for documentation on this function.
  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const;

  int ConnectRetryIntervalMs() const {
    // TODO(mbelshe): Make this tuned dynamically based on measured RTT.
    //                For now, just use the max retry interval.
    return ClientSocketPool::kMaxConnectRetryIntervalMs;
  }

  int NumConnectJobsInGroup(const std::string& group_name) const {
    return group_map_.find(group_name)->second->jobs().size();
  }

  int NumActiveSocketsInGroup(const std::string& group_name) const {
    return group_map_.find(group_name)->second->active_socket_count();
  }

  bool HasGroup(const std::string& group_name) const;

  // Closes all idle sockets if |force| is true.  Else, only closes idle
  // sockets that timed out or can't be reused.  Made public for testing.
  void CleanupIdleSockets(bool force);

  // See ClientSocketPool::GetInfoAsValue for documentation on this function.
  DictionaryValue* GetInfoAsValue(const std::string& name,
                                  const std::string& type) const;

  base::TimeDelta ConnectionTimeout() const {
    return connect_job_factory_->ConnectionTimeout();
  }

  static bool connect_backup_jobs_enabled();
  static void set_connect_backup_jobs_enabled(bool enabled);

  void EnableConnectBackupJobs();

  // ConnectJob::Delegate methods:
  virtual void OnConnectJobComplete(int result, ConnectJob* job);

  // NetworkChangeNotifier::IPAddressObserver methods:
  virtual void OnIPAddressChanged();

 private:
  friend class base::RefCounted<ClientSocketPoolBaseHelper>;

  // Entry for a persistent socket which became idle at time |start_time|.
  struct IdleSocket {
    IdleSocket() : socket(NULL) {}

    // An idle socket should be removed if it can't be reused, or has been idle
    // for too long. |now| is the current time value (TimeTicks::Now()).
    // |timeout| is the length of time to wait before timing out an idle socket.
    //
    // An idle socket can't be reused if it is disconnected or has received
    // data unexpectedly (hence no longer idle).  The unread data would be
    // mistaken for the beginning of the next response if we were to reuse the
    // socket for a new request.
    bool ShouldCleanup(base::TimeTicks now, base::TimeDelta timeout) const;

    ClientSocket* socket;
    base::TimeTicks start_time;
  };

  typedef std::deque<const Request* > RequestQueue;
  typedef std::map<const ClientSocketHandle*, const Request*> RequestMap;

  // A Group is allocated per group_name when there are idle sockets or pending
  // requests.  Otherwise, the Group object is removed from the map.
  // |active_socket_count| tracks the number of sockets held by clients.
  class Group {
   public:
    Group();
    ~Group();

    bool IsEmpty() const {
      return active_socket_count_ == 0 && idle_sockets_.empty() &&
          jobs_.empty() && pending_requests_.empty();
    }

    bool HasAvailableSocketSlot(int max_sockets_per_group) const {
      return NumActiveSocketSlots() < max_sockets_per_group;
    }

    int NumActiveSocketSlots() const {
      return active_socket_count_ + static_cast<int>(jobs_.size()) +
          static_cast<int>(idle_sockets_.size());
    }

    bool IsStalled(int max_sockets_per_group) const {
      return HasAvailableSocketSlot(max_sockets_per_group) &&
          pending_requests_.size() > jobs_.size();
    }

    RequestPriority TopPendingPriority() const {
      return pending_requests_.front()->priority();
    }

    bool HasBackupJob() const { return !method_factory_.empty(); }

    void CleanupBackupJob() {
      method_factory_.RevokeAll();
    }

    // Set a timer to create a backup socket if it takes too long to create one.
    void StartBackupSocketTimer(const std::string& group_name,
                                ClientSocketPoolBaseHelper* pool);

    // Searches |jobs_| to see if there's a preconnect ConnectJob, and if so,
    // uses it.  Returns true on success.  Otherwise, returns false.
    bool TryToUsePreconnectConnectJob();

    void AddJob(ConnectJob* job) { jobs_.insert(job); }
    void RemoveJob(ConnectJob* job) { jobs_.erase(job); }
    void RemoveAllJobs();

    void IncrementActiveSocketCount() { active_socket_count_++; }
    void DecrementActiveSocketCount() { active_socket_count_--; }

    const std::set<ConnectJob*>& jobs() const { return jobs_; }
    const std::list<IdleSocket>& idle_sockets() const { return idle_sockets_; }
    const RequestQueue& pending_requests() const { return pending_requests_; }
    int active_socket_count() const { return active_socket_count_; }
    RequestQueue* mutable_pending_requests() { return &pending_requests_; }
    std::list<IdleSocket>* mutable_idle_sockets() { return &idle_sockets_; }

   private:
    // Called when the backup socket timer fires.
    void OnBackupSocketTimerFired(
        std::string group_name,
        ClientSocketPoolBaseHelper* pool);

    std::list<IdleSocket> idle_sockets_;
    std::set<ConnectJob*> jobs_;
    RequestQueue pending_requests_;
    int active_socket_count_;  // number of active sockets used by clients
    // A factory to pin the backup_job tasks.
    ScopedRunnableMethodFactory<Group> method_factory_;
  };

  typedef std::map<std::string, Group*> GroupMap;

  typedef std::set<ConnectJob*> ConnectJobSet;

  struct CallbackResultPair {
    CallbackResultPair() : callback(NULL), result(OK) {}
    CallbackResultPair(CompletionCallback* callback_in, int result_in)
        : callback(callback_in), result(result_in) {}

    CompletionCallback* callback;
    int result;
  };

  typedef std::map<const ClientSocketHandle*, CallbackResultPair>
      PendingCallbackMap;

  static void InsertRequestIntoQueue(const Request* r,
                                     RequestQueue* pending_requests);
  static const Request* RemoveRequestFromQueue(RequestQueue::iterator it,
                                               Group* group);

  Group* GetOrCreateGroup(const std::string& group_name);
  void RemoveGroup(const std::string& group_name);
  void RemoveGroup(GroupMap::iterator it);

  // Called when the number of idle sockets changes.
  void IncrementIdleCount();
  void DecrementIdleCount();

  // Scans the group map for groups which have an available socket slot and
  // at least one pending request. Returns true if any groups are stalled, and
  // if so, fills |group| and |group_name| with data of the stalled group
  // having highest priority.
  bool FindTopStalledGroup(Group** group, std::string* group_name);

  // Called when timer_ fires.  This method scans the idle sockets removing
  // sockets that timed out or can't be reused.
  void OnCleanupTimerFired() {
    CleanupIdleSockets(false);
  }

  // Removes |job| from |connect_job_set_|.  Also updates |group| if non-NULL.
  void RemoveConnectJob(ConnectJob* job, Group* group);

  // Tries to see if we can handle any more requests for |group|.
  void OnAvailableSocketSlot(const std::string& group_name, Group* group);

  // Process a pending socket request for a group.
  void ProcessPendingRequest(const std::string& group_name, Group* group);

  // Assigns |socket| to |handle| and updates |group|'s counters appropriately.
  void HandOutSocket(ClientSocket* socket,
                     bool reused,
                     ClientSocketHandle* handle,
                     base::TimeDelta time_idle,
                     Group* group,
                     const BoundNetLog& net_log);

  // Adds |socket| to the list of idle sockets for |group|.
  void AddIdleSocket(ClientSocket* socket, Group* group);

  // Iterates through |group_map_|, canceling all ConnectJobs and deleting
  // groups if they are no longer needed.
  void CancelAllConnectJobs();

  // Iterates through |group_map_|, posting ERR_ABORTED callbacks for all
  // requests, and then deleting groups if they are no longer needed.
  void AbortAllRequests();

  // Returns true if we can't create any more sockets due to the total limit.
  bool ReachedMaxSocketsLimit() const;

  // This is the internal implementation of RequestSocket().  It differs in that
  // it does not handle logging into NetLog of the queueing status of
  // |request|.
  int RequestSocketInternal(const std::string& group_name,
                            const Request* request);

  // Assigns an idle socket for the group to the request.
  // Returns |true| if an idle socket is available, false otherwise.
  bool AssignIdleSocketToGroup(const Request* request, Group* group);

  static void LogBoundConnectJobToRequest(
      const NetLog::Source& connect_job_source, const Request* request);

  // Closes one idle socket.  Picks the first one encountered.
  // TODO(willchan): Consider a better algorithm for doing this.  Perhaps we
  // should keep an ordered list of idle sockets, and close them in order.
  // Requires maintaining more state.  It's not clear if it's worth it since
  // I'm not sure if we hit this situation often.
  void CloseOneIdleSocket();

  // Same as CloseOneIdleSocket() except it won't close an idle socket in
  // |group|.  If |group| is NULL, it is ignored.  Returns true if it closed a
  // socket.
  bool CloseOneIdleSocketExceptInGroup(const Group* group);

  // Checks if there are stalled socket groups that should be notified
  // for possible wakeup.
  void CheckForStalledSocketGroups();

  // Posts a task to call InvokeUserCallback() on the next iteration through the
  // current message loop.  Inserts |callback| into |pending_callback_map_|,
  // keyed by |handle|.
  void InvokeUserCallbackLater(
      ClientSocketHandle* handle, CompletionCallback* callback, int rv);

  // Invokes the user callback for |handle|.  By the time this task has run,
  // it's possible that the request has been cancelled, so |handle| may not
  // exist in |pending_callback_map_|.  We look up the callback and result code
  // in |pending_callback_map_|.
  void InvokeUserCallback(ClientSocketHandle* handle);

  GroupMap group_map_;

  // Map of the ClientSocketHandles for which we have a pending Task to invoke a
  // callback.  This is necessary since, before we invoke said callback, it's
  // possible that the request is cancelled.
  PendingCallbackMap pending_callback_map_;

  // Timer used to periodically prune idle sockets that timed out or can't be
  // reused.
  base::RepeatingTimer<ClientSocketPoolBaseHelper> timer_;

  // The total number of idle sockets in the system.
  int idle_socket_count_;

  // Number of connecting sockets across all groups.
  int connecting_socket_count_;

  // Number of connected sockets we handed out across all groups.
  int handed_out_socket_count_;

  // The maximum total number of sockets. See ReachedMaxSocketsLimit.
  const int max_sockets_;

  // The maximum number of sockets kept per group.
  const int max_sockets_per_group_;

  // The time to wait until closing idle sockets.
  const base::TimeDelta unused_idle_socket_timeout_;
  const base::TimeDelta used_idle_socket_timeout_;

  const scoped_ptr<ConnectJobFactory> connect_job_factory_;

  // TODO(vandebo) Remove when backup jobs move to TCPClientSocketPool
  bool connect_backup_jobs_enabled_;

  // A unique id for the pool.  It gets incremented every time we Flush() the
  // pool.  This is so that when sockets get released back to the pool, we can
  // make sure that they are discarded rather than reused.
  int pool_generation_number_;

  ScopedRunnableMethodFactory<ClientSocketPoolBaseHelper> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolBaseHelper);
};

}  // namespace internal

// The maximum duration, in seconds, to keep used idle persistent sockets alive.
static const int kUsedIdleSocketTimeout = 300;  // 5 minutes

template <typename SocketParams>
class ClientSocketPoolBase {
 public:
  class Request : public internal::ClientSocketPoolBaseHelper::Request {
   public:
    Request(ClientSocketHandle* handle,
            CompletionCallback* callback,
            RequestPriority priority,
            internal::ClientSocketPoolBaseHelper::Flags flags,
            const scoped_refptr<SocketParams>& params,
            const BoundNetLog& net_log)
        : internal::ClientSocketPoolBaseHelper::Request(
              handle, callback, priority, flags, net_log),
          params_(params) {}

    const scoped_refptr<SocketParams>& params() const { return params_; }

   private:
    const scoped_refptr<SocketParams> params_;
  };

  class ConnectJobFactory {
   public:
    ConnectJobFactory() {}
    virtual ~ConnectJobFactory() {}

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const Request& request,
        ConnectJob::Delegate* delegate) const = 0;

    virtual base::TimeDelta ConnectionTimeout() const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectJobFactory);
  };

  // |max_sockets| is the maximum number of sockets to be maintained by this
  // ClientSocketPool.  |max_sockets_per_group| specifies the maximum number of
  // sockets a "group" can have.  |unused_idle_socket_timeout| specifies how
  // long to leave an unused idle socket open before closing it.
  // |used_idle_socket_timeout| specifies how long to leave a previously used
  // idle socket open before closing it.
  ClientSocketPoolBase(
      int max_sockets,
      int max_sockets_per_group,
      ClientSocketPoolHistograms* histograms,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      ConnectJobFactory* connect_job_factory)
      : histograms_(histograms),
        helper_(max_sockets, max_sockets_per_group,
                unused_idle_socket_timeout, used_idle_socket_timeout,
                new ConnectJobFactoryAdaptor(connect_job_factory)) {}

  virtual ~ClientSocketPoolBase() {}

  // These member functions simply forward to ClientSocketPoolBaseHelper.

  // RequestSocket bundles up the parameters into a Request and then forwards to
  // ClientSocketPoolBaseHelper::RequestSocket().
  int RequestSocket(const std::string& group_name,
                    const scoped_refptr<SocketParams>& params,
                    RequestPriority priority,
                    ClientSocketHandle* handle,
                    CompletionCallback* callback,
                    const BoundNetLog& net_log) {
    Request* request =
        new Request(handle, callback, priority,
                    internal::ClientSocketPoolBaseHelper::NORMAL,
                    params, net_log);
    return helper_.RequestSocket(group_name, request);
  }

  // RequestSockets bundles up the parameters into a Request and then forwards
  // to ClientSocketPoolBaseHelper::RequestSockets().  Note that it assigns the
  // priority to LOWEST and specifies the NO_IDLE_SOCKETS flag.
  void RequestSockets(const std::string& group_name,
                      const scoped_refptr<SocketParams>& params,
                      int num_sockets,
                      const BoundNetLog& net_log) {
    const Request request(NULL /* no handle */,
                          NULL /* no callback */,
                          LOWEST,
                          internal::ClientSocketPoolBaseHelper::NO_IDLE_SOCKETS,
                          params,
                          net_log);
    helper_.RequestSockets(group_name, request, num_sockets);
  }

  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle) {
    return helper_.CancelRequest(group_name, handle);
  }

  void ReleaseSocket(const std::string& group_name, ClientSocket* socket,
                     int id) {
    return helper_.ReleaseSocket(group_name, socket, id);
  }

  void CloseIdleSockets() { return helper_.CloseIdleSockets(); }

  int idle_socket_count() const { return helper_.idle_socket_count(); }

  int IdleSocketCountInGroup(const std::string& group_name) const {
    return helper_.IdleSocketCountInGroup(group_name);
  }

  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const {
    return helper_.GetLoadState(group_name, handle);
  }

  virtual void OnConnectJobComplete(int result, ConnectJob* job) {
    return helper_.OnConnectJobComplete(result, job);
  }

  int NumConnectJobsInGroup(const std::string& group_name) const {
    return helper_.NumConnectJobsInGroup(group_name);
  }

  int NumActiveSocketsInGroup(const std::string& group_name) const {
    return helper_.NumActiveSocketsInGroup(group_name);
  }

  bool HasGroup(const std::string& group_name) const {
    return helper_.HasGroup(group_name);
  }

  void CleanupIdleSockets(bool force) {
    return helper_.CleanupIdleSockets(force);
  }

  DictionaryValue* GetInfoAsValue(const std::string& name,
                                  const std::string& type) const {
    return helper_.GetInfoAsValue(name, type);
  }

  base::TimeDelta ConnectionTimeout() const {
    return helper_.ConnectionTimeout();
  }

  ClientSocketPoolHistograms* histograms() const {
    return histograms_;
  }

  void EnableConnectBackupJobs() { helper_.EnableConnectBackupJobs(); }

  void Flush() { helper_.Flush(); }

 private:
  // This adaptor class exists to bridge the
  // internal::ClientSocketPoolBaseHelper::ConnectJobFactory and
  // ClientSocketPoolBase::ConnectJobFactory types, allowing clients to use the
  // typesafe ClientSocketPoolBase::ConnectJobFactory, rather than having to
  // static_cast themselves.
  class ConnectJobFactoryAdaptor
      : public internal::ClientSocketPoolBaseHelper::ConnectJobFactory {
   public:
    typedef typename ClientSocketPoolBase<SocketParams>::ConnectJobFactory
        ConnectJobFactory;

    explicit ConnectJobFactoryAdaptor(ConnectJobFactory* connect_job_factory)
        : connect_job_factory_(connect_job_factory) {}
    virtual ~ConnectJobFactoryAdaptor() {}

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const internal::ClientSocketPoolBaseHelper::Request& request,
        ConnectJob::Delegate* delegate) const {
      const Request* casted_request = static_cast<const Request*>(&request);
      return connect_job_factory_->NewConnectJob(
          group_name, *casted_request, delegate);
    }

    virtual base::TimeDelta ConnectionTimeout() const {
      return connect_job_factory_->ConnectionTimeout();
    }

    const scoped_ptr<ConnectJobFactory> connect_job_factory_;
  };

  // Histograms for the pool
  ClientSocketPoolHistograms* const histograms_;
  internal::ClientSocketPoolBaseHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolBase);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_BASE_H_
