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
}

ConnectJob::~ConnectJob() {
  if (delegate_ && !idle_) {
    // If the delegate was not NULLed, then NotifyDelegateOfCompletion has
    // not been called yet.  If we've started then we are cancelling.
    net_log_.AddEvent(NetLog::TYPE_CANCELLED);
    net_log_.EndEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB);
  }
}

int ConnectJob::Connect() {
  if (timeout_duration_ != base::TimeDelta())
    timer_.Start(timeout_duration_, this, &ConnectJob::OnTimeout);

  net_log_.BeginEventWithString(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                                group_name_);
  idle_ = false;

  int rv = ConnectInternal();

  if (rv != ERR_IO_PENDING) {
    delegate_ = NULL;
    net_log_.EndEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB);
  }

  return rv;
}

void ConnectJob::NotifyDelegateOfCompletion(int rv) {
  // The delegate will delete |this|.
  Delegate *delegate = delegate_;
  delegate_ = NULL;

  net_log_.EndEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB);

  delegate->OnConnectJobComplete(rv, this);
}

void ConnectJob::ResetTimer(base::TimeDelta remaining_time) {
  timer_.Stop();
  timer_.Start(remaining_time, this, &ConnectJob::OnTimeout);
}

void ConnectJob::OnTimeout() {
  // Make sure the socket is NULL before calling into |delegate|.
  set_socket(NULL);

  net_log_.AddEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_TIMED_OUT);

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
    ConnectJobFactory* connect_job_factory,
    NetworkChangeNotifier* network_change_notifier)
    : idle_socket_count_(0),
      connecting_socket_count_(0),
      handed_out_socket_count_(0),
      max_sockets_(max_sockets),
      max_sockets_per_group_(max_sockets_per_group),
      unused_idle_socket_timeout_(unused_idle_socket_timeout),
      used_idle_socket_timeout_(used_idle_socket_timeout),
      may_have_stalled_group_(false),
      connect_job_factory_(connect_job_factory),
      network_change_notifier_(network_change_notifier),
      backup_jobs_enabled_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK_LE(0, max_sockets_per_group);
  DCHECK_LE(max_sockets_per_group, max_sockets);

  if (network_change_notifier_)
    network_change_notifier_->AddObserver(this);
}

ClientSocketPoolBaseHelper::~ClientSocketPoolBaseHelper() {
  CancelAllConnectJobs();

  // Clean up any idle sockets.  Assert that we have no remaining active
  // sockets or pending requests.  They should have all been cleaned up prior
  // to the manager being destroyed.
  CloseIdleSockets();
  CHECK(group_map_.empty());
  DCHECK_EQ(0, connecting_socket_count_);

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
  request->net_log().BeginEvent(NetLog::TYPE_SOCKET_POOL);
  Group& group = group_map_[group_name];
  int rv = RequestSocketInternal(group_name, request);
  if (rv != ERR_IO_PENDING)
    request->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL);
  else
    InsertRequestIntoQueue(request, &group.pending_requests);
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

  // Can we make another active socket now?
  if (ReachedMaxSocketsLimit() ||
      !group.HasAvailableSocketSlot(max_sockets_per_group_)) {
    if (ReachedMaxSocketsLimit()) {
      // We could check if we really have a stalled group here, but it requires
      // a scan of all groups, so just flip a flag here, and do the check later.
      may_have_stalled_group_ = true;

      request->net_log().AddEvent(NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS);
    } else {
      request->net_log().AddEvent(NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP);
    }
    return ERR_IO_PENDING;
  }

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

  // See if we already have enough connect jobs or sockets that will be released
  // soon.
  if (group.HasReleasingSockets()) {
    return ERR_IO_PENDING;
  }

  // We couldn't find a socket to reuse, so allocate and connect a new one.
  BoundNetLog job_net_log = BoundNetLog::Make(
      request->net_log().net_log(), NetLog::SOURCE_CONNECT_JOB);
  request->net_log().BeginEventWithInteger(
      NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID, job_net_log.source().id);

  scoped_ptr<ConnectJob> connect_job(
      connect_job_factory_->NewConnectJob(group_name, *request, this,
                                          job_net_log));

  int rv = connect_job->Connect();
  if (rv == OK) {
    request->net_log().EndEventWithInteger(
        NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID, job_net_log.source().id);
    HandOutSocket(connect_job->ReleaseSocket(), false /* not reused */,
                  handle, base::TimeDelta(), &group, request->net_log());
  } else if (rv == ERR_IO_PENDING) {
    // If we don't have any sockets in this group, set a timer for potentially
    // creating a new one.  If the SYN is lost, this backup socket may complete
    // before the slow socket, improving end user latency.
    if (group.IsEmpty() && !group.backup_job && backup_jobs_enabled_) {
      group.backup_job = connect_job_factory_->NewConnectJob(group_name,
                                                             *request,
                                                             this,
                                                             job_net_log);
      StartBackupSocketTimer(group_name);
    }

    connecting_socket_count_++;

    ConnectJob* job = connect_job.release();
    group.jobs.insert(job);
  } else {
    request->net_log().EndEventWithInteger(
        NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID, job_net_log.source().id);
    if (group.IsEmpty())
      group_map_.erase(group_name);
  }

  return rv;
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

  // If our backup job is waiting on DNS, or if we can't create any sockets
  // right now due to limits, just reset the timer.
  CHECK(group.jobs.size());
  if (ReachedMaxSocketsLimit() ||
      !group.HasAvailableSocketSlot(max_sockets_per_group_) ||
      (*group.jobs.begin())->GetLoadState() == LOAD_STATE_RESOLVING_HOST) {
    group.backup_job->net_log().EndEvent(
        NetLog::TYPE_SOCKET_BACKUP_TIMER_EXTENDED);
    StartBackupSocketTimer(group_name);
    return;
  }

  group.backup_job->net_log().AddEvent(NetLog::TYPE_SOCKET_BACKUP_CREATED);
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
  CHECK(ContainsKey(group_map_, group_name));

  Group& group = group_map_[group_name];

  // Search pending_requests for matching handle.
  RequestQueue::iterator it = group.pending_requests.begin();
  for (; it != group.pending_requests.end(); ++it) {
    if ((*it)->handle() == handle) {
      const Request* req = RemoveRequestFromQueue(it, &group.pending_requests);
      req->net_log().AddEvent(NetLog::TYPE_CANCELLED);
      req->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL);
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
                                               ClientSocket* socket) {
  Group& group = group_map_[group_name];
  group.num_releasing_sockets++;
  DCHECK_LE(group.num_releasing_sockets, group.active_socket_count);
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
                                                 ClientSocket* socket) {
  GroupMap::iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  Group& group = i->second;

  group.num_releasing_sockets--;
  DCHECK_GE(group.num_releasing_sockets, 0);

  CHECK_GT(handed_out_socket_count_, 0);
  handed_out_socket_count_--;

  CHECK_GT(group.active_socket_count, 0);
  group.active_socket_count--;

  const bool can_reuse = socket->IsConnectedAndIdle();
  if (can_reuse) {
    AddIdleSocket(socket, true /* used socket */, &group);
  } else {
    delete socket;
  }

  OnAvailableSocketSlot(group_name, &group);

  // If there are no more releasing sockets, then we might have to process
  // multiple available socket slots, since we stalled their processing until
  // all sockets have been released.
  i = group_map_.find(group_name);
  if (i == group_map_.end() || i->second.num_releasing_sockets > 0)
    return;

  while (true) {
    // We can't activate more sockets since we're already at our global limit.
    if (ReachedMaxSocketsLimit())
      return;

    // |group| might now be deleted.
    i = group_map_.find(group_name);
    if (i == group_map_.end())
      return;

    group = i->second;

    // If we already have enough ConnectJobs to satisfy the pending requests,
    // don't bother starting up more.
    if (group.pending_requests.size() <= group.jobs.size())
      return;

    if (!group.HasAvailableSocketSlot(max_sockets_per_group_))
      return;

    OnAvailableSocketSlot(group_name, &group);
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
  DCHECK_NE(ERR_IO_PENDING, result);
  const std::string group_name = job->group_name();
  GroupMap::iterator group_it = group_map_.find(group_name);
  CHECK(group_it != group_map_.end());
  Group& group = group_it->second;

  // We've had a connect on the socket; discard any pending backup job
  // for this group and kill the pending task.
  group.CleanupBackupJob();

  scoped_ptr<ClientSocket> socket(job->ReleaseSocket());

  BoundNetLog job_log = job->net_log();
  RemoveConnectJob(job, &group);

  if (result == OK) {
    DCHECK(socket.get());
    if (!group.pending_requests.empty()) {
      scoped_ptr<const Request> r(RemoveRequestFromQueue(
          group.pending_requests.begin(), &group.pending_requests));
      r->net_log().EndEventWithInteger(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID,
                                       job_log.source().id);
      r->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL);
      HandOutSocket(
          socket.release(), false /* unused socket */, r->handle(),
          base::TimeDelta(), &group, r->net_log());
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
      r->net_log().EndEventWithInteger(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID,
                                       job_log.source().id);
      r->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL);
      r->callback()->Run(result);
    }
    MaybeOnAvailableSocketSlot(group_name);
  }
}

void ClientSocketPoolBaseHelper::OnIPAddressChanged() {
  CloseIdleSockets();
}

void ClientSocketPoolBaseHelper::RemoveConnectJob(const ConnectJob *job,
                                                  Group* group) {
  CHECK_GT(connecting_socket_count_, 0);
  connecting_socket_count_--;

  DCHECK(job);
  delete job;

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
  scoped_ptr<const Request> r(*group->pending_requests.begin());
  int rv = RequestSocketInternal(group_name, r.get());

  if (rv != ERR_IO_PENDING) {
    r->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL);
    RemoveRequestFromQueue(group->pending_requests.begin(),
                           &group->pending_requests);
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
    Group* group,
    const BoundNetLog& net_log) {
  DCHECK(socket);
  handle->set_socket(socket);
  handle->set_is_reused(reused);
  handle->set_idle_time(idle_time);

  if (reused)
    net_log.AddStringLiteral("Reusing socket.");
  if (idle_time != base::TimeDelta()) {
    net_log.AddString(
        StringPrintf("Socket sat idle for %" PRId64 " milliseconds",
                     idle_time.InMilliseconds()));
  }
  net_log.AddEventWithInteger(NetLog::TYPE_SOCKET_POOL_SOCKET_ID,
                              socket->NetLog().source().id);

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
  int total = handed_out_socket_count_ + connecting_socket_count_;
  DCHECK_LE(total, max_sockets_);
  if (total < max_sockets_)
    return false;
  LOG(WARNING) << "ReachedMaxSocketsLimit: " << total << "/" << max_sockets_;
  return true;
}

}  // namespace internal

}  // namespace net
