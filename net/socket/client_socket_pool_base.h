// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "net/base/host_resolver.h"
#include "net/base/load_states.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_pool.h"

namespace net {

class ClientSocketHandle;
class ClientSocketPoolBase;

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

  ConnectJob(const std::string& group_name,
             const ClientSocketHandle* key_handle,
             Delegate* delegate);
  virtual ~ConnectJob();

  // Accessors
  const std::string& group_name() const { return group_name_; }
  LoadState load_state() const { return load_state_; }
  const ClientSocketHandle* key_handle() const { return key_handle_; }

  // Releases |socket_| to the client.  On connection error, this should return
  // NULL.
  ClientSocket* ReleaseSocket() { return socket_.release(); }

  // Begins connecting the socket.  Returns OK on success, ERR_IO_PENDING if it
  // cannot complete synchronously without blocking, or another net error code
  // on error.  In asynchronous completion, the ConnectJob will notify
  // |delegate_| via OnConnectJobComplete.  In both asynchronous and synchronous
  // completion, ReleaseSocket() can be called to acquire the connected socket
  // if it succeeded.
  virtual int Connect() = 0;

 protected:
  void set_load_state(LoadState load_state) { load_state_ = load_state; }
  void set_socket(ClientSocket* socket) { socket_.reset(socket); }
  ClientSocket* socket() { return socket_.get(); }
  Delegate* delegate() { return delegate_; }

 private:
  const std::string group_name_;
  // Temporarily needed until we switch to late binding.
  const ClientSocketHandle* const key_handle_;
  Delegate* const delegate_;
  LoadState load_state_;
  scoped_ptr<ClientSocket> socket_;

  DISALLOW_COPY_AND_ASSIGN(ConnectJob);
};

// A ClientSocketPoolBase is used to restrict the number of sockets open at
// a time.  It also maintains a list of idle persistent sockets.
//
class ClientSocketPoolBase
    : public base::RefCounted<ClientSocketPoolBase>,
      public ConnectJob::Delegate {
 public:
  // A Request is allocated per call to RequestSocket that results in
  // ERR_IO_PENDING.
  struct Request {
    // HostResolver::RequestInfo has no default constructor, so fudge something.
    Request()
        : handle(NULL),
          callback(NULL),
          priority(0),
          resolve_info(std::string(), 0) {}

    Request(ClientSocketHandle* handle,
            CompletionCallback* callback,
            int priority,
            const HostResolver::RequestInfo& resolve_info)
        : handle(handle), callback(callback), priority(priority),
          resolve_info(resolve_info) {
    }

    ClientSocketHandle* handle;
    CompletionCallback* callback;
    int priority;
    HostResolver::RequestInfo resolve_info;
  };

  class ConnectJobFactory {
   public:
    ConnectJobFactory() {}
    virtual ~ConnectJobFactory() {}

    virtual ConnectJob* NewConnectJob(
        const std::string& group_name,
        const Request& request,
        ConnectJob::Delegate* delegate) const = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectJobFactory);
  };

  ClientSocketPoolBase(int max_sockets_per_group,
                       ConnectJobFactory* connect_job_factory);

  ~ClientSocketPoolBase();

  int RequestSocket(const std::string& group_name,
                    const HostResolver::RequestInfo& resolve_info,
                    int priority,
                    ClientSocketHandle* handle,
                    CompletionCallback* callback);

  void CancelRequest(const std::string& group_name,
                     const ClientSocketHandle* handle);

  void ReleaseSocket(const std::string& group_name,
                     ClientSocket* socket);

  void CloseIdleSockets();

  int idle_socket_count() const {
    return idle_socket_count_;
  }

  int max_sockets_per_group() const {
    return max_sockets_per_group_;
  }

  int IdleSocketCountInGroup(const std::string& group_name) const;

  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const;

  virtual void OnConnectJobComplete(int result, ConnectJob* job);

  // Enables late binding of sockets.  In this mode, socket requests are
  // decoupled from socket connection jobs.  A socket request may initiate a
  // socket connection job, but there is no guarantee that that socket
  // connection will service the request (for example, a released socket may
  // service the request sooner, or a higher priority request may come in
  // afterward and receive the socket from the job).
  static void EnableLateBindingOfSockets(bool enabled);

 private:
  // Entry for a persistent socket which became idle at time |start_time|.
  struct IdleSocket {
    IdleSocket() : socket(NULL), used(false) {}
    ClientSocket* socket;
    base::TimeTicks start_time;
    bool used;  // Indicates whether or not the socket has been used yet.

    // An idle socket should be removed if it can't be reused, or has been idle
    // for too long. |now| is the current time value (TimeTicks::Now()).
    //
    // An idle socket can't be reused if it is disconnected or has received
    // data unexpectedly (hence no longer idle).  The unread data would be
    // mistaken for the beginning of the next response if we were to reuse the
    // socket for a new request.
    bool ShouldCleanup(base::TimeTicks now) const;
  };

  typedef std::deque<Request> RequestQueue;
  typedef std::map<const ClientSocketHandle*, Request> RequestMap;

  // A Group is allocated per group_name when there are idle sockets or pending
  // requests.  Otherwise, the Group object is removed from the map.
  struct Group {
    Group() : active_socket_count(0) {}

    bool IsEmpty() const {
      return active_socket_count == 0 && idle_sockets.empty() && jobs.empty();
    }

    bool HasAvailableSocketSlot(int max_sockets_per_group) const {
      return active_socket_count + static_cast<int>(jobs.size()) <
          max_sockets_per_group;
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

  static void InsertRequestIntoQueue(const Request& r,
                                     RequestQueue* pending_requests);

  // Closes all idle sockets if |force| is true.  Else, only closes idle
  // sockets that timed out or can't be reused.
  void CleanupIdleSockets(bool force);

  // Called when the number of idle sockets changes.
  void IncrementIdleCount();
  void DecrementIdleCount();

  // Called via PostTask by ReleaseSocket.
  void DoReleaseSocket(const std::string& group_name, ClientSocket* socket);

  // Called when timer_ fires.  This method scans the idle sockets removing
  // sockets that timed out or can't be reused.
  void OnCleanupTimerFired() {
    CleanupIdleSockets(false);
  }

  // Removes the ConnectJob corresponding to |handle| from the
  // |connect_job_map_| or |connect_job_set_| depending on whether or not late
  // binding is enabled.  |job| must be non-NULL when late binding is
  // enabled.  Also updates |group| if non-NULL.
  void RemoveConnectJob(const ClientSocketHandle* handle,
                        ConnectJob* job,
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
                     Group* group);

  // Adds |socket| to the list of idle sockets for |group|.  |used| indicates
  // whether or not the socket has previously been used.
  void AddIdleSocket(ClientSocket* socket, bool used, Group* group);

  // Iterates through |connect_job_map_|, canceling all ConnectJobs.
  // Afterwards, it iterates through all groups and deletes them if they are no
  // longer needed.
  void CancelAllConnectJobs();

  GroupMap group_map_;

  ConnectJobMap connect_job_map_;

  // Timer used to periodically prune idle sockets that timed out or can't be
  // reused.
  base::RepeatingTimer<ClientSocketPoolBase> timer_;

  // The total number of idle sockets in the system.
  int idle_socket_count_;

  // The maximum number of sockets kept per group.
  const int max_sockets_per_group_;

  const scoped_ptr<ConnectJobFactory> connect_job_factory_;

  // Controls whether or not we use late binding of sockets.
  static bool g_late_binding;

  DISALLOW_COPY_AND_ASSIGN(ClientSocketPoolBase);
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_POOL_BASE_H_
