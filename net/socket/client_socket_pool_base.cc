// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "net/base/net_log.h"
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

}  // namespace

namespace net {

ConnectJob::ConnectJob(const std::string& group_name,
                       base::TimeDelta timeout_duration,
                       Delegate* delegate,
                       const BoundNetLog& net_log)
    : group_name_(group_name),
      timeout_duration_(timeout_duration),
      delegate_(delegate),
      net_log_(net_log),
      idle_(true) {
  DCHECK(!group_name.empty());
  DCHECK(delegate);
  net_log.BeginEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB, NULL);
}

ConnectJob::~ConnectJob() {
  net_log().EndEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB, NULL);
}

int ConnectJob::Connect() {
  if (timeout_duration_ != base::TimeDelta())
    timer_.Start(timeout_duration_, this, &ConnectJob::OnTimeout);

  idle_ = false;

  LogConnectStart();

  int rv = ConnectInternal();

  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    delegate_ = NULL;
  }

  return rv;
}

void ConnectJob::set_socket(ClientSocket* socket) {
  if (socket) {
    net_log().AddEvent(NetLog::TYPE_CONNECT_JOB_SET_SOCKET,
                       new NetLogSourceParameter("source_dependency",
                                                 socket->NetLog().source()));
  }
  socket_.reset(socket);
}

void ConnectJob::NotifyDelegateOfCompletion(int rv) {
  // The delegate will delete |this|.
  Delegate *delegate = delegate_;
  delegate_ = NULL;

  LogConnectCompletion(rv);
  delegate->OnConnectJobComplete(rv, this);
}

void ConnectJob::ResetTimer(base::TimeDelta remaining_time) {
  timer_.Stop();
  timer_.Start(remaining_time, this, &ConnectJob::OnTimeout);
}

void ConnectJob::LogConnectStart() {
  net_log().BeginEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_CONNECT,
                       new NetLogStringParameter("group_name", group_name_));
}

void ConnectJob::LogConnectCompletion(int net_error) {
  scoped_refptr<NetLog::EventParameters> params;
  if (net_error != OK)
    params = new NetLogIntegerParameter("net_error", net_error);
  net_log().EndEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_CONNECT, params);
}

void ConnectJob::OnTimeout() {
  // Make sure the socket is NULL before calling into |delegate|.
  set_socket(NULL);

  net_log_.AddEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_TIMED_OUT, NULL);

  NotifyDelegateOfCompletion(ERR_TIMED_OUT);
}

namespace internal {

ClientSocketPoolBaseHelper::Request::Request(
    ClientSocketHandle* handle,
    CompletionCallback* callback,
    RequestPriority priority,
    const BoundNetLog& net_log)
    : handle_(handle), callback_(callback), priority_(priority),
      net_log_(net_log) {}

ClientSocketPoolBaseHelper::Request::~Request() {}

ClientSocketPoolBaseHelper::ClientSocketPoolBaseHelper(
    int max_sockets,
    int max_sockets_per_group,
    base::TimeDelta unused_idle_socket_timeout,
    base::TimeDelta used_idle_socket_timeout,
    ConnectJobFactory* connect_job_factory)
    : idle_socket_count_(0),
      connecting_socket_count_(0),
      handed_out_socket_count_(0),
      num_releasing_sockets_(0),
      max_sockets_(max_sockets),
      max_sockets_per_group_(max_sockets_per_group),
      unused_idle_socket_timeout_(unused_idle_socket_timeout),
      used_idle_socket_timeout_(used_idle_socket_timeout),
      may_have_stalled_group_(false),
      connect_job_factory_(connect_job_factory),
      backup_jobs_enabled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      pool_generation_number_(0) {
  DCHECK_LE(0, max_sockets_per_group);
  DCHECK_LE(max_sockets_per_group, max_sockets);

  NetworkChangeNotifier::AddObserver(this);
}

ClientSocketPoolBaseHelper::~ClientSocketPoolBaseHelper() {
  CancelAllConnectJobs();

  // Clean up any idle sockets.  Assert that we have no remaining active
  // sockets or pending requests.  They should have all been cleaned up prior
  // to the manager being destroyed.
  CloseIdleSockets();
  CHECK(group_map_.empty());
  DCHECK_EQ(0, connecting_socket_count_);

  NetworkChangeNotifier::RemoveObserver(this);
}

// InsertRequestIntoQueue inserts the request into the queue based on
// priority.  Highest priorities are closest to the front.  Older requests are
// prioritized over requests of equal priority.
//
// static
void ClientSocketPoolBaseHelper::InsertRequestIntoQueue(
    const Request* r, RequestQueue* pending_requests) {
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
  pending_requests->erase(it);
  return req;
}

int ClientSocketPoolBaseHelper::RequestSocket(
    const std::string& group_name,
    const Request* request) {
  request->net_log().BeginEvent(NetLog::TYPE_SOCKET_POOL, NULL);
  Group& group = group_map_[group_name];
  int rv = RequestSocketInternal(group_name, request);
  if (rv != ERR_IO_PENDING) {
    request->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL, NULL);
    delete request;
  } else {
    InsertRequestIntoQueue(request, &group.pending_requests);
  }
  return rv;
}

int ClientSocketPoolBaseHelper::RequestSocketInternal(
    const std::string& group_name,
    const Request* request) {
  DCHECK_GE(request->priority(), 0);
  CompletionCallback* const callback = request->callback();
  CHECK(callback);
  ClientSocketHandle* const handle = request->handle();
  CHECK(handle);
  Group& group = group_map_[group_name];

  // Try to reuse a socket.
  while (!group.idle_sockets.empty()) {
    IdleSocket idle_socket = group.idle_sockets.back();
    group.idle_sockets.pop_back();
    DecrementIdleCount();
    if (idle_socket.socket->IsConnectedAndIdle()) {
      // We found one we can reuse!
      base::TimeDelta idle_time =
          base::TimeTicks::Now() - idle_socket.start_time;
      HandOutSocket(
          idle_socket.socket, idle_socket.used, handle, idle_time, &group,
          request->net_log());
      return OK;
    }
    delete idle_socket.socket;
  }

  // Can we make another active socket now?
  if (!group.HasAvailableSocketSlot(max_sockets_per_group_)) {
    request->net_log().AddEvent(
        NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP, NULL);
    return ERR_IO_PENDING;
  }

  if (ReachedMaxSocketsLimit()) {
    if (idle_socket_count() > 0) {
      CloseOneIdleSocket();
    } else {
      // We could check if we really have a stalled group here, but it requires
      // a scan of all groups, so just flip a flag here, and do the check later.
      may_have_stalled_group_ = true;
      request->net_log().AddEvent(
          NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS, NULL);
      return ERR_IO_PENDING;
    }
  }

  // See if we already have enough connect jobs or sockets that will be released
  // soon.
  if (group.HasReleasingSockets()) {
    return ERR_IO_PENDING;
  }

  // We couldn't find a socket to reuse, so allocate and connect a new one.
  scoped_ptr<ConnectJob> connect_job(
      connect_job_factory_->NewConnectJob(group_name, *request, this));

  int rv = connect_job->Connect();
  if (rv == OK) {
    LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
    HandOutSocket(connect_job->ReleaseSocket(), false /* not reused */,
                  handle, base::TimeDelta(), &group, request->net_log());
  } else if (rv == ERR_IO_PENDING) {
    // If we don't have any sockets in this group, set a timer for potentially
    // creating a new one.  If the SYN is lost, this backup socket may complete
    // before the slow socket, improving end user latency.
    if (group.IsEmpty() && !group.backup_job && backup_jobs_enabled_) {
      group.backup_job = connect_job_factory_->NewConnectJob(group_name,
                                                             *request,
                                                             this);
      StartBackupSocketTimer(group_name);
    }

    connecting_socket_count_++;

    ConnectJob* job = connect_job.release();
    group.jobs.insert(job);
  } else {
    LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
    if (group.IsEmpty())
      group_map_.erase(group_name);
  }

  return rv;
}

// static
void ClientSocketPoolBaseHelper::LogBoundConnectJobToRequest(
    const NetLog::Source& connect_job_source, const Request* request) {
  request->net_log().AddEvent(
      NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      new NetLogSourceParameter("source_dependency", connect_job_source));
}

void ClientSocketPoolBaseHelper::StartBackupSocketTimer(
    const std::string& group_name) {
  CHECK(ContainsKey(group_map_, group_name));
  Group& group = group_map_[group_name];

  // Only allow one timer pending to create a backup socket.
  if (group.backup_task)
    return;

  group.backup_task = method_factory_.NewRunnableMethod(
      &ClientSocketPoolBaseHelper::OnBackupSocketTimerFired, group_name);
  MessageLoop::current()->PostDelayedTask(FROM_HERE, group.backup_task,
                                          ConnectRetryIntervalMs());
}

void ClientSocketPoolBaseHelper::OnBackupSocketTimerFired(
    const std::string& group_name) {
  CHECK(ContainsKey(group_map_, group_name));

  Group& group = group_map_[group_name];

  CHECK(group.backup_task);
  group.backup_task = NULL;

  CHECK(group.backup_job);

  // If there are no more jobs pending, there is no work to do.
  // If we've done our cleanups correctly, this should not happen.
  if (group.jobs.empty()) {
    NOTREACHED();
    return;
  }

  // If our backup job is waiting on DNS, or if we can't create any sockets
  // right now due to limits, just reset the timer.
  if (ReachedMaxSocketsLimit() ||
      !group.HasAvailableSocketSlot(max_sockets_per_group_) ||
      (*group.jobs.begin())->GetLoadState() == LOAD_STATE_RESOLVING_HOST) {
    StartBackupSocketTimer(group_name);
    return;
  }

  group.backup_job->net_log().AddEvent(NetLog::TYPE_SOCKET_BACKUP_CREATED,
                                       NULL);
  SIMPLE_STATS_COUNTER("socket.backup_created");
  int rv = group.backup_job->Connect();
  connecting_socket_count_++;
  group.jobs.insert(group.backup_job);
  ConnectJob* job = group.backup_job;
  group.backup_job = NULL;
  if (rv != ERR_IO_PENDING)
    OnConnectJobComplete(rv, job);
}

void ClientSocketPoolBaseHelper::CancelRequest(
    const std::string& group_name, const ClientSocketHandle* handle) {
  // Running callbacks can cause the last outside reference to be released.
  // Hold onto a reference.
  scoped_refptr<ClientSocketPoolBaseHelper> ref_holder(this);

  CHECK(ContainsKey(group_map_, group_name));

  Group& group = group_map_[group_name];

  // Search pending_requests for matching handle.
  RequestQueue::iterator it = group.pending_requests.begin();
  for (; it != group.pending_requests.end(); ++it) {
    if ((*it)->handle() == handle) {
      const Request* req = RemoveRequestFromQueue(it, &group.pending_requests);
      req->net_log().AddEvent(NetLog::TYPE_CANCELLED, NULL);
      req->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL, NULL);
      delete req;
      // Let one connect job connect and become idle for potential future use.
      if (group.jobs.size() > group.pending_requests.size() + 1) {
        // TODO(willchan): Cancel the job in the earliest LoadState.
        RemoveConnectJob(*group.jobs.begin(), &group);
        OnAvailableSocketSlot(group_name, &group);
      }
      return;
    }
  }
}

void ClientSocketPoolBaseHelper::ReleaseSocket(const std::string& group_name,
                                               ClientSocket* socket,
                                               int id) {
  Group& group = group_map_[group_name];
  group.num_releasing_sockets++;
  num_releasing_sockets_++;
  DCHECK_LE(group.num_releasing_sockets, group.active_socket_count);
  // Run this asynchronously to allow the caller to finish before we let
  // another to begin doing work.  This also avoids nasty recursion issues.
  // NOTE: We cannot refer to the handle argument after this method returns.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &ClientSocketPoolBaseHelper::DoReleaseSocket, group_name, socket, id));
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

  // Search pending_requests for matching handle.
  RequestQueue::const_iterator it = group.pending_requests.begin();
  for (size_t i = 0; it != group.pending_requests.end(); ++it, ++i) {
    if ((*it)->handle() == handle) {
      if (i < group.jobs.size()) {
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
                                                 ClientSocket* socket,
                                                 int id) {
  // Running callbacks can cause the last outside reference to be released.
  // Hold onto a reference.
  scoped_refptr<ClientSocketPoolBaseHelper> ref_holder(this);

  GroupMap::iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  Group& group = i->second;

  group.num_releasing_sockets--;
  DCHECK_GE(group.num_releasing_sockets, 0);

  CHECK_GT(handed_out_socket_count_, 0);
  handed_out_socket_count_--;

  CHECK_GT(group.active_socket_count, 0);
  group.active_socket_count--;

  CHECK_GT(num_releasing_sockets_, 0);
  num_releasing_sockets_--;

  const bool can_reuse = socket->IsConnectedAndIdle() &&
      id == pool_generation_number_;
  if (can_reuse) {
    AddIdleSocket(socket, true /* used socket */, &group);
  } else {
    delete socket;
  }

  // If there are no more releasing sockets, then we might have to process
  // multiple available socket slots, since we stalled their processing until
  // all sockets have been released.  Note that ProcessPendingRequest() will
  // invoke user callbacks, so |num_releasing_sockets_| may change.
  //
  // This code has been known to infinite loop.  Set a counter and CHECK to make
  // sure it doesn't get ridiculously high.

  int iterations = 0;
  while (num_releasing_sockets_ == 0) {
    CHECK_LT(iterations, 1000) << "Probably stuck in an infinite loop.";
    std::string top_group_name;
    Group* top_group = NULL;
    int stalled_group_count = FindTopStalledGroup(&top_group, &top_group_name);
    if (stalled_group_count >= 1) {
      if (ReachedMaxSocketsLimit()) {
        if (idle_socket_count() > 0) {
          CloseOneIdleSocket();
        } else {
          // We can't activate more sockets since we're already at our global
          // limit.
          may_have_stalled_group_ = true;
          return;
        }
      }

      ProcessPendingRequest(top_group_name, top_group);
    } else {
      may_have_stalled_group_ = false;
      return;
    }

    iterations++;
  }
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
    bool has_unused_slot =
        group.HasAvailableSocketSlot(max_sockets_per_group_) &&
        group.pending_requests.size() > group.jobs.size();
    if (has_unused_slot) {
      stalled_group_count++;
      bool has_higher_priority = !top_group ||
          group.TopPendingPriority() < top_group->TopPendingPriority();
      if (has_higher_priority) {
        top_group = &group;
        top_group_name = &i->first;
      }
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
  // Running callbacks can cause the last outside reference to be released.
  // Hold onto a reference.
  scoped_refptr<ClientSocketPoolBaseHelper> ref_holder(this);

  DCHECK_NE(ERR_IO_PENDING, result);
  const std::string group_name = job->group_name();
  GroupMap::iterator group_it = group_map_.find(group_name);
  CHECK(group_it != group_map_.end());
  Group& group = group_it->second;

  scoped_ptr<ClientSocket> socket(job->ReleaseSocket());

  BoundNetLog job_log = job->net_log();
  RemoveConnectJob(job, &group);

  if (result == OK) {
    DCHECK(socket.get());
    if (!group.pending_requests.empty()) {
      scoped_ptr<const Request> r(RemoveRequestFromQueue(
          group.pending_requests.begin(), &group.pending_requests));
      LogBoundConnectJobToRequest(job_log.source(), r.get());
      HandOutSocket(
          socket.release(), false /* unused socket */, r->handle(),
          base::TimeDelta(), &group, r->net_log());
      r->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL, NULL);
      r->callback()->Run(result);
    } else {
      AddIdleSocket(socket.release(), false /* unused socket */, &group);
      OnAvailableSocketSlot(group_name, &group);
    }
  } else {
    DCHECK(!socket.get());
    if (!group.pending_requests.empty()) {
      scoped_ptr<const Request> r(RemoveRequestFromQueue(
          group.pending_requests.begin(), &group.pending_requests));
      LogBoundConnectJobToRequest(job_log.source(), r.get());
      r->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL,
                            new NetLogIntegerParameter("net_error", result));
      r->callback()->Run(result);
    }
    MaybeOnAvailableSocketSlot(group_name);
  }
}

void ClientSocketPoolBaseHelper::OnIPAddressChanged() {
  Flush();
}

void ClientSocketPoolBaseHelper::Flush() {
  pool_generation_number_++;
  CancelAllConnectJobs();
  CloseIdleSockets();
}

void ClientSocketPoolBaseHelper::RemoveConnectJob(const ConnectJob* job,
                                                  Group* group) {
  CHECK_GT(connecting_socket_count_, 0);
  connecting_socket_count_--;

  DCHECK(group);
  DCHECK(ContainsKey(group->jobs, job));
  group->jobs.erase(job);

  // If we've got no more jobs for this group, then we no longer need a
  // backup job either.
  if (group->jobs.empty())
    group->CleanupBackupJob();

  DCHECK(job);
  delete job;
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
    if (stalled_group_count == 0 ||
        (stalled_group_count == 1 && top_group->num_releasing_sockets == 0)) {
      may_have_stalled_group_ = false;
    }
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
  int rv = RequestSocketInternal(group_name, *group->pending_requests.begin());

  if (rv != ERR_IO_PENDING) {
    scoped_ptr<const Request> r(RemoveRequestFromQueue(
          group->pending_requests.begin(), &group->pending_requests));

    scoped_refptr<NetLog::EventParameters> params;
    if (rv != OK)
      params = new NetLogIntegerParameter("net_error", rv);
    r->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL, params);
    r->callback()->Run(rv);
    if (rv != OK) {
      // |group| may be invalid after the callback, we need to search
      // |group_map_| again.
      MaybeOnAvailableSocketSlot(group_name);
    }
  }
}

void ClientSocketPoolBaseHelper::HandOutSocket(
    ClientSocket* socket,
    bool reused,
    ClientSocketHandle* handle,
    base::TimeDelta idle_time,
    Group* group,
    const BoundNetLog& net_log) {
  DCHECK(socket);
  handle->set_socket(socket);
  handle->set_is_reused(reused);
  handle->set_idle_time(idle_time);
  handle->set_pool_id(pool_generation_number_);

  if (reused) {
    net_log.AddEvent(
        NetLog::TYPE_SOCKET_POOL_REUSED_AN_EXISTING_SOCKET,
        new NetLogIntegerParameter(
            "idle_ms", static_cast<int>(idle_time.InMilliseconds())));
  }

  net_log.AddEvent(NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
                   new NetLogSourceParameter(
                       "source_dependency", socket->NetLog().source()));

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
    connecting_socket_count_ -= group.jobs.size();
    STLDeleteElements(&group.jobs);

    if (group.backup_task) {
      group.backup_task->Cancel();
      group.backup_task = NULL;
    }

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
  int total = handed_out_socket_count_ + connecting_socket_count_ +
      idle_socket_count();
  DCHECK_LE(total, max_sockets_);
  if (total < max_sockets_)
    return false;
  LOG(WARNING) << "ReachedMaxSocketsLimit: " << total << "/" << max_sockets_;
  return true;
}

void ClientSocketPoolBaseHelper::CloseOneIdleSocket() {
  CHECK_GT(idle_socket_count(), 0);

  for (GroupMap::iterator i = group_map_.begin(); i != group_map_.end(); ++i) {
    Group& group = i->second;

    if (!group.idle_sockets.empty()) {
      std::deque<IdleSocket>::iterator j = group.idle_sockets.begin();
      delete j->socket;
      group.idle_sockets.erase(j);
      DecrementIdleCount();
      if (group.IsEmpty())
        group_map_.erase(i);

      return;
    }
  }

  LOG(DFATAL) << "No idle socket found to close!.";
}

}  // namespace internal

}  // namespace net
