// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include <math.h>
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "net/base/net_log.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_handle.h"

using base::TimeDelta;

namespace {

// Indicate whether we should enable idle socket cleanup timer. When timer is
// disabled, sockets are closed next time a socket request is made.
bool g_cleanup_timer_enabled = true;

// The timeout value, in seconds, used to clean up idle sockets that can't be
// reused.
//
// Note: It's important to close idle sockets that have received data as soon
// as possible because the received data may cause BSOD on Windows XP under
// some conditions.  See http://crbug.com/4606.
const int kCleanupInterval = 10;  // DO NOT INCREASE THIS TIMEOUT.

// Indicate whether or not we should establish a new transport layer connection
// after a certain timeout has passed without receiving an ACK.
bool g_connect_backup_jobs_enabled = true;

double g_socket_reuse_policy_penalty_exponent = -1;
int g_socket_reuse_policy = -1;

}  // namespace

namespace net {

int GetSocketReusePolicy() {
  return g_socket_reuse_policy;
}

void SetSocketReusePolicy(int policy) {
  DCHECK_GE(policy, 0);
  DCHECK_LE(policy, 2);
  if (policy > 2 || policy < 0) {
    LOG(ERROR) << "Invalid socket reuse policy";
    return;
  }

  double exponents[] = { 0, 0.25, -1 };
  g_socket_reuse_policy_penalty_exponent = exponents[policy];
  g_socket_reuse_policy = policy;

  VLOG(1) << "Setting g_socket_reuse_policy_penalty_exponent = "
          << g_socket_reuse_policy_penalty_exponent;
}

ConnectJob::ConnectJob(const std::string& group_name,
                       base::TimeDelta timeout_duration,
                       Delegate* delegate,
                       const BoundNetLog& net_log)
    : group_name_(group_name),
      timeout_duration_(timeout_duration),
      delegate_(delegate),
      net_log_(net_log),
      idle_(true),
      preconnect_state_(NOT_PRECONNECT) {
  DCHECK(!group_name.empty());
  DCHECK(delegate);
  net_log.BeginEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB, NULL);
}

ConnectJob::~ConnectJob() {
  net_log().EndEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB, NULL);
}

void ConnectJob::Initialize(bool is_preconnect) {
  if (is_preconnect)
    preconnect_state_ = UNUSED_PRECONNECT;
  else
    preconnect_state_ = NOT_PRECONNECT;
}

int ConnectJob::Connect() {
  if (timeout_duration_ != base::TimeDelta())
    timer_.Start(FROM_HERE, timeout_duration_, this, &ConnectJob::OnTimeout);

  idle_ = false;

  LogConnectStart();

  int rv = ConnectInternal();

  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    delegate_ = NULL;
  }

  return rv;
}

void ConnectJob::UseForNormalRequest() {
  DCHECK_EQ(UNUSED_PRECONNECT, preconnect_state_);
  preconnect_state_ = USED_PRECONNECT;
}

void ConnectJob::set_socket(StreamSocket* socket) {
  if (socket) {
    net_log().AddEvent(NetLog::TYPE_CONNECT_JOB_SET_SOCKET, make_scoped_refptr(
          new NetLogSourceParameter("source_dependency",
                                    socket->NetLog().source())));
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
  timer_.Start(FROM_HERE, remaining_time, this, &ConnectJob::OnTimeout);
}

void ConnectJob::LogConnectStart() {
  net_log().BeginEvent(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_CONNECT,
      make_scoped_refptr(new NetLogStringParameter("group_name", group_name_)));
}

void ConnectJob::LogConnectCompletion(int net_error) {
  net_log().EndEventWithNetErrorCode(
      NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_CONNECT, net_error);
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
    OldCompletionCallback* callback,
    RequestPriority priority,
    bool ignore_limits,
    Flags flags,
    const BoundNetLog& net_log)
    : handle_(handle),
      callback_(callback),
      priority_(priority),
      ignore_limits_(ignore_limits),
      flags_(flags),
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
      max_sockets_(max_sockets),
      max_sockets_per_group_(max_sockets_per_group),
      use_cleanup_timer_(g_cleanup_timer_enabled),
      unused_idle_socket_timeout_(unused_idle_socket_timeout),
      used_idle_socket_timeout_(used_idle_socket_timeout),
      connect_job_factory_(connect_job_factory),
      connect_backup_jobs_enabled_(false),
      pool_generation_number_(0),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK_LE(0, max_sockets_per_group);
  DCHECK_LE(max_sockets_per_group, max_sockets);

  NetworkChangeNotifier::AddIPAddressObserver(this);
}

ClientSocketPoolBaseHelper::~ClientSocketPoolBaseHelper() {
  // Clean up any idle sockets and pending connect jobs.  Assert that we have no
  // remaining active sockets or pending requests.  They should have all been
  // cleaned up prior to |this| being destroyed.
  Flush();
  DCHECK(group_map_.empty());
  DCHECK(pending_callback_map_.empty());
  DCHECK_EQ(0, connecting_socket_count_);

  NetworkChangeNotifier::RemoveIPAddressObserver(this);
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
    const RequestQueue::iterator& it, Group* group) {
  const Request* req = *it;
  group->mutable_pending_requests()->erase(it);
  // If there are no more requests, we kill the backup timer.
  if (group->pending_requests().empty())
    group->CleanupBackupJob();
  return req;
}

int ClientSocketPoolBaseHelper::RequestSocket(
    const std::string& group_name,
    const Request* request) {
  CHECK(request->callback());
  CHECK(request->handle());

  // Cleanup any timed-out idle sockets if no timer is used.
  if (!use_cleanup_timer_)
    CleanupIdleSockets(false);

  request->net_log().BeginEvent(NetLog::TYPE_SOCKET_POOL, NULL);
  Group* group = GetOrCreateGroup(group_name);

  int rv = RequestSocketInternal(group_name, request);
  if (rv != ERR_IO_PENDING) {
    request->net_log().EndEventWithNetErrorCode(NetLog::TYPE_SOCKET_POOL, rv);
    CHECK(!request->handle()->is_initialized());
    delete request;
  } else {
    InsertRequestIntoQueue(request, group->mutable_pending_requests());
  }
  return rv;
}

void ClientSocketPoolBaseHelper::RequestSockets(
    const std::string& group_name,
    const Request& request,
    int num_sockets) {
  DCHECK(!request.callback());
  DCHECK(!request.handle());

  // Cleanup any timed out idle sockets if no timer is used.
  if (!use_cleanup_timer_)
    CleanupIdleSockets(false);

  if (num_sockets > max_sockets_per_group_) {
    num_sockets = max_sockets_per_group_;
  }

  request.net_log().BeginEvent(
      NetLog::TYPE_SOCKET_POOL_CONNECTING_N_SOCKETS,
      make_scoped_refptr(new NetLogIntegerParameter(
          "num_sockets", num_sockets)));

  Group* group = GetOrCreateGroup(group_name);

  // RequestSocketsInternal() may delete the group.
  bool deleted_group = false;

  int rv = OK;
  for (int num_iterations_left = num_sockets;
       group->NumActiveSocketSlots() < num_sockets &&
       num_iterations_left > 0 ; num_iterations_left--) {
    rv = RequestSocketInternal(group_name, &request);
    if (rv < 0 && rv != ERR_IO_PENDING) {
      // We're encountering a synchronous error.  Give up.
      if (!ContainsKey(group_map_, group_name))
        deleted_group = true;
      break;
    }
    if (!ContainsKey(group_map_, group_name)) {
      // Unexpected.  The group should only be getting deleted on synchronous
      // error.
      NOTREACHED();
      deleted_group = true;
      break;
    }
  }

  if (!deleted_group && group->IsEmpty())
    RemoveGroup(group_name);

  if (rv == ERR_IO_PENDING)
    rv = OK;
  request.net_log().EndEventWithNetErrorCode(
      NetLog::TYPE_SOCKET_POOL_CONNECTING_N_SOCKETS, rv);
}

int ClientSocketPoolBaseHelper::RequestSocketInternal(
    const std::string& group_name,
    const Request* request) {
  DCHECK_GE(request->priority(), 0);
  ClientSocketHandle* const handle = request->handle();
  const bool preconnecting = !handle;
  Group* group = GetOrCreateGroup(group_name);

  if (!(request->flags() & NO_IDLE_SOCKETS)) {
    // Try to reuse a socket.
    if (AssignIdleSocketToGroup(request, group))
      return OK;
  }

  if (!preconnecting && group->TryToUsePreconnectConnectJob())
    return ERR_IO_PENDING;

  // Can we make another active socket now?
  if (!group->HasAvailableSocketSlot(max_sockets_per_group_) &&
      !request->ignore_limits()) {
    request->net_log().AddEvent(
        NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS_PER_GROUP, NULL);
    return ERR_IO_PENDING;
  }

  if (ReachedMaxSocketsLimit() && !request->ignore_limits()) {
    if (idle_socket_count() > 0) {
      bool closed = CloseOneIdleSocketExceptInGroup(group);
      if (preconnecting && !closed)
        return ERR_PRECONNECT_MAX_SOCKET_LIMIT;
    } else {
      // We could check if we really have a stalled group here, but it requires
      // a scan of all groups, so just flip a flag here, and do the check later.
      request->net_log().AddEvent(
          NetLog::TYPE_SOCKET_POOL_STALLED_MAX_SOCKETS, NULL);
      return ERR_IO_PENDING;
    }
  }

  // We couldn't find a socket to reuse, so allocate and connect a new one.
  scoped_ptr<ConnectJob> connect_job(
      connect_job_factory_->NewConnectJob(group_name, *request, this));

  connect_job->Initialize(preconnecting);
  int rv = connect_job->Connect();
  if (rv == OK) {
    LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
    if (!preconnecting) {
      HandOutSocket(connect_job->ReleaseSocket(), false /* not reused */,
                    handle, base::TimeDelta(), group, request->net_log());
    } else {
      AddIdleSocket(connect_job->ReleaseSocket(), group);
    }
  } else if (rv == ERR_IO_PENDING) {
    // If we don't have any sockets in this group, set a timer for potentially
    // creating a new one.  If the SYN is lost, this backup socket may complete
    // before the slow socket, improving end user latency.
    if (connect_backup_jobs_enabled_ &&
        group->IsEmpty() && !group->HasBackupJob()) {
      group->StartBackupSocketTimer(group_name, this);
    }

    connecting_socket_count_++;

    group->AddJob(connect_job.release());
  } else {
    LogBoundConnectJobToRequest(connect_job->net_log().source(), request);
    StreamSocket* error_socket = NULL;
    if (!preconnecting) {
      DCHECK(handle);
      connect_job->GetAdditionalErrorState(handle);
      error_socket = connect_job->ReleaseSocket();
    }
    if (error_socket) {
      HandOutSocket(error_socket, false /* not reused */, handle,
                    base::TimeDelta(), group, request->net_log());
    } else if (group->IsEmpty()) {
      RemoveGroup(group_name);
    }
  }

  return rv;
}

bool ClientSocketPoolBaseHelper::AssignIdleSocketToGroup(
    const Request* request, Group* group) {
  std::list<IdleSocket>* idle_sockets = group->mutable_idle_sockets();
  std::list<IdleSocket>::iterator idle_socket_it = idle_sockets->end();
  double max_score = -1;

  // Iterate through the idle sockets forwards (oldest to newest)
  //   * Delete any disconnected ones.
  //   * If we find a used idle socket, assign to |idle_socket|.  At the end,
  //   the |idle_socket_it| will be set to the newest used idle socket.
  for (std::list<IdleSocket>::iterator it = idle_sockets->begin();
       it != idle_sockets->end();) {
    if (!it->socket->IsConnectedAndIdle()) {
      DecrementIdleCount();
      delete it->socket;
      it = idle_sockets->erase(it);
      continue;
    }

    if (it->socket->WasEverUsed()) {
      // We found one we can reuse!
      double score = 0;
      int64 bytes_read = it->socket->NumBytesRead();
      double num_kb = static_cast<double>(bytes_read) / 1024.0;
      int idle_time_sec = (base::TimeTicks::Now() - it->start_time).InSeconds();
      idle_time_sec = std::max(1, idle_time_sec);

      if (g_socket_reuse_policy_penalty_exponent >= 0 && num_kb >= 0) {
        score = num_kb / pow(idle_time_sec,
                             g_socket_reuse_policy_penalty_exponent);
      }

      // Equality to prefer recently used connection.
      if (score >= max_score) {
        idle_socket_it = it;
        max_score = score;
      }
    }

    ++it;
  }

  // If we haven't found an idle socket, that means there are no used idle
  // sockets.  Pick the oldest (first) idle socket (FIFO).

  if (idle_socket_it == idle_sockets->end() && !idle_sockets->empty())
    idle_socket_it = idle_sockets->begin();

  if (idle_socket_it != idle_sockets->end()) {
    DecrementIdleCount();
    base::TimeDelta idle_time =
        base::TimeTicks::Now() - idle_socket_it->start_time;
    IdleSocket idle_socket = *idle_socket_it;
    idle_sockets->erase(idle_socket_it);
    HandOutSocket(
        idle_socket.socket,
        idle_socket.socket->WasEverUsed(),
        request->handle(),
        idle_time,
        group,
        request->net_log());
    return true;
  }

  return false;
}

// static
void ClientSocketPoolBaseHelper::LogBoundConnectJobToRequest(
    const NetLog::Source& connect_job_source, const Request* request) {
  request->net_log().AddEvent(
      NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      make_scoped_refptr(new NetLogSourceParameter(
          "source_dependency", connect_job_source)));
}

void ClientSocketPoolBaseHelper::CancelRequest(
    const std::string& group_name, ClientSocketHandle* handle) {
  PendingCallbackMap::iterator callback_it = pending_callback_map_.find(handle);
  if (callback_it != pending_callback_map_.end()) {
    int result = callback_it->second.result;
    pending_callback_map_.erase(callback_it);
    StreamSocket* socket = handle->release_socket();
    if (socket) {
      if (result != OK)
        socket->Disconnect();
      ReleaseSocket(handle->group_name(), socket, handle->id());
    }
    return;
  }

  CHECK(ContainsKey(group_map_, group_name));

  Group* group = GetOrCreateGroup(group_name);

  // Search pending_requests for matching handle.
  RequestQueue::iterator it = group->mutable_pending_requests()->begin();
  for (; it != group->pending_requests().end(); ++it) {
    if ((*it)->handle() == handle) {
      scoped_ptr<const Request> req(RemoveRequestFromQueue(it, group));
      req->net_log().AddEvent(NetLog::TYPE_CANCELLED, NULL);
      req->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL, NULL);

      // We let the job run, unless we're at the socket limit.
      if (group->jobs().size() && ReachedMaxSocketsLimit()) {
        RemoveConnectJob(*group->jobs().begin(), group);
        CheckForStalledSocketGroups();
      }
      break;
    }
  }
}

bool ClientSocketPoolBaseHelper::HasGroup(const std::string& group_name) const {
  return ContainsKey(group_map_, group_name);
}

void ClientSocketPoolBaseHelper::CloseIdleSockets() {
  CleanupIdleSockets(true);
  DCHECK_EQ(0, idle_socket_count_);
}

int ClientSocketPoolBaseHelper::IdleSocketCountInGroup(
    const std::string& group_name) const {
  GroupMap::const_iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  return i->second->idle_sockets().size();
}

LoadState ClientSocketPoolBaseHelper::GetLoadState(
    const std::string& group_name,
    const ClientSocketHandle* handle) const {
  if (ContainsKey(pending_callback_map_, handle))
    return LOAD_STATE_CONNECTING;

  if (!ContainsKey(group_map_, group_name)) {
    NOTREACHED() << "ClientSocketPool does not contain group: " << group_name
                 << " for handle: " << handle;
    return LOAD_STATE_IDLE;
  }

  // Can't use operator[] since it is non-const.
  const Group& group = *group_map_.find(group_name)->second;

  // Search pending_requests for matching handle.
  RequestQueue::const_iterator it = group.pending_requests().begin();
  for (size_t i = 0; it != group.pending_requests().end(); ++it, ++i) {
    if ((*it)->handle() == handle) {
      if (i < group.jobs().size()) {
        LoadState max_state = LOAD_STATE_IDLE;
        for (ConnectJobSet::const_iterator job_it = group.jobs().begin();
             job_it != group.jobs().end(); ++job_it) {
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

DictionaryValue* ClientSocketPoolBaseHelper::GetInfoAsValue(
    const std::string& name, const std::string& type) const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("name", name);
  dict->SetString("type", type);
  dict->SetInteger("handed_out_socket_count", handed_out_socket_count_);
  dict->SetInteger("connecting_socket_count", connecting_socket_count_);
  dict->SetInteger("idle_socket_count", idle_socket_count_);
  dict->SetInteger("max_socket_count", max_sockets_);
  dict->SetInteger("max_sockets_per_group", max_sockets_per_group_);
  dict->SetInteger("pool_generation_number", pool_generation_number_);

  if (group_map_.empty())
    return dict;

  DictionaryValue* all_groups_dict = new DictionaryValue();
  for (GroupMap::const_iterator it = group_map_.begin();
       it != group_map_.end(); it++) {
    const Group* group = it->second;
    DictionaryValue* group_dict = new DictionaryValue();

    group_dict->SetInteger("pending_request_count",
                           group->pending_requests().size());
    if (!group->pending_requests().empty()) {
      group_dict->SetInteger("top_pending_priority",
                             group->TopPendingPriority());
    }

    group_dict->SetInteger("active_socket_count", group->active_socket_count());

    ListValue* idle_socket_list = new ListValue();
    std::list<IdleSocket>::const_iterator idle_socket;
    for (idle_socket = group->idle_sockets().begin();
         idle_socket != group->idle_sockets().end();
         idle_socket++) {
      int source_id = idle_socket->socket->NetLog().source().id;
      idle_socket_list->Append(Value::CreateIntegerValue(source_id));
    }
    group_dict->Set("idle_sockets", idle_socket_list);

    ListValue* connect_jobs_list = new ListValue();
    std::set<ConnectJob*>::const_iterator job = group->jobs().begin();
    for (job = group->jobs().begin(); job != group->jobs().end(); job++) {
      int source_id = (*job)->net_log().source().id;
      connect_jobs_list->Append(Value::CreateIntegerValue(source_id));
    }
    group_dict->Set("connect_jobs", connect_jobs_list);

    group_dict->SetBoolean("is_stalled",
                           group->IsStalled(max_sockets_per_group_));
    group_dict->SetBoolean("has_backup_job", group->HasBackupJob());

    all_groups_dict->SetWithoutPathExpansion(it->first, group_dict);
  }
  dict->Set("groups", all_groups_dict);
  return dict;
}

bool ClientSocketPoolBaseHelper::IdleSocket::ShouldCleanup(
    base::TimeTicks now,
    base::TimeDelta timeout) const {
  bool timed_out = (now - start_time) >= timeout;
  if (timed_out)
    return true;
  if (socket->WasEverUsed())
    return !socket->IsConnectedAndIdle();
  return !socket->IsConnected();
}

void ClientSocketPoolBaseHelper::CleanupIdleSockets(bool force) {
  if (idle_socket_count_ == 0)
    return;

  // Current time value. Retrieving it once at the function start rather than
  // inside the inner loop, since it shouldn't change by any meaningful amount.
  base::TimeTicks now = base::TimeTicks::Now();

  GroupMap::iterator i = group_map_.begin();
  while (i != group_map_.end()) {
    Group* group = i->second;

    std::list<IdleSocket>::iterator j = group->mutable_idle_sockets()->begin();
    while (j != group->idle_sockets().end()) {
      base::TimeDelta timeout =
          j->socket->WasEverUsed() ?
          used_idle_socket_timeout_ : unused_idle_socket_timeout_;
      if (force || j->ShouldCleanup(now, timeout)) {
        delete j->socket;
        j = group->mutable_idle_sockets()->erase(j);
        DecrementIdleCount();
      } else {
        ++j;
      }
    }

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      RemoveGroup(i++);
    } else {
      ++i;
    }
  }
}

ClientSocketPoolBaseHelper::Group* ClientSocketPoolBaseHelper::GetOrCreateGroup(
    const std::string& group_name) {
  GroupMap::iterator it = group_map_.find(group_name);
  if (it != group_map_.end())
    return it->second;
  Group* group = new Group;
  group_map_[group_name] = group;
  return group;
}

void ClientSocketPoolBaseHelper::RemoveGroup(const std::string& group_name) {
  GroupMap::iterator it = group_map_.find(group_name);
  CHECK(it != group_map_.end());

  RemoveGroup(it);
}

void ClientSocketPoolBaseHelper::RemoveGroup(GroupMap::iterator it) {
  delete it->second;
  group_map_.erase(it);
}

// static
bool ClientSocketPoolBaseHelper::connect_backup_jobs_enabled() {
  return g_connect_backup_jobs_enabled;
}

// static
bool ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(bool enabled) {
  bool old_value = g_connect_backup_jobs_enabled;
  g_connect_backup_jobs_enabled = enabled;
  return old_value;
}

void ClientSocketPoolBaseHelper::EnableConnectBackupJobs() {
  connect_backup_jobs_enabled_ = g_connect_backup_jobs_enabled;
}

void ClientSocketPoolBaseHelper::IncrementIdleCount() {
  if (++idle_socket_count_ == 1 && use_cleanup_timer_)
    StartIdleSocketTimer();
}

void ClientSocketPoolBaseHelper::DecrementIdleCount() {
  if (--idle_socket_count_ == 0)
    timer_.Stop();
}

// static
bool ClientSocketPoolBaseHelper::cleanup_timer_enabled() {
  return g_cleanup_timer_enabled;
}

// static
bool ClientSocketPoolBaseHelper::set_cleanup_timer_enabled(bool enabled) {
  bool old_value = g_cleanup_timer_enabled;
  g_cleanup_timer_enabled = enabled;
  return old_value;
}

void ClientSocketPoolBaseHelper::StartIdleSocketTimer() {
  timer_.Start(FROM_HERE, TimeDelta::FromSeconds(kCleanupInterval), this,
               &ClientSocketPoolBaseHelper::OnCleanupTimerFired);
}

void ClientSocketPoolBaseHelper::ReleaseSocket(const std::string& group_name,
                                               StreamSocket* socket,
                                               int id) {
  GroupMap::iterator i = group_map_.find(group_name);
  CHECK(i != group_map_.end());

  Group* group = i->second;

  CHECK_GT(handed_out_socket_count_, 0);
  handed_out_socket_count_--;

  CHECK_GT(group->active_socket_count(), 0);
  group->DecrementActiveSocketCount();

  const bool can_reuse = socket->IsConnectedAndIdle() &&
      id == pool_generation_number_;
  if (can_reuse) {
    // Add it to the idle list.
    AddIdleSocket(socket, group);
    OnAvailableSocketSlot(group_name, group);
  } else {
    delete socket;
  }

  CheckForStalledSocketGroups();
}

void ClientSocketPoolBaseHelper::CheckForStalledSocketGroups() {
  // If we have idle sockets, see if we can give one to the top-stalled group.
  std::string top_group_name;
  Group* top_group = NULL;
  if (!FindTopStalledGroup(&top_group, &top_group_name))
    return;

  if (ReachedMaxSocketsLimit()) {
    if (idle_socket_count() > 0) {
      CloseOneIdleSocket();
    } else {
      // We can't activate more sockets since we're already at our global
      // limit.
      return;
    }
  }

  // Note:  we don't loop on waking stalled groups.  If the stalled group is at
  //        its limit, may be left with other stalled groups that could be
  //        woken.  This isn't optimal, but there is no starvation, so to avoid
  //        the looping we leave it at this.
  OnAvailableSocketSlot(top_group_name, top_group);
}

// Search for the highest priority pending request, amongst the groups that
// are not at the |max_sockets_per_group_| limit. Note: for requests with
// the same priority, the winner is based on group hash ordering (and not
// insertion order).
bool ClientSocketPoolBaseHelper::FindTopStalledGroup(Group** group,
                                                     std::string* group_name) {
  Group* top_group = NULL;
  const std::string* top_group_name = NULL;
  bool has_stalled_group = false;
  for (GroupMap::iterator i = group_map_.begin();
       i != group_map_.end(); ++i) {
    Group* curr_group = i->second;
    const RequestQueue& queue = curr_group->pending_requests();
    if (queue.empty())
      continue;
    if (curr_group->IsStalled(max_sockets_per_group_)) {
      has_stalled_group = true;
      bool has_higher_priority = !top_group ||
          curr_group->TopPendingPriority() < top_group->TopPendingPriority();
      if (has_higher_priority) {
        top_group = curr_group;
        top_group_name = &i->first;
      }
    }
  }

  if (top_group) {
    *group = top_group;
    *group_name = *top_group_name;
  }
  return has_stalled_group;
}

void ClientSocketPoolBaseHelper::OnConnectJobComplete(
    int result, ConnectJob* job) {
  DCHECK_NE(ERR_IO_PENDING, result);
  const std::string group_name = job->group_name();
  GroupMap::iterator group_it = group_map_.find(group_name);
  CHECK(group_it != group_map_.end());
  Group* group = group_it->second;

  scoped_ptr<StreamSocket> socket(job->ReleaseSocket());

  BoundNetLog job_log = job->net_log();

  if (result == OK) {
    DCHECK(socket.get());
    RemoveConnectJob(job, group);
    if (!group->pending_requests().empty()) {
      scoped_ptr<const Request> r(RemoveRequestFromQueue(
          group->mutable_pending_requests()->begin(), group));
      LogBoundConnectJobToRequest(job_log.source(), r.get());
      HandOutSocket(
          socket.release(), false /* unused socket */, r->handle(),
          base::TimeDelta(), group, r->net_log());
      r->net_log().EndEvent(NetLog::TYPE_SOCKET_POOL, NULL);
      InvokeUserCallbackLater(r->handle(), r->callback(), result);
    } else {
      AddIdleSocket(socket.release(), group);
      OnAvailableSocketSlot(group_name, group);
      CheckForStalledSocketGroups();
    }
  } else {
    // If we got a socket, it must contain error information so pass that
    // up so that the caller can retrieve it.
    bool handed_out_socket = false;
    if (!group->pending_requests().empty()) {
      scoped_ptr<const Request> r(RemoveRequestFromQueue(
          group->mutable_pending_requests()->begin(), group));
      LogBoundConnectJobToRequest(job_log.source(), r.get());
      job->GetAdditionalErrorState(r->handle());
      RemoveConnectJob(job, group);
      if (socket.get()) {
        handed_out_socket = true;
        HandOutSocket(socket.release(), false /* unused socket */, r->handle(),
                      base::TimeDelta(), group, r->net_log());
      }
      r->net_log().EndEventWithNetErrorCode(NetLog::TYPE_SOCKET_POOL,
                                            result);
      InvokeUserCallbackLater(r->handle(), r->callback(), result);
    } else {
      RemoveConnectJob(job, group);
    }
    if (!handed_out_socket) {
      OnAvailableSocketSlot(group_name, group);
      CheckForStalledSocketGroups();
    }
  }
}

void ClientSocketPoolBaseHelper::OnIPAddressChanged() {
  Flush();
}

void ClientSocketPoolBaseHelper::Flush() {
  pool_generation_number_++;
  CancelAllConnectJobs();
  CloseIdleSockets();
  AbortAllRequests();
}

void ClientSocketPoolBaseHelper::RemoveConnectJob(ConnectJob* job,
                                                  Group* group) {
  CHECK_GT(connecting_socket_count_, 0);
  connecting_socket_count_--;

  DCHECK(group);
  DCHECK(ContainsKey(group->jobs(), job));
  group->RemoveJob(job);

  // If we've got no more jobs for this group, then we no longer need a
  // backup job either.
  if (group->jobs().empty())
    group->CleanupBackupJob();

  DCHECK(job);
  delete job;
}

void ClientSocketPoolBaseHelper::OnAvailableSocketSlot(
    const std::string& group_name, Group* group) {
  DCHECK(ContainsKey(group_map_, group_name));
  if (group->IsEmpty())
    RemoveGroup(group_name);
  else if (!group->pending_requests().empty())
    ProcessPendingRequest(group_name, group);
}

void ClientSocketPoolBaseHelper::ProcessPendingRequest(
    const std::string& group_name, Group* group) {
  int rv = RequestSocketInternal(group_name,
                                 *group->pending_requests().begin());
  if (rv != ERR_IO_PENDING) {
    scoped_ptr<const Request> request(RemoveRequestFromQueue(
          group->mutable_pending_requests()->begin(), group));
    if (group->IsEmpty())
      RemoveGroup(group_name);

    request->net_log().EndEventWithNetErrorCode(NetLog::TYPE_SOCKET_POOL, rv);
    InvokeUserCallbackLater(request->handle(), request->callback(), rv);
  }
}

void ClientSocketPoolBaseHelper::HandOutSocket(
    StreamSocket* socket,
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
        make_scoped_refptr(new NetLogIntegerParameter(
            "idle_ms", static_cast<int>(idle_time.InMilliseconds()))));
  }

  net_log.AddEvent(NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
                   make_scoped_refptr(new NetLogSourceParameter(
                       "source_dependency", socket->NetLog().source())));

  handed_out_socket_count_++;
  group->IncrementActiveSocketCount();
}

void ClientSocketPoolBaseHelper::AddIdleSocket(
    StreamSocket* socket, Group* group) {
  DCHECK(socket);
  IdleSocket idle_socket;
  idle_socket.socket = socket;
  idle_socket.start_time = base::TimeTicks::Now();

  group->mutable_idle_sockets()->push_back(idle_socket);
  IncrementIdleCount();
}

void ClientSocketPoolBaseHelper::CancelAllConnectJobs() {
  for (GroupMap::iterator i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;
    connecting_socket_count_ -= group->jobs().size();
    group->RemoveAllJobs();

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      // RemoveGroup() will call .erase() which will invalidate the iterator,
      // but i will already have been incremented to a valid iterator before
      // RemoveGroup() is called.
      RemoveGroup(i++);
    } else {
      ++i;
    }
  }
  DCHECK_EQ(0, connecting_socket_count_);
}

void ClientSocketPoolBaseHelper::AbortAllRequests() {
  for (GroupMap::iterator i = group_map_.begin(); i != group_map_.end();) {
    Group* group = i->second;

    RequestQueue pending_requests;
    pending_requests.swap(*group->mutable_pending_requests());
    for (RequestQueue::iterator it2 = pending_requests.begin();
         it2 != pending_requests.end(); ++it2) {
      scoped_ptr<const Request> request(*it2);
      InvokeUserCallbackLater(
          request->handle(), request->callback(), ERR_ABORTED);
    }

    // Delete group if no longer needed.
    if (group->IsEmpty()) {
      // RemoveGroup() will call .erase() which will invalidate the iterator,
      // but i will already have been incremented to a valid iterator before
      // RemoveGroup() is called.
      RemoveGroup(i++);
    } else {
      ++i;
    }
  }
}

bool ClientSocketPoolBaseHelper::ReachedMaxSocketsLimit() const {
  // Each connecting socket will eventually connect and be handed out.
  int total = handed_out_socket_count_ + connecting_socket_count_ +
      idle_socket_count();
  // There can be more sockets than the limit since some requests can ignore
  // the limit
  if (total < max_sockets_)
    return false;
  return true;
}

void ClientSocketPoolBaseHelper::CloseOneIdleSocket() {
  CloseOneIdleSocketExceptInGroup(NULL);
}

bool ClientSocketPoolBaseHelper::CloseOneIdleSocketExceptInGroup(
    const Group* exception_group) {
  CHECK_GT(idle_socket_count(), 0);

  for (GroupMap::iterator i = group_map_.begin(); i != group_map_.end(); ++i) {
    Group* group = i->second;
    if (exception_group == group)
      continue;
    std::list<IdleSocket>* idle_sockets = group->mutable_idle_sockets();

    if (!idle_sockets->empty()) {
      delete idle_sockets->front().socket;
      idle_sockets->pop_front();
      DecrementIdleCount();
      if (group->IsEmpty())
        RemoveGroup(i);

      return true;
    }
  }

  if (!exception_group)
    LOG(DFATAL) << "No idle socket found to close!.";

  return false;
}

void ClientSocketPoolBaseHelper::InvokeUserCallbackLater(
    ClientSocketHandle* handle, OldCompletionCallback* callback, int rv) {
  CHECK(!ContainsKey(pending_callback_map_, handle));
  pending_callback_map_[handle] = CallbackResultPair(callback, rv);
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &ClientSocketPoolBaseHelper::InvokeUserCallback,
          handle));
}

void ClientSocketPoolBaseHelper::InvokeUserCallback(
    ClientSocketHandle* handle) {
  PendingCallbackMap::iterator it = pending_callback_map_.find(handle);

  // Exit if the request has already been cancelled.
  if (it == pending_callback_map_.end())
    return;

  CHECK(!handle->is_initialized());
  OldCompletionCallback* callback = it->second.callback;
  int result = it->second.result;
  pending_callback_map_.erase(it);
  callback->Run(result);
}

ClientSocketPoolBaseHelper::Group::Group()
    : active_socket_count_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

ClientSocketPoolBaseHelper::Group::~Group() {
  CleanupBackupJob();
}

void ClientSocketPoolBaseHelper::Group::StartBackupSocketTimer(
    const std::string& group_name,
    ClientSocketPoolBaseHelper* pool) {
  // Only allow one timer pending to create a backup socket.
  if (!method_factory_.empty())
    return;

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &Group::OnBackupSocketTimerFired, group_name, pool),
      pool->ConnectRetryIntervalMs());
}

bool ClientSocketPoolBaseHelper::Group::TryToUsePreconnectConnectJob() {
  for (std::set<ConnectJob*>::iterator it = jobs_.begin();
       it != jobs_.end(); ++it) {
    ConnectJob* job = *it;
    if (job->is_unused_preconnect()) {
      job->UseForNormalRequest();
      return true;
    }
  }
  return false;
}

void ClientSocketPoolBaseHelper::Group::OnBackupSocketTimerFired(
    std::string group_name,
    ClientSocketPoolBaseHelper* pool) {
  // If there are no more jobs pending, there is no work to do.
  // If we've done our cleanups correctly, this should not happen.
  if (jobs_.empty()) {
    NOTREACHED();
    return;
  }

  // If our backup job is waiting on DNS, or if we can't create any sockets
  // right now due to limits, just reset the timer.
  if (pool->ReachedMaxSocketsLimit() ||
      !HasAvailableSocketSlot(pool->max_sockets_per_group_) ||
      (*jobs_.begin())->GetLoadState() == LOAD_STATE_RESOLVING_HOST) {
    StartBackupSocketTimer(group_name, pool);
    return;
  }

  if (pending_requests_.empty())
    return;

  ConnectJob* backup_job = pool->connect_job_factory_->NewConnectJob(
      group_name, **pending_requests_.begin(), pool);
  backup_job->net_log().AddEvent(NetLog::TYPE_SOCKET_BACKUP_CREATED, NULL);
  SIMPLE_STATS_COUNTER("socket.backup_created");
  int rv = backup_job->Connect();
  pool->connecting_socket_count_++;
  AddJob(backup_job);
  if (rv != ERR_IO_PENDING)
    pool->OnConnectJobComplete(rv, backup_job);
}

void ClientSocketPoolBaseHelper::Group::RemoveAllJobs() {
  // Delete active jobs.
  STLDeleteElements(&jobs_);

  // Cancel pending backup job.
  method_factory_.RevokeAll();
}

}  // namespace internal

}  // namespace net
