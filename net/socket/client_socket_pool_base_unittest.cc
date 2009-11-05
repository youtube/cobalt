// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/scoped_vector.h"
#include "net/base/load_log.h"
#include "net/base/load_log_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kDefaultMaxSockets = 4;
const int kDefaultMaxSocketsPerGroup = 2;
const int kDefaultPriority = 5;

typedef ClientSocketPoolBase<const void*> TestClientSocketPoolBase;

class MockClientSocket : public ClientSocket {
 public:
  MockClientSocket() : connected_(false) {}

  // Socket methods:
  virtual int Read(
      IOBuffer* /* buf */, int /* len */, CompletionCallback* /* callback */) {
    return ERR_UNEXPECTED;
  }

  virtual int Write(
      IOBuffer* /* buf */, int /* len */, CompletionCallback* /* callback */) {
    return ERR_UNEXPECTED;
  }
  virtual bool SetReceiveBufferSize(int32 size) { return true; };
  virtual bool SetSendBufferSize(int32 size) { return true; };

  // ClientSocket methods:

  virtual int Connect(CompletionCallback* callback, LoadLog* load_log) {
    connected_ = true;
    return OK;
  }

  virtual void Disconnect() { connected_ = false; }
  virtual bool IsConnected() const { return connected_; }
  virtual bool IsConnectedAndIdle() const { return connected_; }

#if defined(OS_LINUX)
  virtual int GetPeerName(struct sockaddr* /* name */,
                          socklen_t* /* namelen */) {
    return 0;
  }
#endif

 private:
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(MockClientSocket);
};

class TestConnectJob;

class MockClientSocketFactory : public ClientSocketFactory {
 public:
  MockClientSocketFactory() : allocation_count_(0) {}

  virtual ClientSocket* CreateTCPClientSocket(const AddressList& addresses) {
    allocation_count_++;
    return NULL;
  }

  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocket* transport_socket,
      const std::string& hostname,
      const SSLConfig& ssl_config) {
    NOTIMPLEMENTED();
    return NULL;
  }

  void WaitForSignal(TestConnectJob* job) { waiting_jobs_.push_back(job); }
  void SignalJobs();

  int allocation_count() const { return allocation_count_; }

 private:
  int allocation_count_;
  std::vector<TestConnectJob*> waiting_jobs_;
};

class TestConnectJob : public ConnectJob {
 public:
  enum JobType {
    kMockJob,
    kMockFailingJob,
    kMockPendingJob,
    kMockPendingFailingJob,
    kMockWaitingJob,
    kMockAdvancingLoadStateJob,
  };

  TestConnectJob(JobType job_type,
                 const std::string& group_name,
                 const TestClientSocketPoolBase::Request& request,
                 base::TimeDelta timeout_duration,
                 ConnectJob::Delegate* delegate,
                 MockClientSocketFactory* client_socket_factory,
                 LoadLog* load_log)
      : ConnectJob(group_name, request.handle(), timeout_duration,
                   delegate, load_log),
        job_type_(job_type),
        client_socket_factory_(client_socket_factory),
        method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        load_state_(LOAD_STATE_IDLE) {}

  void Signal() {
    DoConnect(waiting_success_, true /* async */);
  }

  virtual LoadState GetLoadState() const { return load_state_; }

 private:
  // ConnectJob methods:

  virtual int ConnectInternal() {
    AddressList ignored;
    client_socket_factory_->CreateTCPClientSocket(ignored);
    set_socket(new MockClientSocket());
    switch (job_type_) {
      case kMockJob:
        return DoConnect(true /* successful */, false /* sync */);
      case kMockFailingJob:
        return DoConnect(false /* error */, false /* sync */);
      case kMockPendingJob:
        set_load_state(LOAD_STATE_CONNECTING);

        // Depending on execution timings, posting a delayed task can result
        // in the task getting executed the at the earliest possible
        // opportunity or only after returning once from the message loop and
        // then a second call into the message loop. In order to make behavior
        // more deterministic, we change the default delay to 2ms. This should
        // always require us to wait for the second call into the message loop.
        //
        // N.B. The correct fix for this and similar timing problems is to
        // abstract time for the purpose of unittests. Unfortunately, we have
        // a lot of third-party components that directly call the various
        // time functions, so this change would be rather invasive.
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &TestConnectJob::DoConnect,
                true /* successful */,
                true /* async */),
            2);
        return ERR_IO_PENDING;
      case kMockPendingFailingJob:
        set_load_state(LOAD_STATE_CONNECTING);
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &TestConnectJob::DoConnect,
                false /* error */,
                true  /* async */),
            2);
        return ERR_IO_PENDING;
      case kMockWaitingJob:
        client_socket_factory_->WaitForSignal(this);
        waiting_success_ = true;
        return ERR_IO_PENDING;
      case kMockAdvancingLoadStateJob:
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &TestConnectJob::AdvanceLoadState, load_state_),
            2);
        return ERR_IO_PENDING;
      default:
        NOTREACHED();
        set_socket(NULL);
        return ERR_FAILED;
    }
  }

  void set_load_state(LoadState load_state) { load_state_ = load_state; }

  int DoConnect(bool succeed, bool was_async) {
    int result = ERR_CONNECTION_FAILED;
    if (succeed) {
      result = OK;
      socket()->Connect(NULL, NULL);
    } else {
      set_socket(NULL);
    }

    if (was_async)
      NotifyDelegateOfCompletion(result);
    return result;
  }

  void AdvanceLoadState(LoadState state) {
    int tmp = state;
    tmp++;
    state = static_cast<LoadState>(tmp);
    set_load_state(state);
    // Post a delayed task so RunAllPending() won't run it.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(&TestConnectJob::AdvanceLoadState,
                                          state),
        1 /* 1ms delay */);
  }

  bool waiting_success_;
  const JobType job_type_;
  MockClientSocketFactory* const client_socket_factory_;
  ScopedRunnableMethodFactory<TestConnectJob> method_factory_;
  LoadState load_state_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectJob);
};

class TestConnectJobFactory
    : public TestClientSocketPoolBase::ConnectJobFactory {
 public:
  explicit TestConnectJobFactory(MockClientSocketFactory* client_socket_factory)
      : job_type_(TestConnectJob::kMockJob),
        client_socket_factory_(client_socket_factory) {}

  virtual ~TestConnectJobFactory() {}

  void set_job_type(TestConnectJob::JobType job_type) { job_type_ = job_type; }

  void set_timeout_duration(base::TimeDelta timeout_duration) {
    timeout_duration_ = timeout_duration;
  }

  // ConnectJobFactory methods:

  virtual ConnectJob* NewConnectJob(
      const std::string& group_name,
      const TestClientSocketPoolBase::Request& request,
      ConnectJob::Delegate* delegate,
      LoadLog* load_log) const {
    return new TestConnectJob(job_type_,
                              group_name,
                              request,
                              timeout_duration_,
                              delegate,
                              client_socket_factory_,
                              load_log);
  }

 private:
  TestConnectJob::JobType job_type_;
  base::TimeDelta timeout_duration_;
  MockClientSocketFactory* const client_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectJobFactory);
};

class TestClientSocketPool : public ClientSocketPool {
 public:
  TestClientSocketPool(
      int max_sockets,
      int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      TestClientSocketPoolBase::ConnectJobFactory* connect_job_factory)
      : base_(max_sockets, max_sockets_per_group,
              unused_idle_socket_timeout, used_idle_socket_timeout,
              connect_job_factory) {}

  virtual int RequestSocket(
      const std::string& group_name,
      const void* params,
      int priority,
      ClientSocketHandle* handle,
      CompletionCallback* callback,
      LoadLog* load_log) {
    return base_.RequestSocket(
        group_name, params, priority, handle, callback, load_log);
  }

  virtual void CancelRequest(
      const std::string& group_name,
      const ClientSocketHandle* handle) {
    base_.CancelRequest(group_name, handle);
  }

  virtual void ReleaseSocket(
      const std::string& group_name,
      ClientSocket* socket) {
    base_.ReleaseSocket(group_name, socket);
  }

  virtual void CloseIdleSockets() {
    base_.CloseIdleSockets();
  }

  virtual int IdleSocketCount() const { return base_.idle_socket_count(); }

  virtual int IdleSocketCountInGroup(const std::string& group_name) const {
    return base_.IdleSocketCountInGroup(group_name);
  }

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const {
    return base_.GetLoadState(group_name, handle);
  }

  const TestClientSocketPoolBase* base() const { return &base_; }

  int NumConnectJobsInGroup(const std::string& group_name) const {
    return base_.NumConnectJobsInGroup(group_name);
  }

  void CleanupTimedOutIdleSockets() { base_.CleanupIdleSockets(false); }

 private:
  ~TestClientSocketPool() {}

  TestClientSocketPoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(TestClientSocketPool);
};

}  // namespace

REGISTER_SOCKET_PARAMS_FOR_POOL(TestClientSocketPool, const void*);

namespace {

void MockClientSocketFactory::SignalJobs() {
  for (std::vector<TestConnectJob*>::iterator it = waiting_jobs_.begin();
       it != waiting_jobs_.end(); ++it) {
    (*it)->Signal();
  }
  waiting_jobs_.clear();
}

class TestConnectJobDelegate : public ConnectJob::Delegate {
 public:
  TestConnectJobDelegate()
      : have_result_(false), waiting_for_result_(false), result_(OK) {}
  virtual ~TestConnectJobDelegate() {}

  virtual void OnConnectJobComplete(int result, ConnectJob* job) {
    result_ = result;
    scoped_ptr<ClientSocket> socket(job->ReleaseSocket());
    // socket.get() should be NULL iff result != OK
    EXPECT_EQ(socket.get() == NULL, result != OK);
    delete job;
    have_result_ = true;
    if (waiting_for_result_)
      MessageLoop::current()->Quit();
  }

  int WaitForResult() {
    DCHECK(!waiting_for_result_);
    while (!have_result_) {
      waiting_for_result_ = true;
      MessageLoop::current()->Run();
      waiting_for_result_ = false;
    }
    have_result_ = false;  // auto-reset for next callback
    return result_;
  }

 private:
  bool have_result_;
  bool waiting_for_result_;
  int result_;
};

class ClientSocketPoolBaseTest : public ClientSocketPoolTest {
 protected:
  ClientSocketPoolBaseTest() {}

  void CreatePool(int max_sockets, int max_sockets_per_group) {
    CreatePoolWithIdleTimeouts(
        max_sockets,
        max_sockets_per_group,
        base::TimeDelta::FromSeconds(kUnusedIdleSocketTimeout),
        base::TimeDelta::FromSeconds(kUsedIdleSocketTimeout));
  }

  void CreatePoolWithIdleTimeouts(
      int max_sockets, int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout) {
    DCHECK(!pool_.get());
    connect_job_factory_ = new TestConnectJobFactory(&client_socket_factory_);
    pool_ = new TestClientSocketPool(max_sockets,
                                     max_sockets_per_group,
                                     unused_idle_socket_timeout,
                                     used_idle_socket_timeout,
                                     connect_job_factory_);
  }

  int StartRequest(const std::string& group_name, int priority) {
    return StartRequestUsingPool<TestClientSocketPool, const void*>(
        pool_.get(), group_name, priority, NULL);
  }

  virtual void TearDown() {
    // We post all of our delayed tasks with a 2ms delay. I.e. they don't
    // actually become pending until 2ms after they have been created. In order
    // to flush all tasks, we need to wait so that we know there are no
    // soon-to-be-pending tasks waiting.
    PlatformThread::Sleep(10);
    MessageLoop::current()->RunAllPending();

    // Need to delete |pool_| before we turn late binding back off. We also need
    // to delete |requests_| because the pool is reference counted and requests
    // keep reference to it.
    // TODO(willchan): Remove this part when late binding becomes the default.
    pool_ = NULL;
    requests_.reset();

    EnableLateBindingOfSockets(false);

    ClientSocketPoolTest::TearDown();
  }

  MockClientSocketFactory client_socket_factory_;
  TestConnectJobFactory* connect_job_factory_;
  scoped_refptr<TestClientSocketPool> pool_;
};

// Helper function which explicitly specifies the template parameters, since
// the compiler will infer (in this case, incorrectly) that NULL is of type int.
int InitHandle(ClientSocketHandle* handle,
               const std::string& group_name,
               int priority,
               CompletionCallback* callback,
               TestClientSocketPool* pool,
               LoadLog* load_log) {
  return handle->Init<const void*, TestClientSocketPool>(
      group_name, NULL, priority, callback, pool, load_log);
}

// Even though a timeout is specified, it doesn't time out on a synchronous
// completion.
TEST_F(ClientSocketPoolBaseTest, ConnectJob_NoTimeoutOnSynchronousCompletion) {
  TestConnectJobDelegate delegate;
  ClientSocketHandle ignored;
  TestClientSocketPoolBase::Request request(
      &ignored, NULL, kDefaultPriority, NULL, NULL);
  scoped_ptr<TestConnectJob> job(
      new TestConnectJob(TestConnectJob::kMockJob,
                         "a",
                         request,
                         base::TimeDelta::FromMicroseconds(1),
                         &delegate,
                         &client_socket_factory_,
                         NULL));
  EXPECT_EQ(OK, job->Connect());
}

TEST_F(ClientSocketPoolBaseTest, ConnectJob_TimedOut) {
  TestConnectJobDelegate delegate;
  ClientSocketHandle ignored;
  scoped_refptr<LoadLog> log(new LoadLog);
  TestClientSocketPoolBase::Request request(
      &ignored, NULL, kDefaultPriority, NULL, NULL);
  // Deleted by TestConnectJobDelegate.
  TestConnectJob* job =
      new TestConnectJob(TestConnectJob::kMockPendingJob,
                         "a",
                         request,
                         base::TimeDelta::FromMicroseconds(1),
                         &delegate,
                         &client_socket_factory_,
                         log);
  ASSERT_EQ(ERR_IO_PENDING, job->Connect());
  PlatformThread::Sleep(1);
  EXPECT_EQ(ERR_TIMED_OUT, delegate.WaitForResult());

  EXPECT_EQ(3u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB_TIMED_OUT,
                    LoadLog::PHASE_NONE);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest, BasicSynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  scoped_refptr<LoadLog> log(new LoadLog);
  EXPECT_EQ(OK, InitHandle(&handle, "a", kDefaultPriority,
                           &callback, pool_.get(), log));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  handle.Reset();

  EXPECT_EQ(4u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest, BasicAsynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  scoped_refptr<LoadLog> log(new LoadLog);
  TestSocketRequest req(&request_order_, &completion_count_);
  int rv = InitHandle(req.handle(), "a", 0, &req, pool_.get(), log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", req.handle()));
  EXPECT_EQ(OK, req.WaitForResult());
  EXPECT_TRUE(req.handle()->is_initialized());
  EXPECT_TRUE(req.handle()->socket());
  req.handle()->Reset();

  EXPECT_EQ(4u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest, InitConnectionFailure) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockFailingJob);
  scoped_refptr<LoadLog> log(new LoadLog);
  TestSocketRequest req(&request_order_, &completion_count_);
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            InitHandle(req.handle(), "a", kDefaultPriority, &req,
                       pool_.get(), log));

  EXPECT_EQ(4u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest, InitConnectionAsynchronousFailure) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);
  scoped_refptr<LoadLog> log(new LoadLog);
  TestSocketRequest req(&request_order_, &completion_count_);
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(req.handle(), "a", kDefaultPriority, &req,
                       pool_.get(), log));
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", req.handle()));
  EXPECT_EQ(ERR_CONNECTION_FAILED, req.WaitForResult());

  EXPECT_EQ(4u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest, TotalLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  // TODO(eroman): Check that the LoadLog contains this event.

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("c", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("d", kDefaultPriority));

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSockets, completion_count_);

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("e", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("f", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("g", kDefaultPriority));

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSockets, completion_count_);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));
  EXPECT_EQ(7, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(8));
}

TEST_F(ClientSocketPoolBaseTest, TotalLimitReachedNewGroup) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  // TODO(eroman): Check that the LoadLog contains this event.

  // Reach all limits: max total sockets, and max sockets per group.
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSockets, completion_count_);

  // Now create a new group and verify that we don't starve it.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kDefaultPriority));

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSockets, completion_count_);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(6));
}

TEST_F(ClientSocketPoolBaseTest, TotalLimitRespectsPriority) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("b", 3));
  EXPECT_EQ(OK, StartRequest("a", 3));
  EXPECT_EQ(OK, StartRequest("b", 6));
  EXPECT_EQ(OK, StartRequest("a", 6));

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 5));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("b", 7));

  ReleaseAllConnections(KEEP_ALIVE);

  // We're re-using one socket for group "a", and one for "b".
  EXPECT_EQ(static_cast<int>(requests_.size()) - 2,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSockets, completion_count_);

  // First 4 requests don't have to wait, and finish in order.
  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));

  // Request ("b", 7) has the highest priority, then ("a", 5),
  // and then ("c", 4).
  EXPECT_EQ(7, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));
  EXPECT_EQ(5, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(8));
}

TEST_F(ClientSocketPoolBaseTest, TotalLimitRespectsGroupLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", 3));
  EXPECT_EQ(OK, StartRequest("a", 6));
  EXPECT_EQ(OK, StartRequest("b", 3));
  EXPECT_EQ(OK, StartRequest("b", 6));

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", 6));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("b", 7));

  ReleaseAllConnections(KEEP_ALIVE);

  // We're re-using one socket for group "a", and one for "b".
  EXPECT_EQ(static_cast<int>(requests_.size()) - 2,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSockets, completion_count_);

  // First 4 requests don't have to wait, and finish in order.
  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));

  // Request ("b", 7) has the highest priority, but we can't make new socket for
  // group "b", because it has reached the per-group limit. Then we make
  // socket for ("c", 6), because it has higher priority than ("a", 4),
  // and we still can't make a socket for group "b".
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));
  EXPECT_EQ(7, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(8));
}

// Make sure that we count connecting sockets against the total limit.
TEST_F(ClientSocketPoolBaseTest, TotalLimitCountsConnectingSockets) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("c", kDefaultPriority));

  // Create one asynchronous request.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("d", kDefaultPriority));

  // We post all of our delayed tasks with a 2ms delay. I.e. they don't
  // actually become pending until 2ms after they have been created. In order
  // to flush all tasks, we need to wait so that we know there are no
  // soon-to-be-pending tasks waiting.
  PlatformThread::Sleep(10);
  MessageLoop::current()->RunAllPending();

  // The next synchronous request should wait for its turn.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("e", kDefaultPriority));

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(6));
}

// Inside ClientSocketPoolBase we have a may_have_stalled_group flag,
// which tells it to use more expensive, but accurate, group selection
// algorithm. Make sure it doesn't get stuck in the "on" state.
TEST_F(ClientSocketPoolBaseTest, MayHaveStalledGroupReset) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_FALSE(pool_->base()->may_have_stalled_group());

  // Reach group socket limit.
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_FALSE(pool_->base()->may_have_stalled_group());

  // Reach total limit, but don't request more sockets.
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));
  EXPECT_FALSE(pool_->base()->may_have_stalled_group());

  // Request one more socket while we are at the maximum sockets limit.
  // This should flip the may_have_stalled_group flag.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kDefaultPriority));
  EXPECT_TRUE(pool_->base()->may_have_stalled_group());

  // After releasing first connection for "a", we're still at the
  // maximum sockets limit, but every group's pending queue is empty,
  // so we reset the flag.
  EXPECT_TRUE(ReleaseOneConnection(KEEP_ALIVE));
  EXPECT_FALSE(pool_->base()->may_have_stalled_group());

  // Requesting additional socket while at the total limit should
  // flip the flag back to "on".
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kDefaultPriority));
  EXPECT_TRUE(pool_->base()->may_have_stalled_group());

  // We'll request one more socket to verify that we don't reset the flag
  // too eagerly.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("d", kDefaultPriority));
  EXPECT_TRUE(pool_->base()->may_have_stalled_group());

  // We're at the maximum socket limit, and still have one request pending
  // for "d". Flag should be "on".
  EXPECT_TRUE(ReleaseOneConnection(KEEP_ALIVE));
  EXPECT_TRUE(pool_->base()->may_have_stalled_group());

  // Now every group's pending queue should be empty again.
  EXPECT_TRUE(ReleaseOneConnection(KEEP_ALIVE));
  EXPECT_FALSE(pool_->base()->may_have_stalled_group());

  ReleaseAllConnections(KEEP_ALIVE);
  EXPECT_FALSE(pool_->base()->may_have_stalled_group());
}

TEST_F(ClientSocketPoolBaseTest, PendingRequests) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(kDefaultMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup, completion_count_);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(6, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(3, GetOrderOfRequest(5));
  EXPECT_EQ(5, GetOrderOfRequest(6));
  EXPECT_EQ(7, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(8));
}

TEST_F(ClientSocketPoolBaseTest, PendingRequests_NoKeepAlive) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));

  ReleaseAllConnections(NO_KEEP_ALIVE);

  for (size_t i = kDefaultMaxSocketsPerGroup; i < requests_.size(); ++i)
    EXPECT_EQ(OK, requests_[i]->WaitForResult());

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup, completion_count_);
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending connect job will be cancelled and should not call back into
// ClientSocketPoolBase.
TEST_F(ClientSocketPoolBaseTest, CancelRequestClearGroup) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(&request_order_, &completion_count_);
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(req.handle(), "a", kDefaultPriority, &req,
                       pool_.get(), NULL));
  req.handle()->Reset();
}

TEST_F(ClientSocketPoolBaseTest, TwoRequestsCancelOne) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(&request_order_, &completion_count_);
  TestSocketRequest req2(&request_order_, &completion_count_);

  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(req.handle(), "a", kDefaultPriority, &req,
                       pool_.get(), NULL));
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(req2.handle(), "a", kDefaultPriority, &req2,
                       pool_.get(), NULL));

  req.handle()->Reset();

  EXPECT_EQ(OK, req2.WaitForResult());
  req2.handle()->Reset();
}

TEST_F(ClientSocketPoolBaseTest, ConnectCancelConnect) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  TestSocketRequest req(&request_order_, &completion_count_);

  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(&handle, "a", kDefaultPriority, &callback,
                       pool_.get(), NULL));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(&handle, "a", kDefaultPriority, &callback2,
                       pool_.get(), NULL));

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, CancelRequest) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));

  // Cancel a request.
  size_t index_to_cancel = kDefaultMaxSocketsPerGroup + 2;
  EXPECT_FALSE(requests_[index_to_cancel]->handle()->is_initialized());
  requests_[index_to_cancel]->handle()->Reset();

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(kDefaultMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup - 1,
            completion_count_);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(5, GetOrderOfRequest(3));
  EXPECT_EQ(3, GetOrderOfRequest(4));
  EXPECT_EQ(kRequestNotFound, GetOrderOfRequest(5));  // Canceled request.
  EXPECT_EQ(4, GetOrderOfRequest(6));
  EXPECT_EQ(6, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(8));
}

class RequestSocketCallback : public CallbackRunner< Tuple1<int> > {
 public:
  RequestSocketCallback(ClientSocketHandle* handle,
                        TestClientSocketPool* pool,
                        TestConnectJobFactory* test_connect_job_factory,
                        TestConnectJob::JobType next_job_type)
      : handle_(handle),
        pool_(pool),
        within_callback_(false),
        test_connect_job_factory_(test_connect_job_factory),
        next_job_type_(next_job_type) {}

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    ASSERT_EQ(OK, params.a);

    if (!within_callback_) {
      test_connect_job_factory_->set_job_type(next_job_type_);
      handle_->Reset();
      within_callback_ = true;
      TestCompletionCallback next_job_callback;
      int rv = InitHandle(
          handle_, "a", kDefaultPriority, &next_job_callback, pool_.get(),
          NULL);
      switch (next_job_type_) {
        case TestConnectJob::kMockJob:
          EXPECT_EQ(OK, rv);
          break;
        case TestConnectJob::kMockPendingJob:
          EXPECT_EQ(ERR_IO_PENDING, rv);

          // For pending jobs, wait for new socket to be created. This makes
          // sure there are no more pending operations nor any unclosed sockets
          // when the test finishes.
          // We need to give it a little bit of time to run, so that all the
          // operations that happen on timers (e.g. cleanup of idle
          // connections) can execute.
          MessageLoop::current()->SetNestableTasksAllowed(true);
          PlatformThread::Sleep(10);
          EXPECT_EQ(OK, next_job_callback.WaitForResult());
          break;
        default:
          FAIL() << "Unexpected job type: " << next_job_type_;
          break;
      }
    }
  }

  int WaitForResult() {
    return callback_.WaitForResult();
  }

 private:
  ClientSocketHandle* const handle_;
  const scoped_refptr<TestClientSocketPool> pool_;
  bool within_callback_;
  TestConnectJobFactory* const test_connect_job_factory_;
  TestConnectJob::JobType next_job_type_;
  TestCompletionCallback callback_;
};

TEST_F(ClientSocketPoolBaseTest, RequestPendingJobTwice) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  RequestSocketCallback callback(
      &handle, pool_.get(), connect_job_factory_,
      TestConnectJob::kMockPendingJob);
  int rv = InitHandle(&handle, "a", kDefaultPriority, &callback,
                      pool_.get(), NULL);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, RequestPendingJobThenSynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  RequestSocketCallback callback(
      &handle, pool_.get(), connect_job_factory_, TestConnectJob::kMockJob);
  int rv = InitHandle(&handle, "a", kDefaultPriority, &callback,
                      pool_.get(), NULL);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(ClientSocketPoolBaseTest, CancelActiveRequestWithPendingRequests) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  // Now, kDefaultMaxSocketsPerGroup requests should be active.
  // Let's cancel them.
  for (int i = 0; i < kDefaultMaxSocketsPerGroup; ++i) {
    ASSERT_FALSE(requests_[i]->handle()->is_initialized());
    requests_[i]->handle()->Reset();
  }

  // Let's wait for the rest to complete now.
  for (size_t i = kDefaultMaxSocketsPerGroup; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult());
    requests_[i]->handle()->Reset();
  }

  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup, completion_count_);
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(ClientSocketPoolBaseTest, FailingActiveRequestWithPendingRequests) {
  const size_t kMaxSockets = 5;
  CreatePool(kMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  const size_t kNumberOfRequests = 2 * kDefaultMaxSocketsPerGroup + 1;
  ASSERT_LE(kNumberOfRequests, kMaxSockets);  // Otherwise the test will hang.

  // Queue up all the requests
  for (size_t i = 0; i < kNumberOfRequests; ++i)
    EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  for (size_t i = 0; i < kNumberOfRequests; ++i)
    EXPECT_EQ(ERR_CONNECTION_FAILED, requests_[i]->WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest, CancelActiveRequestThenRequestSocket) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  TestSocketRequest req(&request_order_, &completion_count_);
  int rv = InitHandle(req.handle(), "a", kDefaultPriority, &req,
                      pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel the active request.
  req.handle()->Reset();

  rv = InitHandle(req.handle(), "a", kDefaultPriority, &req,
                  pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req.WaitForResult());

  EXPECT_FALSE(req.handle()->is_reused());
  EXPECT_EQ(1U, completion_count_);
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// A pending asynchronous job completes, which will free up a socket slot.  The
// next job finishes synchronously.  The callback for the asynchronous job
// should be first though.
TEST_F(ClientSocketPoolBaseTest, PendingJobCompletionOrder) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  // First two jobs are async.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  // Start job 1 (async error).
  TestSocketRequest req1(&request_order_, &completion_count_);
  int rv = InitHandle(req1.handle(), "a", kDefaultPriority, &req1,
                      pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Start job 2 (async error).
  TestSocketRequest req2(&request_order_, &completion_count_);
  rv = InitHandle(req2.handle(), "a", kDefaultPriority, &req2,
                  pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // The pending job is sync.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  // Request 3 does not have a ConnectJob yet.  It's just pending.
  TestSocketRequest req3(&request_order_, &completion_count_);
  rv = InitHandle(
      req3.handle(), "a", kDefaultPriority, &req3, pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(ERR_CONNECTION_FAILED, req1.WaitForResult());
  EXPECT_EQ(ERR_CONNECTION_FAILED, req2.WaitForResult());
  EXPECT_EQ(OK, req3.WaitForResult());

  ASSERT_EQ(3U, request_order_.size());

  // After job 1 finishes unsuccessfully, it will try to process the pending
  // requests queue, so it starts up job 3 for request 3.  This job
  // synchronously succeeds, so the request order is 1, 3, 2.
  EXPECT_EQ(&req1, request_order_[0]);
  EXPECT_EQ(&req2, request_order_[2]);
  EXPECT_EQ(&req3, request_order_[1]);
}

// When a ConnectJob is coupled to a request, even if a free socket becomes
// available, the request will be serviced by the ConnectJob.
TEST_F(ClientSocketPoolBaseTest, ReleaseSockets) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  EnableLateBindingOfSockets(false);

  // Start job 1 (async OK)
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  TestSocketRequest req1(&request_order_, &completion_count_);
  int rv = InitHandle(req1.handle(), "a", kDefaultPriority, &req1,
                      pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req1.WaitForResult());

  // Job 1 finished OK.  Start job 2 (also async OK).  Release socket 1.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);

  TestSocketRequest req2(&request_order_, &completion_count_);
  rv = InitHandle(req2.handle(), "a", kDefaultPriority, &req2,
                  pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  req1.handle()->Reset();
  MessageLoop::current()->RunAllPending();  // Run the DoReleaseSocket()

  // Job 2 is pending. Start request 3 (which has no associated job since it
  // will use the idle socket).

  TestSocketRequest req3(&request_order_, &completion_count_);
  rv = InitHandle(req3.handle(), "a", kDefaultPriority, &req3,
                  pool_.get(), NULL);
  EXPECT_EQ(OK, rv);

  EXPECT_FALSE(req2.handle()->socket());
  client_socket_factory_.SignalJobs();
  EXPECT_EQ(OK, req2.WaitForResult());

  ASSERT_EQ(2U, request_order_.size());
  EXPECT_EQ(&req1, request_order_[0]);
  EXPECT_EQ(&req2, request_order_[1]);
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
}

// Regression test for http://crbug.com/17985.
TEST_F(ClientSocketPoolBaseTest, GroupWithPendingRequestsIsNotEmpty) {
  const int kMaxSockets = 3;
  const int kMaxSocketsPerGroup = 2;
  CreatePool(kMaxSockets, kMaxSocketsPerGroup);

  const int kHighPriority = kDefaultPriority + 100;

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // This is going to be a pending request in an otherwise empty group.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  // Reach the maximum socket limit.
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));

  // Create a stalled group with high priorities.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kHighPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kHighPriority));
  EXPECT_TRUE(pool_->base()->may_have_stalled_group());

  // Release the first two sockets from "a", which will make room
  // for requests from "c". After that "a" will have no active sockets
  // and one pending request.
  EXPECT_TRUE(ReleaseOneConnection(KEEP_ALIVE));
  EXPECT_TRUE(ReleaseOneConnection(KEEP_ALIVE));

  // Closing idle sockets should not get us into trouble, but in the bug
  // we were hitting a CHECK here.
  EXPECT_EQ(2, pool_->IdleSocketCountInGroup("a"));
  pool_->CloseIdleSockets();
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
}

class ClientSocketPoolBaseTest_LateBinding : public ClientSocketPoolBaseTest {
 protected:
  virtual void SetUp() {
    ClientSocketPoolBaseTest::SetUp();
    EnableLateBindingOfSockets(true);
  }
};

// Even though a timeout is specified, it doesn't time out on a synchronous
// completion.
TEST_F(ClientSocketPoolBaseTest_LateBinding,
       ConnectJob_NoTimeoutOnSynchronousCompletion) {
  TestConnectJobDelegate delegate;
  ClientSocketHandle ignored;
  TestClientSocketPoolBase::Request request(&ignored, NULL, 0, NULL, NULL);
  scoped_ptr<TestConnectJob> job(
      new TestConnectJob(TestConnectJob::kMockJob,
                         "a",
                         request,
                         base::TimeDelta::FromMicroseconds(1),
                         &delegate,
                         &client_socket_factory_,
                         NULL));
  EXPECT_EQ(OK, job->Connect());
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, ConnectJob_TimedOut) {
  TestConnectJobDelegate delegate;
  ClientSocketHandle ignored;
  TestClientSocketPoolBase::Request request(&ignored, NULL, 0, NULL, NULL);
  // Deleted by TestConnectJobDelegate.
  TestConnectJob* job =
      new TestConnectJob(TestConnectJob::kMockPendingJob,
                         "a",
                         request,
                         base::TimeDelta::FromMicroseconds(1),
                         &delegate,
                         &client_socket_factory_,
                         NULL);
  ASSERT_EQ(ERR_IO_PENDING, job->Connect());
  PlatformThread::Sleep(1);
  EXPECT_EQ(ERR_TIMED_OUT, delegate.WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, BasicSynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  scoped_refptr<LoadLog> log(new LoadLog);
  EXPECT_EQ(OK, InitHandle(&handle, "a", kDefaultPriority, &callback,
                           pool_.get(), log));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  handle.Reset();

  EXPECT_EQ(4u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, BasicAsynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(&request_order_, &completion_count_);
  scoped_refptr<LoadLog> log(new LoadLog);
  int rv = InitHandle(req.handle(), "a", 0, &req, pool_.get(), log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", req.handle()));
  EXPECT_EQ(OK, req.WaitForResult());
  EXPECT_TRUE(req.handle()->is_initialized());
  EXPECT_TRUE(req.handle()->socket());
  req.handle()->Reset();

  EXPECT_EQ(6u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 4, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 5, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, InitConnectionFailure) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockFailingJob);
  TestSocketRequest req(&request_order_, &completion_count_);
  scoped_refptr<LoadLog> log(new LoadLog);
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            InitHandle(req.handle(), "a", kDefaultPriority, &req,
                       pool_.get(), log));

  EXPECT_EQ(4u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest_LateBinding,
       InitConnectionAsynchronousFailure) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);
  TestSocketRequest req(&request_order_, &completion_count_);
  scoped_refptr<LoadLog> log(new LoadLog);
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(req.handle(), "a", kDefaultPriority, &req,
                       pool_.get(), log));
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", req.handle()));
  EXPECT_EQ(ERR_CONNECTION_FAILED, req.WaitForResult());

  EXPECT_EQ(6u, log->events().size());
  ExpectLogContains(log, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 1, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 2, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 3, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log, 4, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log, 5, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, PendingRequests) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(kDefaultMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup, completion_count_);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(6, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(3, GetOrderOfRequest(5));
  EXPECT_EQ(5, GetOrderOfRequest(6));
  EXPECT_EQ(7, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(8));
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, PendingRequests_NoKeepAlive) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));

  ReleaseAllConnections(NO_KEEP_ALIVE);

  for (size_t i = kDefaultMaxSocketsPerGroup; i < requests_.size(); ++i)
    EXPECT_EQ(OK, requests_[i]->WaitForResult());

  EXPECT_EQ(static_cast<int>(requests_.size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup, completion_count_);
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending connect job will be cancelled and should not call back into
// ClientSocketPoolBase.
TEST_F(ClientSocketPoolBaseTest_LateBinding, CancelRequestClearGroup) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(&request_order_, &completion_count_);
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(req.handle(), "a", kDefaultPriority, &req,
                       pool_.get(), NULL));
  req.handle()->Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, TwoRequestsCancelOne) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(&request_order_, &completion_count_);
  TestSocketRequest req2(&request_order_, &completion_count_);

  scoped_refptr<LoadLog> log1(new LoadLog);
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(req.handle(), "a", kDefaultPriority, &req,
                       pool_.get(), log1));
  scoped_refptr<LoadLog> log2(new LoadLog);
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(req2.handle(), "a", kDefaultPriority, &req2,
                       pool_.get(), log2));

  req.handle()->Reset();

  EXPECT_EQ(5u, log1->events().size());
  ExpectLogContains(log1, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log1, 1, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log1, 2, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_END);
  ExpectLogContains(log1, 3, LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
  ExpectLogContains(log1, 4, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);

  // At this point, request 2 is just waiting for the connect job to finish.
  EXPECT_EQ(2u, log2->events().size());
  ExpectLogContains(log2, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log2, 1, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_BEGIN);

  EXPECT_EQ(OK, req2.WaitForResult());
  req2.handle()->Reset();

  // Now request 2 has actually finished.
  EXPECT_EQ(6u, log2->events().size());
  ExpectLogContains(log2, 0, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_BEGIN);
  ExpectLogContains(log2, 1, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log1, 2, LoadLog::TYPE_SOCKET_POOL_WAITING_IN_QUEUE,
                    LoadLog::PHASE_END);
  ExpectLogContains(log2, 3, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_BEGIN);
  ExpectLogContains(log2, 4, LoadLog::TYPE_SOCKET_POOL_CONNECT_JOB,
                    LoadLog::PHASE_END);
  ExpectLogContains(log2, 5, LoadLog::TYPE_SOCKET_POOL, LoadLog::PHASE_END);

}

TEST_F(ClientSocketPoolBaseTest_LateBinding, ConnectCancelConnect) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  TestSocketRequest req(&request_order_, &completion_count_);

  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(&handle, "a", kDefaultPriority, &callback,
                       pool_.get(), NULL));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            InitHandle(&handle, "a", kDefaultPriority, &callback2,
                       pool_.get(), NULL));

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, CancelRequest) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));

  // Cancel a request.
  size_t index_to_cancel = kDefaultMaxSocketsPerGroup + 2;
  EXPECT_FALSE(requests_[index_to_cancel]->handle()->is_initialized());
  requests_[index_to_cancel]->handle()->Reset();

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(kDefaultMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup - 1,
            completion_count_);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(5, GetOrderOfRequest(3));
  EXPECT_EQ(3, GetOrderOfRequest(4));
  EXPECT_EQ(kRequestNotFound, GetOrderOfRequest(5));  // Canceled request.
  EXPECT_EQ(4, GetOrderOfRequest(6));
  EXPECT_EQ(6, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(kIndexOutOfBounds, GetOrderOfRequest(8));
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, CancelRequestLimitsJobs) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));

  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->NumConnectJobsInGroup("a"));
  requests_[2]->handle()->Reset();
  requests_[3]->handle()->Reset();
  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->NumConnectJobsInGroup("a"));

  requests_[1]->handle()->Reset();
  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->NumConnectJobsInGroup("a"));

  requests_[0]->handle()->Reset();
  EXPECT_EQ(kDefaultMaxSocketsPerGroup - 1, pool_->NumConnectJobsInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, RequestPendingJobTwice) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  RequestSocketCallback callback(
      &handle, pool_.get(), connect_job_factory_,
      TestConnectJob::kMockPendingJob);
  int rv = InitHandle(
      &handle, "a", kDefaultPriority, &callback, pool_.get(), NULL);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, RequestPendingJobThenSynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  RequestSocketCallback callback(
      &handle, pool_.get(), connect_job_factory_, TestConnectJob::kMockJob);
  int rv = InitHandle(
      &handle, "a", kDefaultPriority, &callback,
      pool_.get(), NULL);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(ClientSocketPoolBaseTest_LateBinding,
       CancelActiveRequestWithPendingRequests) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  // Now, kDefaultMaxSocketsPerGroup requests should be active.
  // Let's cancel them.
  for (int i = 0; i < kDefaultMaxSocketsPerGroup; ++i) {
    ASSERT_FALSE(requests_[i]->handle()->is_initialized());
    requests_[i]->handle()->Reset();
  }

  // Let's wait for the rest to complete now.
  for (size_t i = kDefaultMaxSocketsPerGroup; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult());
    requests_[i]->handle()->Reset();
  }

  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup, completion_count_);
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(ClientSocketPoolBaseTest_LateBinding,
       FailingActiveRequestWithPendingRequests) {
  const int kMaxSockets = 5;
  CreatePool(kMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  const int kNumberOfRequests = 2 * kDefaultMaxSocketsPerGroup + 1;
  ASSERT_LE(kNumberOfRequests, kMaxSockets);  // Otherwise the test hangs.

  // Queue up all the requests
  for (int i = 0; i < kNumberOfRequests; ++i)
    EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  for (int i = 0; i < kNumberOfRequests; ++i)
    EXPECT_EQ(ERR_CONNECTION_FAILED, requests_[i]->WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest_LateBinding,
       CancelActiveRequestThenRequestSocket) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  TestSocketRequest req(&request_order_, &completion_count_);
  int rv = InitHandle(req.handle(), "a", kDefaultPriority, &req,
                      pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel the active request.
  req.handle()->Reset();

  rv = InitHandle(req.handle(), "a", kDefaultPriority, &req,
                  pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req.WaitForResult());

  EXPECT_FALSE(req.handle()->is_reused());
  EXPECT_EQ(1U, completion_count_);
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// When requests and ConnectJobs are not coupled, the request will get serviced
// by whatever comes first.
TEST_F(ClientSocketPoolBaseTest_LateBinding, ReleaseSockets) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  // Start job 1 (async OK)
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  TestSocketRequest req1(&request_order_, &completion_count_);
  int rv = InitHandle(req1.handle(), "a", kDefaultPriority,
                      &req1, pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req1.WaitForResult());

  // Job 1 finished OK.  Start job 2 (also async OK).  Request 3 is pending
  // without a job.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);

  TestSocketRequest req2(&request_order_, &completion_count_);
  rv = InitHandle(req2.handle(), "a", kDefaultPriority, &req2,
                  pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  TestSocketRequest req3(&request_order_, &completion_count_);
  rv = InitHandle(
      req3.handle(), "a", kDefaultPriority, &req3, pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Both Requests 2 and 3 are pending.  We release socket 1 which should
  // service request 2.  Request 3 should still be waiting.
  req1.handle()->Reset();
  MessageLoop::current()->RunAllPending();  // Run the DoReleaseSocket()
  ASSERT_TRUE(req2.handle()->socket());
  EXPECT_EQ(OK, req2.WaitForResult());
  EXPECT_FALSE(req3.handle()->socket());

  // Signal job 2, which should service request 3.

  client_socket_factory_.SignalJobs();
  EXPECT_EQ(OK, req3.WaitForResult());

  ASSERT_EQ(3U, request_order_.size());
  EXPECT_EQ(&req1, request_order_[0]);
  EXPECT_EQ(&req2, request_order_[1]);
  EXPECT_EQ(&req3, request_order_[2]);
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
}

// The requests are not coupled to the jobs.  So, the requests should finish in
// their priority / insertion order.
TEST_F(ClientSocketPoolBaseTest_LateBinding, PendingJobCompletionOrder) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  // First two jobs are async.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  TestSocketRequest req1(&request_order_, &completion_count_);
  int rv = InitHandle(
      req1.handle(), "a", kDefaultPriority, &req1, pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestSocketRequest req2(&request_order_, &completion_count_);
  rv = InitHandle(req2.handle(), "a", kDefaultPriority, &req2,
                  pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // The pending job is sync.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  TestSocketRequest req3(&request_order_, &completion_count_);
  rv = InitHandle(
      req3.handle(), "a", kDefaultPriority, &req3, pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(ERR_CONNECTION_FAILED, req1.WaitForResult());
  EXPECT_EQ(OK, req2.WaitForResult());
  EXPECT_EQ(ERR_CONNECTION_FAILED, req3.WaitForResult());

  ASSERT_EQ(3U, request_order_.size());
  EXPECT_EQ(&req1, request_order_[0]);
  EXPECT_EQ(&req2, request_order_[1]);
  EXPECT_EQ(&req3, request_order_[2]);
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, DISABLED_LoadState) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(
      TestConnectJob::kMockAdvancingLoadStateJob);

  TestSocketRequest req1(&request_order_, &completion_count_);
  int rv = InitHandle(
      req1.handle(), "a", kDefaultPriority, &req1, pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_IDLE, req1.handle()->GetLoadState());

  MessageLoop::current()->RunAllPending();

  TestSocketRequest req2(&request_order_, &completion_count_);
  rv = InitHandle(req2.handle(), "a", kDefaultPriority, &req2,
                  pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, req1.handle()->GetLoadState());
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, req2.handle()->GetLoadState());
}

// Regression test for http://crbug.com/17985.
TEST_F(ClientSocketPoolBaseTest_LateBinding,
       GroupWithPendingRequestsIsNotEmpty) {
  const int kMaxSockets = 3;
  const int kMaxSocketsPerGroup = 2;
  CreatePool(kMaxSockets, kMaxSocketsPerGroup);

  const int kHighPriority = kDefaultPriority + 100;

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // This is going to be a pending request in an otherwise empty group.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  // Reach the maximum socket limit.
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));

  // Create a stalled group with high priorities.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kHighPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kHighPriority));
  EXPECT_TRUE(pool_->base()->may_have_stalled_group());

  // Release the first two sockets from "a", which will make room
  // for requests from "c". After that "a" will have no active sockets
  // and one pending request.
  EXPECT_TRUE(ReleaseOneConnection(KEEP_ALIVE));
  EXPECT_TRUE(ReleaseOneConnection(KEEP_ALIVE));

  // Closing idle sockets should not get us into trouble, but in the bug
  // we were hitting a CHECK here.
  EXPECT_EQ(2, pool_->IdleSocketCountInGroup("a"));
  pool_->CloseIdleSockets();
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, CleanupTimedOutIdleSockets) {
  CreatePoolWithIdleTimeouts(
      kDefaultMaxSockets, kDefaultMaxSocketsPerGroup,
      base::TimeDelta(),  // Time out unused sockets immediately.
      base::TimeDelta::FromDays(1));  // Don't time out used sockets.

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  // Startup two mock pending connect jobs, which will sit in the MessageLoop.

  TestSocketRequest req(&request_order_, &completion_count_);
  int rv = InitHandle(req.handle(), "a", 0, &req, pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", req.handle()));

  TestSocketRequest req2(&request_order_, &completion_count_);
  rv = InitHandle(req2.handle(), "a", 0, &req2, pool_.get(), NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", req2.handle()));

  // Cancel one of the requests.  Wait for the other, which will get the first
  // job.  Release the socket.  Run the loop again to make sure the second
  // socket is sitting idle and the first one is released (since ReleaseSocket()
  // just posts a DoReleaseSocket() task).

  req.handle()->Reset();
  EXPECT_EQ(OK, req2.WaitForResult());
  req2.handle()->Reset();

  // We post all of our delayed tasks with a 2ms delay. I.e. they don't
  // actually become pending until 2ms after they have been created. In order
  // to flush all tasks, we need to wait so that we know there are no
  // soon-to-be-pending tasks waiting.
  PlatformThread::Sleep(10);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(2, pool_->IdleSocketCount());

  // Invoke the idle socket cleanup check.  Only one socket should be left, the
  // used socket.  Request it to make sure that it's used.

  pool_->CleanupTimedOutIdleSockets();
  rv = InitHandle(req.handle(), "a", 0, &req, pool_.get(), NULL);
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(req.handle()->is_reused());
}

}  // namespace

}  // namespace net
