// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_handle.h"

using base::TimeDelta;

namespace {

// The timeout value, in seconds, used to clean up idle sockets that can't be
// reused.
//
// Note: It's important to close idle sockets that have received data as soon
// as possible because the received data may cause BSOD on Windows XP under
// some conditions.  See http://crbug.com/4606.
const int kCleanupInterval = 10;  // DO NOT INCREASE THIS TIMEOUT.

// The maximum size of the ConnectJob's LoadLog.
const int kMaxNumLoadLogEntries = 50;

}  // namespace

namespace net {

ConnectJob::ConnectJob(const std::string& group_name,
                       const ClientSocketHandle* key_handle,
                       base::TimeDelta timeout_duration,
                       Delegate* delegate,
                       LoadLog* load_log)
    : group_name_(group_name),
      key_handle_(key_handle),
      timeout_duration_(timeout_duration),
      delegate_(delegate),
      load_log_(load_log) {
  DCHECK(!group_name.empty());
  DCHECK(key_handle);
  DCHECK(delegate);
}

ConnectJob::~ConnectJob() {
  if (delegate_) {
    // If the delegate was not NULLed, then NotifyDelegateOfCompletion has
    // not been called yet (hence we are cancelling).
    LoadLog::AddEvent(load_log_, LoadLog::TYPE_CANCELLED);
    LoadLog::EndEvent(load_log_, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB);
  }
}

int ConnectJob::Connect() {
  if (timeout_duration_ != base::TimeDelta())
    timer_.Start(timeout_duration_, this, &ConnectJob::OnTimeout);

  LoadLog::BeginEvent(load_log_, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB);

  int rv = ConnectInternal();

  if (rv != ERR_IO_PENDING) {
    delegate_ = NULL;
    LoadLog::EndEvent(load_log_, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB);
  }

  return rv;
}

void ConnectJob::NotifyDelegateOfCompletion(int rv) {
  // The delegate will delete |this|.
  Delegate *delegate = delegate_;
  delegate_ = NULL;

  LoadLog::EndEvent(load_log_, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB);

  delegate->OnConnectJobComplete(rv, this);
}

void ConnectJob::OnTimeout() {
  // Make sure the socket is NULL before calling into |delegate|.
  set_socket(NULL);

  LoadLog::AddEvent(load_log_,
      LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB_TIMED_OUT);

  NotifyDelegateOfCompletion(ERR_TIMED_OUT);
}

namespace internal {

bool ClientSocketPoolBaseHelper::g_late_binding = false;

ClientSocketPoolBaseHelper::ClientSocketPoolBaseHelper(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    base::TimeDelta used_idle_socket_timeout,
    ConnectJobFactory* connect_job_factory,
    const scoped_refptr<NetworkChangeNotifier>& network_change_notifier)
    : idle_socket_count_(0),
      connecting_socket_count_(0),
      handed_out_socket_count_(0),
      max_sockets_(max_sockets),
      max_sockets_per_group_(max_sockets_per_group),
      unused_idle_socket_timeout_(unused_idle_socket_timeout),
      used_idle_socket_timeout_(used_idle_socket_timeout),
      may_have_stalled_group_(false),
      connect_job_factory_(connect_job_factory),
      network_change_notifier_(network_change_notifier) {
  DCHECK_LE(0, max_sockets_per_group);
  DCHECK_LE(max_sockets_per_group, max_sockets);

  if (network_change_notifier_)
    network_change_notifier_->AddObserver(this);
}

ClientSocketPoolBaseHelper::~ClientSocketPoolBaseHelper() {
  if (g_late_binding)
    CancelAllConnectJobs();
  // Clean up any idle sockets.  Assert that we have no remaining active
  // sockets or pending requests.  They should have all been cleaned up prior
  // to the manager being destroyed.
  CloseIdleSockets();
  DCHECK(group_map_.empty());
  DCHECK(connect_job_map_.empty());

  if (network_change_notifier_)
    network_change_notifier_->RemoveObserver(this);
}

// InsertRequestIntoQueue inserts the request into the queue based on
// priority.  Highest priorities are closest to the front.  Older requests are
// prioritized over requests of equal priority.
//
// static
void ClientSocketPoolBaseHelper::InsertRequestIntoQueue(
    const Request* r, RequestQueue* pending_requests) {
  LoadLog::BeginEvent(r->load_log(),
      LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE);

  RequestQueue::iterator it = pending_requests->begin();
  while (it != pending_requests->end() && r->priority() >= (*it)->priority())
    ++it;
  pending_requests->insert(it, r);
}

// static
const ClientSocketPoolBaseHelper::Request*
ClientSocketPoolBaseHelper::RemoveRequestFromQueue(
    RequestQueue::iterator it, RequestQueue* pending_requests) {
  const Request* req = *it;

  LoadLog::EndEvent(req->load_log(),
      LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE);

  pending_requests->erase(it);
  return req;
}

int ClientSocketPoolBaseHelper::RequestSocket(
    const std::string& group_name,
    const Request* request) {
  DCHECK_GE(request->priority(), 0);
  CompletionCallback* const callback = request->callback();
  CHECK(callback);
  ClientSocketHandle* const handle = request->handle();
  CHECK(handle);
  Group& group = group_map_[group_name];

  // Can we make another active socket now?
  if (ReachedMaxSocketsLimit() ||
      !group.HasAvailableSocketSlot(max_sockets_per_group_)) {
    if (ReachedMaxSocketsLimit()) {
      // We could check if we really have a stalled group here, but it requires
      // a scan of all groups, so just flip a flag here, and do the check later.
      may_have_stalled_group_ = true;

      LoadLog::AddEvent(request->load_log(),
          LoadLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS);
    } else {
      LoadLog::AddEvent(request->load_log(),
          LoadLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP);
    }
    InsertRequestIntoQueue(request, &group.pending_requests);
    return ERR_IO_PENDING;
  }

  while (!group.idle_sockets.empty()) {
    IdleSocket idle_socket = group.idle_sockets.back();
    group.idle_sockets.pop_back();
    DecrementIdleCount();
    if (idle_socket.socket->IsConnectedAndIdle()) {
      // We found one we can reuse!
      base::TimeDelta idle_time =
          base::TimeTicks::Now() - idle_socket.start_time;
      HandOutSocket(
          idle_socket.socket, idle_socket.used, handle, idle_time, &group);
      return OK;
    }
    delete idle_socket.socket;
  }

  // We couldn't find a socket to reuse, so allocate and connect a new one.

  // If we aren't using late binding, the job lines up with a request so
  // just write directly into the request's LoadLog.
  scoped_refptr<LoadLog> job_load_log = g_late_binding ?
      new LoadLog(kMaxNumLoadLogEntries) : request->load_log();

  scoped_ptr<ConnectJob> connect_job(
      connect_job_factory_->NewConnectJob(group_name, *request, this,
                                         job_load_log));

  int rv = connect_job->Connect();

  if (g_late_binding && rv != ERR_IO_PENDING && request->load_log())
    request->load_log()->Append(job_load_log);

  if (rv == OK) {
    HandOutSocket(connect_job->ReleaseSocket(), false /* not reused */,
                  handle, base::TimeDelta(), &group);
  } else if (rv == ERR_IO_PENDING) {
    connecting_socket_count_++;

    ConnectJob* job = connect_job.release();
    if (g_late_binding) {
      CHECK(!ContainsKey(connect_job_map_, handle));
      InsertRequestIntoQueue(request, &group.pending_requests);
    } else {
      group.connecting_requests[handle] = request;
      CHECK(!ContainsKey(connect_job_map_, handle));
      connect_job_map_[handle] = job;
    }
    group.jobs.insert(job);
  } else if (group.IsEmpty()) {
    group_map_.erase(group_name);
  }

  return rv;
}

void ClientSocketPoolBaseHelper::CancelRequest(
    const std::string& group_name, const ClientSocketHandle* handle) {
  CHECK(ContainsKey(group_map_, group_name));

  Group& group = group_map_[group_name];

  // Search pending_requests for matching handle.
  RequestQueue::iterator it = group.pending_requests.begin();
  for (; it != group.pending_requests.end(); ++it) {
    if ((*it)->handle() == handle) {
      const Request* req = RemoveRequestFromQueue(it, &group.pending_requests);
      LoadLog::AddEvent(req->load_log(), LoadLog::TYPE_CANCELLED);
      LoadLog::EndEvent(req->load_log(), LoadLog::TYPE_SOCKET_POOL);
      delete req;
      if (g_late_binding &&
          group.jobs.size() > group.pending_requests.size() + 1) {
        // TODO(willchan): Cancel the job in the earliest LoadState.
        RemoveConnectJob(handle, *group.jobs.begin(), &group);
        OnAvailableSocketSlot(group_name, &group);
      }
      return;
    }
  }

  if (!g_late_binding) {
    // It's invalid to cancel a non-existent request.
    CHECK(ContainsKey(group.connecting_requests, handle));

    RequestMap::iterator map_it = group.connecting_requests.find(handle);
    if (map_it != group.connecting_requests.end()) {
      scoped_refptr<LoadLog> log(map_it->second->load_log());
      LoadLog::AddEvent(log, LoadLog::TYPE_CANCELLED);
      LoadLog::EndEvent(log, LoadLog::TYPE_SOCKET_POOL);
      RemoveConnectJob(handle, NULL, &group);
      OnAvailableSocketSlot(group_name, &group);
    }
  }
}

void ClientSocketPoolBaseHelper::ReleaseSocket(const std::string& group_name,
                                               ClientSocket* socket) {
  // Run this asynchronously to allow the caller to finish before we let
  // another to begin doing work.  This also avoids nasty recursion issues.
  // NOTE: We cannot refer to the handle argument after this method returns.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &ClientSocketPoolBaseHelper::DoReleaseSocket, group_name, socket));
}

void ClientSocketPoolBaseHelper::CloseIdleSockets() {
  CleanupIdleSockets(true);
}

int ClientSocketPoolBaseHelper::IdleSocketCountInGroup(
    const std::string& group_name) const {
  GroupMap::const_iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  return i->second.idle_sockets.size();
}

LoadState ClientSocketPoolBaseHelper::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  if (!ContainsKey(group_map_, group_name)) {
    NOTREACHED() << "ClientSocketPool does not contain group: " << group_name
                 << " for handle: " << handle;
    return LOAD_STATE_IDLE;
  }

  // Can't use operator[] since it is non-const.
  const Group& group = group_map_.find(group_name)->second;

  // Search connecting_requests for matching handle.
  RequestMap::const_iterator map_it = group.connecting_requests.find(handle);
  if (map_it != group.connecting_requests.end()) {
    ConnectJobMap::const_iterator job_it = connect_job_map_.find(handle);
    if (job_it == connect_job_map_.end()) {
      NOTREACHED();
      return LOAD_STATE_IDLE;
    }
    return job_it->second->GetLoadState();
  }

  // Search pending_requests for matching handle.
  RequestQueue::const_iterator it = group.pending_requests.begin();
  for (size_t i = 0; it != group.pending_requests.end(); ++it, ++i) {
    if ((*it)->handle() == handle) {
      if (g_late_binding && i < group.jobs.size()) {
        LoadState max_state = LOAD_STATE_IDLE;
        for (ConnectJobSet::const_iterator job_it = group.jobs.begin();
             job_it != group.jobs.end(); ++job_it) {
          max_state = std::max(max_state, (*job_it)->GetLoadState());
        }
        return max_state;
      } else {
        // TODO(wtc): Add a state for being on the wait list.
        // See http://www.crbug.com/5077.
        return LOAD_STATE_IDLE;
      }
    }
  }

  NOTREACHED();
  return LOAD_STATE_IDLE;
}

bool ClientSocketPoolBaseHelper::IdleSocket::ShouldCleanup(
    base::TimeTicks now,
    base::TimeDelta timeout) const {
  bool timed_out = (now - start_time) >= timeout;
  return timed_out ||
      !(used ? socket->IsConnectedAndIdle() : socket->IsConnected());
}

void ClientSocketPoolBaseHelper::CleanupIdleSockets(bool force) {
  if (idle_socket_count_ == 0)
    return;

  // Current time value. Retrieving it once at the function start rather than
  // inside the inner loop, since it shouldn't change by any meaningful amount.
  base::TimeTicks now = base::TimeTicks::Now();

  GroupMap::iterator i = group_map_.begin();
  while (i != group_map_.end()) {
    Group& group = i->second;

    std::deque<IdleSocket>::iterator j = group.idle_sockets.begin();
    while (j != group.idle_sockets.end()) {
      base::TimeDelta timeout =
          j->used ? used_idle_socket_timeout_ : unused_idle_socket_timeout_;
      if (force || j->ShouldCleanup(now, timeout)) {
        delete j->socket;
        j = group.idle_sockets.erase(j);
        DecrementIdleCount();
      } else {
        ++j;
      }
    }

    // Delete group if no longer needed.
    if (group.IsEmpty()) {
      group_map_.erase(i++);
    } else {
      ++i;
    }
  }
}

void ClientSocketPoolBaseHelper::IncrementIdleCount() {
  if (++idle_socket_count_ == 1)
    timer_.Start(TimeDelta::FromSeconds(kCleanupInterval), this,
                 &ClientSocketPoolBaseHelper::OnCleanupTimerFired);
}

void ClientSocketPoolBaseHelper::DecrementIdleCount() {
  if (--idle_socket_count_ == 0)
    timer_.Stop();
}

void ClientSocketPoolBaseHelper::DoReleaseSocket(const std::string& group_name,
                                                 ClientSocket* socket) {
  GroupMap::iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  Group& group = i->second;

  CHECK(handed_out_socket_count_ > 0);
  handed_out_socket_count_--;

  CHECK(group.active_socket_count > 0);
  group.active_socket_count--;

  const bool can_reuse = socket->IsConnectedAndIdle();
  if (can_reuse) {
    AddIdleSocket(socket, true /* used socket */, &group);
  } else {
    delete socket;
  }

  OnAvailableSocketSlot(group_name, &group);
}

// Search for the highest priority pending request, amongst the groups that
// are not at the |max_sockets_per_group_| limit. Note: for requests with
// the same priority, the winner is based on group hash ordering (and not
// insertion order).
int ClientSocketPoolBaseHelper::FindTopStalledGroup(Group** group,
                                                    std::string* group_name) {
  Group* top_group = NULL;
  const std::string* top_group_name = NULL;
  int stalled_group_count = 0;
  for (GroupMap::iterator i = group_map_.begin();
       i != group_map_.end(); ++i) {
    Group& group = i->second;
    const RequestQueue& queue = group.pending_requests;
    if (queue.empty())
      continue;
    bool has_slot = group.HasAvailableSocketSlot(max_sockets_per_group_);
    if (has_slot)
      stalled_group_count++;
    bool has_higher_priority = !top_group ||
        group.TopPendingPriority() < top_group->TopPendingPriority();
    if (has_slot && has_higher_priority) {
      top_group = &group;
      top_group_name = &i->first;
    }
  }
  if (top_group) {
    *group = top_group;
    *group_name = *top_group_name;
  }
  return stalled_group_count;
}

void ClientSocketPoolBaseHelper::OnConnectJobComplete(
    int result, ConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);
  const std::string group_name = job->group_name();
  GroupMap::iterator group_it = group_map_.find(group_name);
  CHECK(group_it != group_map_.end());
  Group& group = group_it->second;

  const ClientSocketHandle* const key_handle = job->key_handle();
  scoped_ptr<ClientSocket> socket(job->ReleaseSocket());

  if (g_late_binding) {
    scoped_refptr<LoadLog> job_load_log(job->load_log());
    RemoveConnectJob(key_handle, job, &group);

    scoped_ptr<const Request> r;
    if (!group.pending_requests.empty()) {
      r.reset(RemoveRequestFromQueue(
          group.pending_requests.begin(), &group.pending_requests));

      if (r->load_log())
        r->load_log()->Append(job_load_log);

      LoadLog::EndEvent(r->load_log(), LoadLog::TYPE_SOCKET_POOL);
    }

    if (result == OK) {
      DCHECK(socket.get());
      if (r.get()) {
        HandOutSocket(
            socket.release(), false /* unused socket */, r->handle(),
            base::TimeDelta(), &group);
        r->callback()->Run(result);
      } else {
        AddIdleSocket(socket.release(), false /* unused socket */, &group);
        OnAvailableSocketSlot(group_name, &group);
      }
    } else {
      DCHECK(!socket.get());
      if (r.get())
        r->callback()->Run(result);
      MaybeOnAvailableSocketSlot(group_name);
    }

    return;
  }

  RequestMap* request_map = &group.connecting_requests;
  RequestMap::iterator it = request_map->find(key_handle);
  CHECK(it != request_map->end());
  const Request* request = it->second;
  ClientSocketHandle* const handle = request->handle();
  CompletionCallback* const callback = request->callback();

  LoadLog::EndEvent(request->load_log(), LoadLog::TYPE_SOCKET_POOL);

  RemoveConnectJob(key_handle, job, &group);

  if (result != OK) {
    DCHECK(!socket.get());
    callback->Run(result);  // |group| is not necessarily valid after this.
    // |group| may be invalid after the callback, we need to search
    // |group_map_| again.
    MaybeOnAvailableSocketSlot(group_name);
  } else {
    DCHECK(socket.get());
    HandOutSocket(socket.release(), false /* not reused */, handle,
                  base::TimeDelta(), &group);
    callback->Run(result);
  }
}

void ClientSocketPoolBaseHelper::OnIPAddressChanged() {
  CloseIdleSockets();
}

void ClientSocketPoolBaseHelper::EnableLateBindingOfSockets(bool enabled) {
  g_late_binding = enabled;
}

void ClientSocketPoolBaseHelper::RemoveConnectJob(
    const ClientSocketHandle* handle, const ConnectJob *job, Group* group) {
  CHECK(connecting_socket_count_ > 0);
  connecting_socket_count_--;

  if (g_late_binding) {
    DCHECK(job);
    delete job;
  } else {
    ConnectJobMap::iterator it = connect_job_map_.find(handle);
    CHECK(it != connect_job_map_.end());
    job = it->second;
    delete job;
    connect_job_map_.erase(it);
    RequestMap::iterator map_it = group->connecting_requests.find(handle);
    CHECK(map_it != group->connecting_requests.end());
    const Request* request = map_it->second;
    delete request;
    group->connecting_requests.erase(map_it);
  }

  if (group) {
    DCHECK(ContainsKey(group->jobs, job));
    group->jobs.erase(job);
  }
}

void ClientSocketPoolBaseHelper::MaybeOnAvailableSocketSlot(
    const std::string& group_name) {
  GroupMap::iterator it = group_map_.find(group_name);
  if (it != group_map_.end()) {
    Group& group = it->second;
    if (group.HasAvailableSocketSlot(max_sockets_per_group_))
      OnAvailableSocketSlot(group_name, &group);
  }
}

void ClientSocketPoolBaseHelper::OnAvailableSocketSlot(
    const std::string& group_name, Group* group) {
  if (may_have_stalled_group_) {
    std::string top_group_name;
    Group* top_group = NULL;
    int stalled_group_count = FindTopStalledGroup(&top_group, &top_group_name);
    if (stalled_group_count <= 1)
      may_have_stalled_group_ = false;
    if (stalled_group_count >= 1)
      ProcessPendingRequest(top_group_name, top_group);
  } else if (!group->pending_requests.empty()) {
    ProcessPendingRequest(group_name, group);
    // |group| may no longer be valid after this point.  Be careful not to
    // access it again.
  } else if (group->IsEmpty()) {
    // Delete |group| if no longer needed.  |group| will no longer be valid.
    group_map_.erase(group_name);
  }
}

void ClientSocketPoolBaseHelper::ProcessPendingRequest(
    const std::string& group_name, Group* group) {
  scoped_ptr<const Request> r(RemoveRequestFromQueue(
        group->pending_requests.begin(), &group->pending_requests));

  int rv = RequestSocket(group_name, r.get());

  if (rv != ERR_IO_PENDING) {
    LoadLog::EndEvent(r->load_log(), LoadLog::TYPE_SOCKET_POOL);
    r->callback()->Run(rv);
    if (rv != OK) {
      // |group| may be invalid after the callback, we need to search
      // |group_map_| again.
      MaybeOnAvailableSocketSlot(group_name);
    }
  } else {
    r.release();
  }
}

void ClientSocketPoolBaseHelper::HandOutSocket(
    ClientSocket* socket,
    bool reused,
    ClientSocketHandle* handle,
    base::TimeDelta idle_time,
    Group* group) {
  DCHECK(socket);
  handle->set_socket(socket);
  handle->set_is_reused(reused);
  handle->set_idle_time(idle_time);

  handed_out_socket_count_++;
  group->active_socket_count++;
}

void ClientSocketPoolBaseHelper::AddIdleSocket(
    ClientSocket* socket, bool used, Group* group) {
  DCHECK(socket);
  IdleSocket idle_socket;
  idle_socket.socket = socket;
  idle_socket.start_time = base::TimeTicks::Now();
  idle_socket.used = used;

  group->idle_sockets.push_back(idle_socket);
  IncrementIdleCount();
}

void ClientSocketPoolBaseHelper::CancelAllConnectJobs() {
  for (GroupMap::iterator i = group_map_.begin(); i != group_map_.end();) {
    Group& group = i->second;
    STLDeleteElements(&group.jobs);

    // Delete group if no longer needed.
    if (group.IsEmpty()) {
      group_map_.erase(i++);
    } else {
      ++i;
    }
  }
}

bool ClientSocketPoolBaseHelper::ReachedMaxSocketsLimit() const {
  // Each connecting socket will eventually connect and be handed out.
  int total = handed_out_socket_count_ + connecting_socket_count_;
  DCHECK_LE(total, max_sockets_);
  return total == max_sockets_;
}

}  // namespace internal

void EnableLateBindingOfSockets(bool enabled) {
  internal::ClientSocketPoolBaseHelper::EnableLateBindingOfSockets(enabled);
}

}  // namespace net
