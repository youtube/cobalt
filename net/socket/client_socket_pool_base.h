// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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

#include <deque>
#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/load_log.h"
#include "net/base/load_states.h"
#include "net/base/net_errors.h"
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
             const ClientSocketHandle* key_handle,
             base::TimeDelta timeout_duration,
             Delegate* delegate,
             LoadLog* load_log);
  virtual ~ConnectJob();

  // Accessors
  const std::string& group_name() const { return group_name_; }
  const ClientSocketHandle* key_handle() const { return key_handle_; }
  LoadLog* load_log() { return load_log_; }

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

  virtual LoadState GetLoadState() const = 0;

 protected:
  void set_socket(ClientSocket* socket) { socket_.reset(socket); }
  ClientSocket* socket() { return socket_.get(); }
  void NotifyDelegateOfCompletion(int rv);

 private:
  virtual int ConnectInternal() = 0;

  // Alerts the delegate that the ConnectJob has timed out.
  void OnTimeout();

  const std::string group_name_;
  // Temporarily needed until we switch to late binding.
  const ClientSocketHandle* const key_handle_;
  const base::TimeDelta timeout_duration_;
  // Timer to abort jobs that take too long.
  base::OneShotTimer<ConnectJob> timer_;
  Delegate* delegate_;
  scoped_ptr<ClientSocket> socket_;
  scoped_refptr<LoadLog> load_log_;

  DISALLOW_COPY_AND_ASSIGN(ConnectJob);
};

namespace internal {

// ClientSocketPoolBaseHelper is an internal class that implements almost all
// the functionality from ClientSocketPoolBase without using templates.
// ClientSocketPoolBase adds templated definitions built on top of
// ClientSocketPoolBaseHelper.  This class is not for external use, please use
// ClientSocketPoolBase instead.
class ClientSocketPoolBaseHelper
    : public base::RefCounted<ClientSocketPoolBaseHelper>,
      public ConnectJob::Delegate {
 public:
  class Request {
   public:
    Request(ClientSocketHandle* handle,
            CompletionCallback* callback,
            RequestPriority priority,
            LoadLog* load_log)
        : handle_(handle), callback_(callback), priority_(priority),
          load_log_(load_log) {}

    virtual ~Request() {}

    ClientSocketHandle* handle() const { return handle_; }
    CompletionCallback* callback() const { return callback_; }
    RequestPriority priority() const { return priority_; }
    LoadLog* load_log() const { return load_log_.get(); }

   private:
    ClientSocketHandle* const handle_;
    CompletionCallback* const callback_;
    const RequestPriority priority_;
    const scoped_refptr<LoadLog> load_log_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  class ConnectJobFactory {
   public:
    ConnectJobFactory() {}
    virtual ~ConnectJobFactory() {}

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const Request& request,
        ConnectJob::Delegate* delegate,
        LoadLog* load_log) const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectJobFactory);
  };

  ClientSocketPoolBaseHelper(int max_sockets,
                             int max_sockets_per_group,
                             base::TimeDelta unused_idle_socket_timeout,
                             base::TimeDelta used_idle_socket_timeout,
                             ConnectJobFactory* connect_job_factory);

  // See ClientSocketPool::RequestSocket for documentation on this function.
  // Note that |request| must be heap allocated.  If ERR_IO_PENDING is returned,
  // then ClientSocketPoolBaseHelper takes ownership of |request|.
  int RequestSocket(const std::string& group_name, const Request* request);

  // See ClientSocketPool::CancelRequest for documentation on this function.
  void CancelRequest(const std::string& group_name,
                     const ClientSocketHandle* handle);

  // See ClientSocketPool::ReleaseSocket for documentation on this function.
  void ReleaseSocket(const std::string& group_name,
                     ClientSocket* socket);

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

  // ConnectJob::Delegate methods:
  virtual void OnConnectJobComplete(int result, ConnectJob* job);

  // Enables late binding of sockets.  In this mode, socket requests are
  // decoupled from socket connection jobs.  A socket request may initiate a
  // socket connection job, but there is no guarantee that that socket
  // connection will service the request (for example, a released socket may
  // service the request sooner, or a higher priority request may come in
  // afterward and receive the socket from the job).
  static void EnableLateBindingOfSockets(bool enabled);

  // For testing.
  bool may_have_stalled_group() const { return may_have_stalled_group_; }

  int NumConnectJobsInGroup(const std::string& group_name) const {
    return group_map_.find(group_name)->second.jobs.size();
  }

  // Closes all idle sockets if |force| is true.  Else, only closes idle
  // sockets that timed out or can't be reused.  Made public for testing.
  void CleanupIdleSockets(bool force);

 private:
  friend class base::RefCounted<ClientSocketPoolBaseHelper>;

  ~ClientSocketPoolBaseHelper();

  // Entry for a persistent socket which became idle at time |start_time|.
  struct IdleSocket {
    IdleSocket() : socket(NULL), used(false) {}
    ClientSocket* socket;
    base::TimeTicks start_time;
    bool used;  // Indicates whether or not the socket has been used yet.

    // An idle socket should be removed if it can't be reused, or has been idle
    // for too long. |now| is the current time value (TimeTicks::Now()).
    // |timeout| is the length of time to wait before timing out an idle socket.
    //
    // An idle socket can't be reused if it is disconnected or has received
    // data unexpectedly (hence no longer idle).  The unread data would be
    // mistaken for the beginning of the next response if we were to reuse the
    // socket for a new request.
    bool ShouldCleanup(base::TimeTicks now, base::TimeDelta timeout) const;
  };

  typedef std::deque<const Request*> RequestQueue;
  typedef std::map<const ClientSocketHandle*, const Request*> RequestMap;

  // A Group is allocated per group_name when there are idle sockets or pending
  // requests.  Otherwise, the Group object is removed from the map.
  struct Group {
    Group() : active_socket_count(0) {}

    bool IsEmpty() const {
      return active_socket_count == 0 && idle_sockets.empty() && jobs.empty() &&
          pending_requests.empty() && connecting_requests.empty();
    }

    bool HasAvailableSocketSlot(int max_sockets_per_group) const {
      return active_socket_count + static_cast<int>(jobs.size()) <
          max_sockets_per_group;
    }

    RequestPriority TopPendingPriority() const {
      return pending_requests.front()->priority();
    }

    std::deque<IdleSocket> idle_sockets;
    std::set<const ConnectJob*> jobs;
    RequestQueue pending_requests;
    RequestMap connecting_requests;
    int active_socket_count;  // number of active sockets used by clients
  };

  typedef std::map<std::string, Group> GroupMap;

  typedef std::map<const ClientSocketHandle*, ConnectJob*> ConnectJobMap;
  typedef std::set<const ConnectJob*> ConnectJobSet;

  static void InsertRequestIntoQueue(const Request* r,
                                     RequestQueue* pending_requests);
  static const Request* RemoveRequestFromQueue(RequestQueue::iterator it,
                                               RequestQueue* pending_requests);

  // Called when the number of idle sockets changes.
  void IncrementIdleCount();
  void DecrementIdleCount();

  // Called via PostTask by ReleaseSocket.
  void DoReleaseSocket(const std::string& group_name, ClientSocket* socket);

  // Scans the group map for groups which have an available socket slot and
  // at least one pending request. Returns number of groups found, and if found
  // at least one, fills |group| and |group_name| with data of the stalled group
  // having highest priority.
  int FindTopStalledGroup(Group** group, std::string* group_name);

  // Called when timer_ fires.  This method scans the idle sockets removing
  // sockets that timed out or can't be reused.
  void OnCleanupTimerFired() {
    CleanupIdleSockets(false);
  }

  // Removes the ConnectJob corresponding to |handle| from the
  // |connect_job_map_| or |connect_job_set_| depending on whether or not late
  // binding is enabled.  |job| must be non-NULL when late binding is
  // enabled.  Also updates |group| if non-NULL.  When late binding is disabled,
  // this will also delete the Request from |group->connecting_requests|.
  void RemoveConnectJob(const ClientSocketHandle* handle,
                        const ConnectJob* job,
                        Group* group);

  // Same as OnAvailableSocketSlot except it looks up the Group first to see if
  // it's there.
  void MaybeOnAvailableSocketSlot(const std::string& group_name);

  // Might delete the Group from |group_map_|.
  void OnAvailableSocketSlot(const std::string& group_name, Group* group);

  // Process a request from a group's pending_requests queue.
  void ProcessPendingRequest(const std::string& group_name, Group* group);

  // Assigns |socket| to |handle| and updates |group|'s counters appropriately.
  void HandOutSocket(ClientSocket* socket,
                     bool reused,
                     ClientSocketHandle* handle,
                     base::TimeDelta time_idle,
                     Group* group);

  // Adds |socket| to the list of idle sockets for |group|.  |used| indicates
  // whether or not the socket has previously been used.
  void AddIdleSocket(ClientSocket* socket, bool used, Group* group);

  // Iterates through |connect_job_map_|, canceling all ConnectJobs.
  // Afterwards, it iterates through all groups and deletes them if they are no
  // longer needed.
  void CancelAllConnectJobs();

  // Returns true if we can't create any more sockets due to the total limit.
  // TODO(phajdan.jr): Also take idle sockets into account.
  bool ReachedMaxSocketsLimit() const;

  GroupMap group_map_;

  ConnectJobMap connect_job_map_;

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

  // Until the maximum number of sockets limit is reached, a group can only
  // have pending requests if it exceeds the "max sockets per group" limit.
  //
  // This means when a socket is released, the only pending requests that can
  // be started next belong to the same group.
  //
  // However once the |max_sockets_| limit is reached, this stops being true:
  // groups can now have pending requests without having first reached the
  // |max_sockets_per_group_| limit. So choosing the next request involves
  // selecting the highest priority request across *all* groups.
  //
  // Since reaching the maximum number of sockets is an edge case, we make note
  // of when it happens, and thus avoid doing the slower "scan all groups"
  // in the common case.
  bool may_have_stalled_group_;

  const scoped_ptr<ConnectJobFactory> connect_job_factory_;

  // Controls whether or not we use late binding of sockets.
  static bool g_late_binding;

};

}  // namespace internal

// The maximum duration, in seconds, to keep unused idle persistent sockets
// alive.
// TODO(willchan): Change this timeout after getting histogram data on how
// long it should be.
static const int kUnusedIdleSocketTimeout = 10;
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
            const SocketParams& params,
            LoadLog* load_log)
        : internal::ClientSocketPoolBaseHelper::Request(
            handle, callback, priority, load_log),
          params_(params) {}

    const SocketParams& params() const { return params_; }

   private:
    SocketParams params_;
  };

  class ConnectJobFactory {
   public:
    ConnectJobFactory() {}
    virtual ~ConnectJobFactory() {}

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const Request& request,
        ConnectJob::Delegate* delegate,
        LoadLog* load_log) const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectJobFactory);
  };

  // |max_sockets| is the maximum number of sockets to be maintained by this
  // ClientSocketPool.  |max_sockets_per_group| specifies the maximum number of
  // sockets a "group" can have.  |unused_idle_socket_timeout| specifies how
  // long to leave an unused idle socket open before closing it.
  // |used_idle_socket_timeout| specifies how long to leave a previously used
  // idle socket open before closing it.
  ClientSocketPoolBase(int max_sockets,
                       int max_sockets_per_group,
                       base::TimeDelta unused_idle_socket_timeout,
                       base::TimeDelta used_idle_socket_timeout,
                       ConnectJobFactory* connect_job_factory)
      : helper_(new internal::ClientSocketPoolBaseHelper(
          max_sockets, max_sockets_per_group,
          unused_idle_socket_timeout, used_idle_socket_timeout,
          new ConnectJobFactoryAdaptor(connect_job_factory))) {}

  virtual ~ClientSocketPoolBase() {}

  // These member functions simply forward to ClientSocketPoolBaseHelper.

  // RequestSocket bundles up the parameters into a Request and then forwards to
  // ClientSocketPoolBaseHelper::RequestSocket().  Note that the memory
  // ownership is transferred in the asynchronous (ERR_IO_PENDING) case.
  int RequestSocket(const std::string& group_name,
                    const SocketParams& params,
                    RequestPriority priority,
                    ClientSocketHandle* handle,
                    CompletionCallback* callback,
                    LoadLog* load_log) {
    scoped_ptr<Request> request(
        new Request(handle, callback, priority, params, load_log));
    LoadLog::BeginEvent(load_log, LoadLog::TYPE_SOCKET_POOL);
    int rv = helper_->RequestSocket(group_name, request.get());
    if (rv == ERR_IO_PENDING)
      request.release();
    else
      LoadLog::EndEvent(load_log, LoadLog::TYPE_SOCKET_POOL);
    return rv;
  }

  void CancelRequest(const std::string& group_name,
                     const ClientSocketHandle* handle) {
    return helper_->CancelRequest(group_name, handle);
  }

  void ReleaseSocket(const std::string& group_name, ClientSocket* socket) {
    return helper_->ReleaseSocket(group_name, socket);
  }

  void CloseIdleSockets() { return helper_->CloseIdleSockets(); }

  int idle_socket_count() const { return helper_->idle_socket_count(); }

  int IdleSocketCountInGroup(const std::string& group_name) const {
    return helper_->IdleSocketCountInGroup(group_name);
  }

  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const {
    return helper_->GetLoadState(group_name, handle);
  }

  virtual void OnConnectJobComplete(int result, ConnectJob* job) {
    return helper_->OnConnectJobComplete(result, job);
  }

  // For testing.
  bool may_have_stalled_group() const {
    return helper_->may_have_stalled_group();
  }

  int NumConnectJobsInGroup(const std::string& group_name) const {
    return helper_->NumConnectJobsInGroup(group_name);
  }

  void CleanupIdleSockets(bool force) {
    return helper_->CleanupIdleSockets(force);
  }

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

    explicit ConnectJobFactoryAdaptor(
        ConnectJobFactory* connect_job_factory)
        : connect_job_factory_(connect_job_factory) {}
    virtual ~ConnectJobFactoryAdaptor() {}

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const internal::ClientSocketPoolBaseHelper::Request& request,
        ConnectJob::Delegate* delegate,
        LoadLog* load_log) const {
      const Request* casted_request = static_cast<const Request*>(&request);
      return connect_job_factory_->NewConnectJob(
          group_name, *casted_request, delegate, load_log);
    }

    const scoped_ptr<ConnectJobFactory> connect_job_factory_;
  };

  // One might ask why ClientSocketPoolBaseHelper is also refcounted if its
  // containing ClientSocketPool is already refcounted.  The reason is because
  // DoReleaseSocket() posts a task.  If ClientSocketPool gets deleted between
  // the posting of the task and the execution, then we'll hit the DCHECK that
  // |ClientSocketPoolBaseHelper::group_map_| is empty.
  scoped_refptr<internal::ClientSocketPoolBaseHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolBase);
};

// Enables late binding of sockets.  In this mode, socket requests are
// decoupled from socket connection jobs.  A socket request may initiate a
// socket connection job, but there is no guarantee that that socket
// connection will service the request (for example, a released socket may
// service the request sooner, or a higher priority request may come in
// afterward and receive the socket from the job).
void EnableLateBindingOfSockets(bool enabled);

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_BASE_H_
