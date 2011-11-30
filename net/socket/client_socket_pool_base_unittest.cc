// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/request_priority.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_host_info.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::Return;

namespace net {

namespace {

const int kDefaultMaxSockets = 4;
const int kDefaultMaxSocketsPerGroup = 2;
const net::RequestPriority kDefaultPriority = MEDIUM;

class TestSocketParams : public base::RefCounted<TestSocketParams> {
 public:
  TestSocketParams() : ignore_limits_(false) {}

  void set_ignore_limits(bool ignore_limits) {
    ignore_limits_ = ignore_limits;
  }
  bool ignore_limits() { return ignore_limits_; }

 private:
  friend class base::RefCounted<TestSocketParams>;
  ~TestSocketParams() {}

  bool ignore_limits_;
};
typedef ClientSocketPoolBase<TestSocketParams> TestClientSocketPoolBase;

class MockClientSocket : public StreamSocket {
 public:
  MockClientSocket() : connected_(false), was_used_to_convey_data_(false),
                       num_bytes_read_(0) {}

  // Socket methods:
  virtual int Read(
      IOBuffer* /* buf */, int len, OldCompletionCallback* /* callback */) {
    num_bytes_read_ += len;
    return len;
  }

  virtual int Write(
      IOBuffer* /* buf */, int len, OldCompletionCallback* /* callback */) {
    was_used_to_convey_data_ = true;
    return len;
  }
  virtual bool SetReceiveBufferSize(int32 size) { return true; }
  virtual bool SetSendBufferSize(int32 size) { return true; }

  // StreamSocket methods:

  virtual int Connect(OldCompletionCallback* callback) {
    connected_ = true;
    return OK;
  }

  virtual void Disconnect() { connected_ = false; }
  virtual bool IsConnected() const { return connected_; }
  virtual bool IsConnectedAndIdle() const { return connected_; }

  virtual int GetPeerAddress(AddressList* /* address */) const {
    return ERR_UNEXPECTED;
  }

  virtual int GetLocalAddress(IPEndPoint* /* address */) const {
    return ERR_UNEXPECTED;
  }

  virtual const BoundNetLog& NetLog() const {
    return net_log_;
  }

  virtual void SetSubresourceSpeculation() {}
  virtual void SetOmniboxSpeculation() {}
  virtual bool WasEverUsed() const {
    return was_used_to_convey_data_ || num_bytes_read_ > 0;
  }
  virtual bool UsingTCPFastOpen() const { return false; }
  virtual int64 NumBytesRead() const { return num_bytes_read_; }
  virtual base::TimeDelta GetConnectTimeMicros() const {
    static const base::TimeDelta kDummyConnectTimeMicros =
        base::TimeDelta::FromMicroseconds(10);
    return kDummyConnectTimeMicros;  // Dummy value.
  }

 private:
  bool connected_;
  BoundNetLog net_log_;
  bool was_used_to_convey_data_;
  int num_bytes_read_;

  DISALLOW_COPY_AND_ASSIGN(MockClientSocket);
};

class TestConnectJob;

class MockClientSocketFactory : public ClientSocketFactory {
 public:
  MockClientSocketFactory() : allocation_count_(0) {}

  virtual DatagramClientSocket* CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      NetLog* net_log,
      const NetLog::Source& source) {
    NOTREACHED();
    return NULL;
  }

  virtual StreamSocket* CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog* /* net_log */,
      const NetLog::Source& /*source*/) {
    allocation_count_++;
    return NULL;
  }

  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocketHandle* transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      SSLHostInfo* ssl_host_info,
      const SSLClientSocketContext& context) {
    NOTIMPLEMENTED();
    delete ssl_host_info;
    return NULL;
  }

  virtual void ClearSSLSessionCache() {
    NOTIMPLEMENTED();
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
    kMockRecoverableJob,
    kMockPendingRecoverableJob,
    kMockAdditionalErrorStateJob,
    kMockPendingAdditionalErrorStateJob,
  };

  // The kMockPendingJob uses a slight delay before allowing the connect
  // to complete.
  static const int kPendingConnectDelay = 2;

  TestConnectJob(JobType job_type,
                 const std::string& group_name,
                 const TestClientSocketPoolBase::Request& request,
                 base::TimeDelta timeout_duration,
                 ConnectJob::Delegate* delegate,
                 MockClientSocketFactory* client_socket_factory,
                 NetLog* net_log)
      : ConnectJob(group_name, timeout_duration, delegate,
                   BoundNetLog::Make(net_log, NetLog::SOURCE_CONNECT_JOB)),
        job_type_(job_type),
        client_socket_factory_(client_socket_factory),
        method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        load_state_(LOAD_STATE_IDLE),
        store_additional_error_state_(false) {}

  void Signal() {
    DoConnect(waiting_success_, true /* async */, false /* recoverable */);
  }

  virtual LoadState GetLoadState() const { return load_state_; }

  virtual void GetAdditionalErrorState(ClientSocketHandle* handle) {
    if (store_additional_error_state_) {
      // Set all of the additional error state fields in some way.
      handle->set_is_ssl_error(true);
      HttpResponseInfo info;
      info.headers = new HttpResponseHeaders("");
      handle->set_ssl_error_response_info(info);
    }
  }

 private:
  // ConnectJob methods:

  virtual int ConnectInternal() {
    AddressList ignored;
    client_socket_factory_->CreateTransportClientSocket(
        ignored, NULL, net::NetLog::Source());
    set_socket(new MockClientSocket());
    switch (job_type_) {
      case kMockJob:
        return DoConnect(true /* successful */, false /* sync */,
                         false /* recoverable */);
      case kMockFailingJob:
        return DoConnect(false /* error */, false /* sync */,
                         false /* recoverable */);
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
                true /* async */,
                false /* recoverable */),
            kPendingConnectDelay);
        return ERR_IO_PENDING;
      case kMockPendingFailingJob:
        set_load_state(LOAD_STATE_CONNECTING);
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &TestConnectJob::DoConnect,
                false /* error */,
                true  /* async */,
                false /* recoverable */),
            2);
        return ERR_IO_PENDING;
      case kMockWaitingJob:
        client_socket_factory_->WaitForSignal(this);
        waiting_success_ = true;
        return ERR_IO_PENDING;
      case kMockAdvancingLoadStateJob:
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &TestConnectJob::AdvanceLoadState, load_state_));
        return ERR_IO_PENDING;
      case kMockRecoverableJob:
        return DoConnect(false /* error */, false /* sync */,
                         true /* recoverable */);
      case kMockPendingRecoverableJob:
        set_load_state(LOAD_STATE_CONNECTING);
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &TestConnectJob::DoConnect,
                false /* error */,
                true  /* async */,
                true  /* recoverable */),
            2);
        return ERR_IO_PENDING;
      case kMockAdditionalErrorStateJob:
        store_additional_error_state_ = true;
        return DoConnect(false /* error */, false /* sync */,
                         false /* recoverable */);
      case kMockPendingAdditionalErrorStateJob:
        set_load_state(LOAD_STATE_CONNECTING);
        store_additional_error_state_ = true;
        MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &TestConnectJob::DoConnect,
                false /* error */,
                true  /* async */,
                false /* recoverable */),
            2);
        return ERR_IO_PENDING;
      default:
        NOTREACHED();
        set_socket(NULL);
        return ERR_FAILED;
    }
  }

  void set_load_state(LoadState load_state) { load_state_ = load_state; }

  int DoConnect(bool succeed, bool was_async, bool recoverable) {
    int result = OK;
    if (succeed) {
      socket()->Connect(NULL);
    } else if (recoverable) {
      result = ERR_PROXY_AUTH_REQUESTED;
    } else {
      result = ERR_CONNECTION_FAILED;
      set_socket(NULL);
    }

    if (was_async)
      NotifyDelegateOfCompletion(result);
    return result;
  }

  // This function helps simulate the progress of load states on a ConnectJob.
  // Each time it is called it advances the load state and posts a task to be
  // called again.  It stops at the last connecting load state (the one
  // before LOAD_STATE_SENDING_REQUEST).
  void AdvanceLoadState(LoadState state) {
    int tmp = state;
    tmp++;
    if (tmp < LOAD_STATE_SENDING_REQUEST) {
      state = static_cast<LoadState>(tmp);
      set_load_state(state);
      MessageLoop::current()->PostTask(
          FROM_HERE,
          method_factory_.NewRunnableMethod(&TestConnectJob::AdvanceLoadState,
                                            state));
    }
  }

  bool waiting_success_;
  const JobType job_type_;
  MockClientSocketFactory* const client_socket_factory_;
  ScopedRunnableMethodFactory<TestConnectJob> method_factory_;
  LoadState load_state_;
  bool store_additional_error_state_;

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
      ConnectJob::Delegate* delegate) const {
    return new TestConnectJob(job_type_,
                              group_name,
                              request,
                              timeout_duration_,
                              delegate,
                              client_socket_factory_,
                              NULL);
  }

  virtual base::TimeDelta ConnectionTimeout() const {
    return timeout_duration_;
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
      ClientSocketPoolHistograms* histograms,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout,
      TestClientSocketPoolBase::ConnectJobFactory* connect_job_factory)
      : base_(max_sockets, max_sockets_per_group, histograms,
              unused_idle_socket_timeout, used_idle_socket_timeout,
              connect_job_factory) {}

  virtual ~TestClientSocketPool() {}

  virtual int RequestSocket(
      const std::string& group_name,
      const void* params,
      net::RequestPriority priority,
      ClientSocketHandle* handle,
      OldCompletionCallback* callback,
      const BoundNetLog& net_log) OVERRIDE {
    const scoped_refptr<TestSocketParams>* casted_socket_params =
        static_cast<const scoped_refptr<TestSocketParams>*>(params);
    return base_.RequestSocket(group_name, *casted_socket_params, priority,
                               handle, callback, net_log);
  }

  virtual void RequestSockets(const std::string& group_name,
                              const void* params,
                              int num_sockets,
                              const BoundNetLog& net_log) OVERRIDE {
    const scoped_refptr<TestSocketParams>* casted_params =
        static_cast<const scoped_refptr<TestSocketParams>*>(params);

    base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
  }

  virtual void CancelRequest(
      const std::string& group_name,
      ClientSocketHandle* handle) OVERRIDE {
    base_.CancelRequest(group_name, handle);
  }

  virtual void ReleaseSocket(
      const std::string& group_name,
      StreamSocket* socket,
      int id) OVERRIDE {
    base_.ReleaseSocket(group_name, socket, id);
  }

  virtual void Flush() OVERRIDE {
    base_.Flush();
  }

  virtual bool IsStalled() const OVERRIDE {
    return base_.IsStalled();
  }

  virtual void CloseIdleSockets() OVERRIDE {
    base_.CloseIdleSockets();
  }

  virtual int IdleSocketCount() const OVERRIDE {
    return base_.idle_socket_count();
  }

  virtual int IdleSocketCountInGroup(
      const std::string& group_name) const OVERRIDE {
    return base_.IdleSocketCountInGroup(group_name);
  }

  virtual LoadState GetLoadState(
      const std::string& group_name,
      const ClientSocketHandle* handle) const OVERRIDE {
    return base_.GetLoadState(group_name, handle);
  }

  virtual void AddLayeredPool(LayeredPool* pool) OVERRIDE {
    base_.AddLayeredPool(pool);
  }

  virtual void RemoveLayeredPool(LayeredPool* pool) OVERRIDE {
    base_.RemoveLayeredPool(pool);
  }

  virtual DictionaryValue* GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const OVERRIDE {
    return base_.GetInfoAsValue(name, type);
  }

  virtual base::TimeDelta ConnectionTimeout() const OVERRIDE {
    return base_.ConnectionTimeout();
  }

  virtual ClientSocketPoolHistograms* histograms() const OVERRIDE {
    return base_.histograms();
  }

  const TestClientSocketPoolBase* base() const { return &base_; }

  int NumConnectJobsInGroup(const std::string& group_name) const {
    return base_.NumConnectJobsInGroup(group_name);
  }

  int NumActiveSocketsInGroup(const std::string& group_name) const {
    return base_.NumActiveSocketsInGroup(group_name);
  }

  bool HasGroup(const std::string& group_name) const {
    return base_.HasGroup(group_name);
  }

  void CleanupTimedOutIdleSockets() { base_.CleanupIdleSockets(false); }

  void EnableConnectBackupJobs() { base_.EnableConnectBackupJobs(); }

  bool CloseOneIdleConnectionInLayeredPool() {
    return base_.CloseOneIdleConnectionInLayeredPool();
  }

 private:
  TestClientSocketPoolBase base_;

  DISALLOW_COPY_AND_ASSIGN(TestClientSocketPool);
};

}  // namespace

REGISTER_SOCKET_PARAMS_FOR_POOL(TestClientSocketPool, TestSocketParams);

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
    scoped_ptr<StreamSocket> socket(job->ReleaseSocket());
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

class ClientSocketPoolBaseTest : public testing::Test {
 protected:
  ClientSocketPoolBaseTest()
      : params_(new TestSocketParams()),
        histograms_("ClientSocketPoolTest") {
    connect_backup_jobs_enabled_ =
        internal::ClientSocketPoolBaseHelper::connect_backup_jobs_enabled();
    internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(true);
    cleanup_timer_enabled_ =
        internal::ClientSocketPoolBaseHelper::cleanup_timer_enabled();
  }

  virtual ~ClientSocketPoolBaseTest() {
    internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        connect_backup_jobs_enabled_);
    internal::ClientSocketPoolBaseHelper::set_cleanup_timer_enabled(
        cleanup_timer_enabled_);
  }

  void CreatePool(int max_sockets, int max_sockets_per_group) {
    CreatePoolWithIdleTimeouts(
        max_sockets,
        max_sockets_per_group,
        ClientSocketPool::unused_idle_socket_timeout(),
        ClientSocketPool::used_idle_socket_timeout());
  }

  void CreatePoolWithIdleTimeouts(
      int max_sockets, int max_sockets_per_group,
      base::TimeDelta unused_idle_socket_timeout,
      base::TimeDelta used_idle_socket_timeout) {
    DCHECK(!pool_.get());
    connect_job_factory_ = new TestConnectJobFactory(&client_socket_factory_);
    pool_.reset(new TestClientSocketPool(max_sockets,
                                         max_sockets_per_group,
                                         &histograms_,
                                         unused_idle_socket_timeout,
                                         used_idle_socket_timeout,
                                         connect_job_factory_));
  }

  int StartRequest(const std::string& group_name,
                   net::RequestPriority priority) {
    return test_base_.StartRequestUsingPool<
        TestClientSocketPool, TestSocketParams>(
            pool_.get(), group_name, priority, params_);
  }

  int GetOrderOfRequest(size_t index) const {
    return test_base_.GetOrderOfRequest(index);
  }

  bool ReleaseOneConnection(ClientSocketPoolTest::KeepAlive keep_alive) {
    return test_base_.ReleaseOneConnection(keep_alive);
  }

  void ReleaseAllConnections(ClientSocketPoolTest::KeepAlive keep_alive) {
    test_base_.ReleaseAllConnections(keep_alive);
  }

  TestSocketRequest* request(int i) { return test_base_.request(i); }
  size_t requests_size() const { return test_base_.requests_size(); }
  ScopedVector<TestSocketRequest>* requests() { return test_base_.requests(); }
  size_t completion_count() const { return test_base_.completion_count(); }

  bool connect_backup_jobs_enabled_;
  bool cleanup_timer_enabled_;
  MockClientSocketFactory client_socket_factory_;
  TestConnectJobFactory* connect_job_factory_;
  scoped_refptr<TestSocketParams> params_;
  ClientSocketPoolHistograms histograms_;
  scoped_ptr<TestClientSocketPool> pool_;
  ClientSocketPoolTest test_base_;
};

TEST_F(ClientSocketPoolBaseTest, AssignIdleSocketToGroup_WarmestSocket) {
  CreatePool(4, 4);
  net::SetSocketReusePolicy(0);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  std::map<int, StreamSocket*> sockets_;
  for (size_t i = 0; i < test_base_.requests_size(); i++) {
    TestSocketRequest* req = test_base_.request(i);
    StreamSocket* s = req->handle()->socket();
    MockClientSocket* sock = static_cast<MockClientSocket*>(s);
    CHECK(sock);
    sockets_[i] = sock;
    sock->Read(NULL, 1024 - i, NULL);
  }

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  TestSocketRequest* req = test_base_.request(test_base_.requests_size() - 1);

  // First socket is warmest.
  EXPECT_EQ(sockets_[0], req->handle()->socket());

  // Test that NumBytes are as expected.
  EXPECT_EQ(1024, sockets_[0]->NumBytesRead());
  EXPECT_EQ(1023, sockets_[1]->NumBytesRead());
  EXPECT_EQ(1022, sockets_[2]->NumBytesRead());
  EXPECT_EQ(1021, sockets_[3]->NumBytesRead());

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);
}

TEST_F(ClientSocketPoolBaseTest, AssignIdleSocketToGroup_LastAccessedSocket) {
  CreatePool(4, 4);
  net::SetSocketReusePolicy(2);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  std::map<int, StreamSocket*> sockets_;
  for (size_t i = 0; i < test_base_.requests_size(); i++) {
    TestSocketRequest* req = test_base_.request(i);
    StreamSocket* s = req->handle()->socket();
    MockClientSocket* sock = static_cast<MockClientSocket*>(s);
    CHECK(sock);
    sockets_[i] = sock;
    sock->Read(NULL, 1024 - i, NULL);
  }

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  TestSocketRequest* req = test_base_.request(test_base_.requests_size() - 1);

  // Last socket is most recently accessed.
  EXPECT_EQ(sockets_[3], req->handle()->socket());
  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);
}

// Even though a timeout is specified, it doesn't time out on a synchronous
// completion.
TEST_F(ClientSocketPoolBaseTest, ConnectJob_NoTimeoutOnSynchronousCompletion) {
  TestConnectJobDelegate delegate;
  ClientSocketHandle ignored;
  TestClientSocketPoolBase::Request request(
      &ignored, NULL, kDefaultPriority,
      internal::ClientSocketPoolBaseHelper::NORMAL,
      false, params_, BoundNetLog());
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
  CapturingNetLog log(CapturingNetLog::kUnbounded);

  TestClientSocketPoolBase::Request request(
      &ignored, NULL, kDefaultPriority,
      internal::ClientSocketPoolBaseHelper::NORMAL,
      false, params_, BoundNetLog());
  // Deleted by TestConnectJobDelegate.
  TestConnectJob* job =
      new TestConnectJob(TestConnectJob::kMockPendingJob,
                         "a",
                         request,
                         base::TimeDelta::FromMicroseconds(1),
                         &delegate,
                         &client_socket_factory_,
                         &log);
  ASSERT_EQ(ERR_IO_PENDING, job->Connect());
  base::PlatformThread::Sleep(1);
  EXPECT_EQ(ERR_TIMED_OUT, delegate.WaitForResult());

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(6u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB));
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 1, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_CONNECT));
  EXPECT_TRUE(LogContainsEvent(
      entries, 2, NetLog::TYPE_CONNECT_JOB_SET_SOCKET,
      NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEvent(
      entries, 3, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_TIMED_OUT,
      NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 4, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_CONNECT));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 5, NetLog::TYPE_SOCKET_POOL_CONNECT_JOB));
}

TEST_F(ClientSocketPoolBaseTest, BasicSynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  TestOldCompletionCallback callback;
  ClientSocketHandle handle;
  CapturingBoundNetLog log(CapturingNetLog::kUnbounded);

  EXPECT_EQ(OK,
            handle.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback,
                        pool_.get(),
                        log.bound()));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  handle.Reset();

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(4u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKET_POOL));
  EXPECT_TRUE(LogContainsEvent(
      entries, 1, NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEvent(
      entries, 2, NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
      NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 3, NetLog::TYPE_SOCKET_POOL));
}

TEST_F(ClientSocketPoolBaseTest, InitConnectionFailure) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockFailingJob);
  CapturingBoundNetLog log(CapturingNetLog::kUnbounded);

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  // Set the additional error state members to ensure that they get cleared.
  handle.set_is_ssl_error(true);
  HttpResponseInfo info;
  info.headers = new HttpResponseHeaders("");
  handle.set_ssl_error_response_info(info);
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            handle.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback,
                        pool_.get(),
                        log.bound()));
  EXPECT_FALSE(handle.socket());
  EXPECT_FALSE(handle.is_ssl_error());
  EXPECT_TRUE(handle.ssl_error_response_info().headers.get() == NULL);

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(3u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKET_POOL));
  EXPECT_TRUE(LogContainsEvent(
      entries, 1, NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 2, NetLog::TYPE_SOCKET_POOL));
}

TEST_F(ClientSocketPoolBaseTest, TotalLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  // TODO(eroman): Check that the NetLog contains this event.

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("c", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("d", kDefaultPriority));

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_size() - kDefaultMaxSockets, completion_count());

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("e", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("f", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("g", kDefaultPriority));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_size() - kDefaultMaxSockets, completion_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));
  EXPECT_EQ(7, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(8));
}

TEST_F(ClientSocketPoolBaseTest, TotalLimitReachedNewGroup) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  // TODO(eroman): Check that the NetLog contains this event.

  // Reach all limits: max total sockets, and max sockets per group.
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_size() - kDefaultMaxSockets, completion_count());

  // Now create a new group and verify that we don't starve it.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kDefaultPriority));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_size() - kDefaultMaxSockets, completion_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(6));
}

TEST_F(ClientSocketPoolBaseTest, TotalLimitRespectsPriority) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("b", LOWEST));
  EXPECT_EQ(OK, StartRequest("a", MEDIUM));
  EXPECT_EQ(OK, StartRequest("b", HIGHEST));
  EXPECT_EQ(OK, StartRequest("a", LOWEST));

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("b", HIGHEST));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  EXPECT_EQ(requests_size() - kDefaultMaxSockets, completion_count());

  // First 4 requests don't have to wait, and finish in order.
  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));

  // Request ("b", HIGHEST) has the highest priority, then ("a", MEDIUM),
  // and then ("c", LOWEST).
  EXPECT_EQ(7, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));
  EXPECT_EQ(5, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(9));
}

TEST_F(ClientSocketPoolBaseTest, TotalLimitRespectsGroupLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", LOWEST));
  EXPECT_EQ(OK, StartRequest("a", LOW));
  EXPECT_EQ(OK, StartRequest("b", HIGHEST));
  EXPECT_EQ(OK, StartRequest("b", MEDIUM));

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("b", HIGHEST));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_size() - kDefaultMaxSockets, completion_count());

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
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(8));
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
  base::PlatformThread::Sleep(10);
  MessageLoop::current()->RunAllPending();

  // The next synchronous request should wait for its turn.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("e", kDefaultPriority));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(6));
}

TEST_F(ClientSocketPoolBaseTest, CorrectlyCountStalledGroups) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSockets);
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);

  EXPECT_EQ(kDefaultMaxSockets, client_socket_factory_.allocation_count());

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("b", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kDefaultPriority));

  EXPECT_EQ(kDefaultMaxSockets, client_socket_factory_.allocation_count());

  EXPECT_TRUE(ReleaseOneConnection(ClientSocketPoolTest::KEEP_ALIVE));
  EXPECT_EQ(kDefaultMaxSockets + 1, client_socket_factory_.allocation_count());
  EXPECT_TRUE(ReleaseOneConnection(ClientSocketPoolTest::KEEP_ALIVE));
  EXPECT_EQ(kDefaultMaxSockets + 2, client_socket_factory_.allocation_count());
  EXPECT_TRUE(ReleaseOneConnection(ClientSocketPoolTest::KEEP_ALIVE));
  EXPECT_TRUE(ReleaseOneConnection(ClientSocketPoolTest::KEEP_ALIVE));
  EXPECT_EQ(kDefaultMaxSockets + 2, client_socket_factory_.allocation_count());
}

TEST_F(ClientSocketPoolBaseTest, StallAndThenCancelAndTriggerAvailableSocket) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSockets);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback,
                        pool_.get(),
                        BoundNetLog()));

  ClientSocketHandle handles[4];
  for (size_t i = 0; i < arraysize(handles); ++i) {
    TestOldCompletionCallback callback;
    EXPECT_EQ(ERR_IO_PENDING,
              handles[i].Init("b",
                              params_,
                              kDefaultPriority,
                              &callback,
                              pool_.get(),
                              BoundNetLog()));
  }

  // One will be stalled, cancel all the handles now.
  // This should hit the OnAvailableSocketSlot() code where we previously had
  // stalled groups, but no longer have any.
  for (size_t i = 0; i < arraysize(handles); ++i)
    handles[i].Reset();
}

TEST_F(ClientSocketPoolBaseTest, CancelStalledSocketAtSocketLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  {
    ClientSocketHandle handles[kDefaultMaxSockets];
    TestOldCompletionCallback callbacks[kDefaultMaxSockets];
    for (int i = 0; i < kDefaultMaxSockets; ++i) {
      EXPECT_EQ(OK, handles[i].Init(base::IntToString(i),
                                    params_,
                                    kDefaultPriority,
                                    &callbacks[i],
                                    pool_.get(),
                                    BoundNetLog()));
    }

    // Force a stalled group.
    ClientSocketHandle stalled_handle;
    TestOldCompletionCallback callback;
    EXPECT_EQ(ERR_IO_PENDING, stalled_handle.Init("foo",
                                                  params_,
                                                  kDefaultPriority,
                                                  &callback,
                                                  pool_.get(),
                                                  BoundNetLog()));

    // Cancel the stalled request.
    stalled_handle.Reset();

    EXPECT_EQ(kDefaultMaxSockets, client_socket_factory_.allocation_count());
    EXPECT_EQ(0, pool_->IdleSocketCount());

    // Dropping out of scope will close all handles and return them to idle.
  }

  EXPECT_EQ(kDefaultMaxSockets, client_socket_factory_.allocation_count());
  EXPECT_EQ(kDefaultMaxSockets, pool_->IdleSocketCount());
}

TEST_F(ClientSocketPoolBaseTest, CancelPendingSocketAtSocketLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);

  {
    ClientSocketHandle handles[kDefaultMaxSockets];
    for (int i = 0; i < kDefaultMaxSockets; ++i) {
      TestOldCompletionCallback callback;
      EXPECT_EQ(ERR_IO_PENDING, handles[i].Init(base::IntToString(i),
                                                params_,
                                                kDefaultPriority,
                                                &callback,
                                                pool_.get(),
                                                BoundNetLog()));
    }

    // Force a stalled group.
    connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
    ClientSocketHandle stalled_handle;
    TestOldCompletionCallback callback;
    EXPECT_EQ(ERR_IO_PENDING, stalled_handle.Init("foo",
                                                  params_,
                                                  kDefaultPriority,
                                                  &callback,
                                                  pool_.get(),
                                                  BoundNetLog()));

    // Since it is stalled, it should have no connect jobs.
    EXPECT_EQ(0, pool_->NumConnectJobsInGroup("foo"));

    // Cancel the stalled request.
    handles[0].Reset();

    // Now we should have a connect job.
    EXPECT_EQ(1, pool_->NumConnectJobsInGroup("foo"));

    // The stalled socket should connect.
    EXPECT_EQ(OK, callback.WaitForResult());

    EXPECT_EQ(kDefaultMaxSockets + 1,
              client_socket_factory_.allocation_count());
    EXPECT_EQ(0, pool_->IdleSocketCount());
    EXPECT_EQ(0, pool_->NumConnectJobsInGroup("foo"));

    // Dropping out of scope will close all handles and return them to idle.
  }

  EXPECT_EQ(1, pool_->IdleSocketCount());
}

TEST_F(ClientSocketPoolBaseTest, WaitForStalledSocketAtSocketLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  ClientSocketHandle stalled_handle;
  TestOldCompletionCallback callback;
  {
    EXPECT_FALSE(pool_->IsStalled());
    ClientSocketHandle handles[kDefaultMaxSockets];
    for (int i = 0; i < kDefaultMaxSockets; ++i) {
      TestOldCompletionCallback callback;
      EXPECT_EQ(OK, handles[i].Init(base::StringPrintf(
          "Take 2: %d", i),
          params_,
          kDefaultPriority,
          &callback,
          pool_.get(),
          BoundNetLog()));
    }

    EXPECT_EQ(kDefaultMaxSockets, client_socket_factory_.allocation_count());
    EXPECT_EQ(0, pool_->IdleSocketCount());
    EXPECT_FALSE(pool_->IsStalled());

    // Now we will hit the socket limit.
    EXPECT_EQ(ERR_IO_PENDING, stalled_handle.Init("foo",
                                                  params_,
                                                  kDefaultPriority,
                                                  &callback,
                                                  pool_.get(),
                                                  BoundNetLog()));
    EXPECT_TRUE(pool_->IsStalled());

    // Dropping out of scope will close all handles and return them to idle.
  }

  // But if we wait for it, the released idle sockets will be closed in
  // preference of the waiting request.
  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_EQ(kDefaultMaxSockets + 1, client_socket_factory_.allocation_count());
  EXPECT_EQ(3, pool_->IdleSocketCount());
}

// Regression test for http://crbug.com/40952.
TEST_F(ClientSocketPoolBaseTest, CloseIdleSocketAtSocketLimitDeleteGroup) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  pool_->EnableConnectBackupJobs();
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  for (int i = 0; i < kDefaultMaxSockets; ++i) {
    ClientSocketHandle handle;
    TestOldCompletionCallback callback;
    EXPECT_EQ(OK, handle.Init(base::IntToString(i),
                              params_,
                              kDefaultPriority,
                              &callback,
                              pool_.get(),
                              BoundNetLog()));
  }

  // Flush all the DoReleaseSocket tasks.
  MessageLoop::current()->RunAllPending();

  // Stall a group.  Set a pending job so it'll trigger a backup job if we don't
  // reuse a socket.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;

  // "0" is special here, since it should be the first entry in the sorted map,
  // which is the one which we would close an idle socket for.  We shouldn't
  // close an idle socket though, since we should reuse the idle socket.
  EXPECT_EQ(OK, handle.Init("0",
                            params_,
                            kDefaultPriority,
                            &callback,
                            pool_.get(),
                            BoundNetLog()));

  EXPECT_EQ(kDefaultMaxSockets, client_socket_factory_.allocation_count());
  EXPECT_EQ(kDefaultMaxSockets - 1, pool_->IdleSocketCount());
}

TEST_F(ClientSocketPoolBaseTest, PendingRequests) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", IDLE));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(kDefaultMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_size() - kDefaultMaxSocketsPerGroup,
            completion_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(8, GetOrderOfRequest(3));
  EXPECT_EQ(6, GetOrderOfRequest(4));
  EXPECT_EQ(4, GetOrderOfRequest(5));
  EXPECT_EQ(3, GetOrderOfRequest(6));
  EXPECT_EQ(5, GetOrderOfRequest(7));
  EXPECT_EQ(7, GetOrderOfRequest(8));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(9));
}

TEST_F(ClientSocketPoolBaseTest, PendingRequests_NoKeepAlive) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  for (size_t i = kDefaultMaxSocketsPerGroup; i < requests_size(); ++i)
    EXPECT_EQ(OK, request(i)->WaitForResult());

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_size() - kDefaultMaxSocketsPerGroup,
            completion_count());
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending connect job will be cancelled and should not call back into
// ClientSocketPoolBase.
TEST_F(ClientSocketPoolBaseTest, CancelRequestClearGroup) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, ConnectCancelConnect) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;

  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));

  handle.Reset();

  TestOldCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback2,
                        pool_.get(),
                        BoundNetLog()));

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, CancelRequest) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));

  // Cancel a request.
  size_t index_to_cancel = kDefaultMaxSocketsPerGroup + 2;
  EXPECT_FALSE((*requests())[index_to_cancel]->handle()->is_initialized());
  (*requests())[index_to_cancel]->handle()->Reset();

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(kDefaultMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_size() - kDefaultMaxSocketsPerGroup - 1,
            completion_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(5, GetOrderOfRequest(3));
  EXPECT_EQ(3, GetOrderOfRequest(4));
  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound,
            GetOrderOfRequest(5));  // Canceled request.
  EXPECT_EQ(4, GetOrderOfRequest(6));
  EXPECT_EQ(6, GetOrderOfRequest(7));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(8));
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

      // Don't allow reuse of the socket.  Disconnect it and then release it and
      // run through the MessageLoop once to get it completely released.
      handle_->socket()->Disconnect();
      handle_->Reset();
      {
        MessageLoop::ScopedNestableTaskAllower nestable(
            MessageLoop::current());
        MessageLoop::current()->RunAllPending();
      }
      within_callback_ = true;
      TestOldCompletionCallback next_job_callback;
      scoped_refptr<TestSocketParams> params(new TestSocketParams());
      int rv = handle_->Init("a",
                             params,
                             kDefaultPriority,
                             &next_job_callback,
                             pool_,
                             BoundNetLog());
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
          {
            MessageLoop::ScopedNestableTaskAllower nestable(
                MessageLoop::current());
            base::PlatformThread::Sleep(10);
            EXPECT_EQ(OK, next_job_callback.WaitForResult());
          }
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
  TestClientSocketPool* const pool_;
  bool within_callback_;
  TestConnectJobFactory* const test_connect_job_factory_;
  TestConnectJob::JobType next_job_type_;
  TestOldCompletionCallback callback_;
};

TEST_F(ClientSocketPoolBaseTest, RequestPendingJobTwice) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  RequestSocketCallback callback(
      &handle, pool_.get(), connect_job_factory_,
      TestConnectJob::kMockPendingJob);
  int rv = handle.Init("a",
                       params_,
                       kDefaultPriority,
                       &callback,
                       pool_.get(),
                       BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest, RequestPendingJobThenSynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  RequestSocketCallback callback(
      &handle, pool_.get(), connect_job_factory_, TestConnectJob::kMockJob);
  int rv = handle.Init("a",
                       params_,
                       kDefaultPriority,
                       &callback,
                       pool_.get(),
                       BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
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
    ASSERT_FALSE(request(i)->handle()->is_initialized());
    request(i)->handle()->Reset();
  }

  // Let's wait for the rest to complete now.
  for (size_t i = kDefaultMaxSocketsPerGroup; i < requests_size(); ++i) {
    EXPECT_EQ(OK, request(i)->WaitForResult());
    request(i)->handle()->Reset();
  }

  EXPECT_EQ(requests_size() - kDefaultMaxSocketsPerGroup,
            completion_count());
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
    EXPECT_EQ(ERR_CONNECTION_FAILED, request(i)->WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest, CancelActiveRequestThenRequestSocket) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  int rv = handle.Init("a",
                       params_,
                       kDefaultPriority,
                       &callback,
                       pool_.get(),
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel the active request.
  handle.Reset();

  rv = handle.Init("a",
                   params_,
                   kDefaultPriority,
                   &callback,
                   pool_.get(),
                   BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());

  EXPECT_FALSE(handle.is_reused());
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// Regression test for http://crbug.com/17985.
TEST_F(ClientSocketPoolBaseTest, GroupWithPendingRequestsIsNotEmpty) {
  const int kMaxSockets = 3;
  const int kMaxSocketsPerGroup = 2;
  CreatePool(kMaxSockets, kMaxSocketsPerGroup);

  const RequestPriority kHighPriority = HIGHEST;

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // This is going to be a pending request in an otherwise empty group.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  // Reach the maximum socket limit.
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));

  // Create a stalled group with high priorities.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kHighPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("c", kHighPriority));

  // Release the first two sockets from "a".  Because this is a keepalive,
  // the first release will unblock the pending request for "a".  The
  // second release will unblock a request for "c", becaue it is the next
  // high priority socket.
  EXPECT_TRUE(ReleaseOneConnection(ClientSocketPoolTest::KEEP_ALIVE));
  EXPECT_TRUE(ReleaseOneConnection(ClientSocketPoolTest::KEEP_ALIVE));

  // Closing idle sockets should not get us into trouble, but in the bug
  // we were hitting a CHECK here.
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
  pool_->CloseIdleSockets();

  MessageLoop::current()->RunAllPending();  // Run the released socket wakeups
}

TEST_F(ClientSocketPoolBaseTest, BasicAsynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  CapturingBoundNetLog log(CapturingNetLog::kUnbounded);
  int rv = handle.Init("a",
                       params_,
                       LOWEST,
                       &callback,
                       pool_.get(),
                       log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &handle));
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  handle.Reset();

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(4u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKET_POOL));
  EXPECT_TRUE(LogContainsEvent(
      entries, 1, NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEvent(
      entries, 2, NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
      NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 3, NetLog::TYPE_SOCKET_POOL));
}

TEST_F(ClientSocketPoolBaseTest,
       InitConnectionAsynchronousFailure) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  CapturingBoundNetLog log(CapturingNetLog::kUnbounded);
  // Set the additional error state members to ensure that they get cleared.
  handle.set_is_ssl_error(true);
  HttpResponseInfo info;
  info.headers = new HttpResponseHeaders("");
  handle.set_ssl_error_response_info(info);
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        log.bound()));
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &handle));
  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_ssl_error());
  EXPECT_TRUE(handle.ssl_error_response_info().headers.get() == NULL);

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);

  EXPECT_EQ(3u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(
      entries, 0, NetLog::TYPE_SOCKET_POOL));
  EXPECT_TRUE(LogContainsEvent(
      entries, 1, NetLog::TYPE_SOCKET_POOL_BOUND_TO_CONNECT_JOB,
      NetLog::PHASE_NONE));
  EXPECT_TRUE(LogContainsEndEvent(
      entries, 2, NetLog::TYPE_SOCKET_POOL));
}

TEST_F(ClientSocketPoolBaseTest, TwoRequestsCancelOne) {
  // TODO(eroman): Add back the log expectations! Removed them because the
  //               ordering is difficult, and some may fire during destructor.
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;

  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback,
                        pool_.get(),
                        BoundNetLog()));
  CapturingBoundNetLog log2(CapturingNetLog::kUnbounded);
  EXPECT_EQ(ERR_IO_PENDING,
            handle2.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback2,
                         pool_.get(),
                         BoundNetLog()));

  handle.Reset();


  // At this point, request 2 is just waiting for the connect job to finish.

  EXPECT_EQ(OK, callback2.WaitForResult());
  handle2.Reset();

  // Now request 2 has actually finished.
  // TODO(eroman): Add back log expectations.
}

TEST_F(ClientSocketPoolBaseTest, CancelRequestLimitsJobs) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));

  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->NumConnectJobsInGroup("a"));
  (*requests())[2]->handle()->Reset();
  (*requests())[3]->handle()->Reset();
  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->NumConnectJobsInGroup("a"));

  (*requests())[1]->handle()->Reset();
  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->NumConnectJobsInGroup("a"));

  (*requests())[0]->handle()->Reset();
  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->NumConnectJobsInGroup("a"));
}

// When requests and ConnectJobs are not coupled, the request will get serviced
// by whatever comes first.
TEST_F(ClientSocketPoolBaseTest, ReleaseSockets) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  // Start job 1 (async OK)
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  std::vector<TestSocketRequest*> request_order;
  size_t completion_count;  // unused
  TestSocketRequest req1(&request_order, &completion_count);
  int rv = req1.handle()->Init("a",
                               params_,
                               kDefaultPriority,
                               &req1, pool_.get(),
                               BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req1.WaitForResult());

  // Job 1 finished OK.  Start job 2 (also async OK).  Request 3 is pending
  // without a job.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);

  TestSocketRequest req2(&request_order, &completion_count);
  rv = req2.handle()->Init("a",
                           params_,
                           kDefaultPriority,
                           &req2,
                           pool_.get(),
                           BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  TestSocketRequest req3(&request_order, &completion_count);
  rv = req3.handle()->Init("a",
                           params_,
                           kDefaultPriority,
                           &req3,
                           pool_.get(),
                           BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Both Requests 2 and 3 are pending.  We release socket 1 which should
  // service request 2.  Request 3 should still be waiting.
  req1.handle()->Reset();
  MessageLoop::current()->RunAllPending();  // Run the released socket wakeups
  ASSERT_TRUE(req2.handle()->socket());
  EXPECT_EQ(OK, req2.WaitForResult());
  EXPECT_FALSE(req3.handle()->socket());

  // Signal job 2, which should service request 3.

  client_socket_factory_.SignalJobs();
  EXPECT_EQ(OK, req3.WaitForResult());

  ASSERT_EQ(3U, request_order.size());
  EXPECT_EQ(&req1, request_order[0]);
  EXPECT_EQ(&req2, request_order[1]);
  EXPECT_EQ(&req3, request_order[2]);
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
}

// The requests are not coupled to the jobs.  So, the requests should finish in
// their priority / insertion order.
TEST_F(ClientSocketPoolBaseTest, PendingJobCompletionOrder) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  // First two jobs are async.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  std::vector<TestSocketRequest*> request_order;
  size_t completion_count;  // unused
  TestSocketRequest req1(&request_order, &completion_count);
  int rv = req1.handle()->Init("a",
                               params_,
                               kDefaultPriority,
                               &req1,
                               pool_.get(),
                               BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestSocketRequest req2(&request_order, &completion_count);
  rv = req2.handle()->Init("a",
                           params_,
                           kDefaultPriority,
                           &req2,
                           pool_.get(),
                           BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // The pending job is sync.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  TestSocketRequest req3(&request_order, &completion_count);
  rv = req3.handle()->Init("a",
                           params_,
                           kDefaultPriority,
                           &req3,
                           pool_.get(),
                           BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(ERR_CONNECTION_FAILED, req1.WaitForResult());
  EXPECT_EQ(OK, req2.WaitForResult());
  EXPECT_EQ(ERR_CONNECTION_FAILED, req3.WaitForResult());

  ASSERT_EQ(3U, request_order.size());
  EXPECT_EQ(&req1, request_order[0]);
  EXPECT_EQ(&req2, request_order[1]);
  EXPECT_EQ(&req3, request_order[2]);
}

TEST_F(ClientSocketPoolBaseTest, LoadState) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(
      TestConnectJob::kMockAdvancingLoadStateJob);

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  int rv = handle.Init("a",
                       params_,
                       kDefaultPriority,
                       &callback,
                       pool_.get(),
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_IDLE, handle.GetLoadState());

  MessageLoop::current()->RunAllPending();

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  rv = handle2.Init("a",
                    params_,
                    kDefaultPriority,
                    &callback2, pool_.get(),
                    BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_NE(LOAD_STATE_IDLE, handle.GetLoadState());
  EXPECT_NE(LOAD_STATE_IDLE, handle2.GetLoadState());
}

TEST_F(ClientSocketPoolBaseTest, Recoverable) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockRecoverableJob);

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, handle.Init("a",
                                                  params_,
                                                  kDefaultPriority,
                                                  &callback, pool_.get(),
                                                  BoundNetLog()));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(ClientSocketPoolBaseTest, AsyncRecoverable) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(
      TestConnectJob::kMockPendingRecoverableJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback,
                        pool_.get(),
                        BoundNetLog()));
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &handle));
  EXPECT_EQ(ERR_PROXY_AUTH_REQUESTED, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(ClientSocketPoolBaseTest, AdditionalErrorStateSynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(
      TestConnectJob::kMockAdditionalErrorStateJob);

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            handle.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback,
                        pool_.get(),
                        BoundNetLog()));
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_TRUE(handle.is_ssl_error());
  EXPECT_FALSE(handle.ssl_error_response_info().headers.get() == NULL);
}

TEST_F(ClientSocketPoolBaseTest, AdditionalErrorStateAsynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(
      TestConnectJob::kMockPendingAdditionalErrorStateJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback,
                        pool_.get(),
                        BoundNetLog()));
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &handle));
  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_TRUE(handle.is_ssl_error());
  EXPECT_FALSE(handle.ssl_error_response_info().headers.get() == NULL);
}

TEST_F(ClientSocketPoolBaseTest, DisableCleanupTimer) {
  // Disable cleanup timer.
  internal::ClientSocketPoolBaseHelper::set_cleanup_timer_enabled(false);

  CreatePoolWithIdleTimeouts(
      kDefaultMaxSockets, kDefaultMaxSocketsPerGroup,
      base::TimeDelta::FromMilliseconds(10),  // Time out unused sockets
      base::TimeDelta::FromMilliseconds(10));  // Time out used sockets

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  // Startup two mock pending connect jobs, which will sit in the MessageLoop.

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  int rv = handle.Init("a",
                       params_,
                       LOWEST,
                       &callback,
                       pool_.get(),
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &handle));

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  rv = handle2.Init("a",
                    params_,
                    LOWEST,
                    &callback2,
                    pool_.get(),
                    BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &handle2));

  // Cancel one of the requests.  Wait for the other, which will get the first
  // job.  Release the socket.  Run the loop again to make sure the second
  // socket is sitting idle and the first one is released (since ReleaseSocket()
  // just posts a DoReleaseSocket() task).

  handle.Reset();
  EXPECT_EQ(OK, callback2.WaitForResult());
  // Use the socket.
  EXPECT_EQ(1, handle2.socket()->Write(NULL, 1, NULL));
  handle2.Reset();

  // The idle socket timeout value was set to 10 milliseconds. Wait 20
  // milliseconds so the sockets timeout.
  base::PlatformThread::Sleep(100);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(2, pool_->IdleSocketCount());

  // Request a new socket. This should cleanup the unused and timed out ones.
  // A new socket will be created rather than reusing the idle one.
  CapturingBoundNetLog log(CapturingNetLog::kUnbounded);
  rv = handle.Init("a",
                   params_,
                   LOWEST,
                   &callback,
                   pool_.get(),
                   log.bound());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_FALSE(handle.is_reused());

  // Make sure the idle socket is closed
  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
  EXPECT_EQ(1, pool_->NumActiveSocketsInGroup("a"));

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_FALSE(LogContainsEntryWithType(
      entries, 1, NetLog::TYPE_SOCKET_POOL_REUSED_AN_EXISTING_SOCKET));
}

TEST_F(ClientSocketPoolBaseTest, CleanupTimedOutIdleSockets) {
  CreatePoolWithIdleTimeouts(
      kDefaultMaxSockets, kDefaultMaxSocketsPerGroup,
      base::TimeDelta(),  // Time out unused sockets immediately.
      base::TimeDelta::FromDays(1));  // Don't time out used sockets.

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  // Startup two mock pending connect jobs, which will sit in the MessageLoop.

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  int rv = handle.Init("a",
                       params_,
                       LOWEST,
                       &callback,
                       pool_.get(),
                       BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &handle));

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  rv = handle2.Init("a",
                    params_,
                    LOWEST,
                    &callback2,
                    pool_.get(),
                    BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &handle2));

  // Cancel one of the requests.  Wait for the other, which will get the first
  // job.  Release the socket.  Run the loop again to make sure the second
  // socket is sitting idle and the first one is released (since ReleaseSocket()
  // just posts a DoReleaseSocket() task).

  handle.Reset();
  EXPECT_EQ(OK, callback2.WaitForResult());
  // Use the socket.
  EXPECT_EQ(1, handle2.socket()->Write(NULL, 1, NULL));
  handle2.Reset();

  // We post all of our delayed tasks with a 2ms delay. I.e. they don't
  // actually become pending until 2ms after they have been created. In order
  // to flush all tasks, we need to wait so that we know there are no
  // soon-to-be-pending tasks waiting.
  base::PlatformThread::Sleep(10);
  MessageLoop::current()->RunAllPending();

  ASSERT_EQ(2, pool_->IdleSocketCount());

  // Invoke the idle socket cleanup check.  Only one socket should be left, the
  // used socket.  Request it to make sure that it's used.

  pool_->CleanupTimedOutIdleSockets();
  CapturingBoundNetLog log(CapturingNetLog::kUnbounded);
  rv = handle.Init("a",
                   params_,
                   LOWEST,
                   &callback,
                   pool_.get(),
                   log.bound());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_reused());

  net::CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsEntryWithType(
      entries, 1, NetLog::TYPE_SOCKET_POOL_REUSED_AN_EXISTING_SOCKET));
}

// Make sure that we process all pending requests even when we're stalling
// because of multiple releasing disconnected sockets.
TEST_F(ClientSocketPoolBaseTest, MultipleReleasingDisconnectedSockets) {
  CreatePoolWithIdleTimeouts(
      kDefaultMaxSockets, kDefaultMaxSocketsPerGroup,
      base::TimeDelta(),  // Time out unused sockets immediately.
      base::TimeDelta::FromDays(1));  // Don't time out used sockets.

  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  // Startup 4 connect jobs.  Two of them will be pending.

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  int rv = handle.Init("a",
                       params_,
                       LOWEST,
                       &callback,
                       pool_.get(),
                       BoundNetLog());
  EXPECT_EQ(OK, rv);

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  rv = handle2.Init("a",
                    params_,
                    LOWEST,
                    &callback2,
                    pool_.get(),
                    BoundNetLog());
  EXPECT_EQ(OK, rv);

  ClientSocketHandle handle3;
  TestOldCompletionCallback callback3;
  rv = handle3.Init("a",
                    params_,
                    LOWEST,
                    &callback3,
                    pool_.get(),
                    BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  ClientSocketHandle handle4;
  TestOldCompletionCallback callback4;
  rv = handle4.Init("a",
                    params_,
                    LOWEST,
                    &callback4,
                    pool_.get(),
                    BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Release two disconnected sockets.

  handle.socket()->Disconnect();
  handle.Reset();
  handle2.socket()->Disconnect();
  handle2.Reset();

  EXPECT_EQ(OK, callback3.WaitForResult());
  EXPECT_FALSE(handle3.is_reused());
  EXPECT_EQ(OK, callback4.WaitForResult());
  EXPECT_FALSE(handle4.is_reused());
}

// Regression test for http://crbug.com/42267.
// When DoReleaseSocket() is processed for one socket, it is blocked because the
// other stalled groups all have releasing sockets, so no progress can be made.
TEST_F(ClientSocketPoolBaseTest, SocketLimitReleasingSockets) {
  CreatePoolWithIdleTimeouts(
      4 /* socket limit */, 4 /* socket limit per group */,
      base::TimeDelta(),  // Time out unused sockets immediately.
      base::TimeDelta::FromDays(1));  // Don't time out used sockets.

  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  // Max out the socket limit with 2 per group.

  ClientSocketHandle handle_a[4];
  TestOldCompletionCallback callback_a[4];
  ClientSocketHandle handle_b[4];
  TestOldCompletionCallback callback_b[4];

  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(OK, handle_a[i].Init("a",
                                   params_,
                                   LOWEST,
                                   &callback_a[i],
                                   pool_.get(),
                                   BoundNetLog()));
    EXPECT_EQ(OK, handle_b[i].Init("b",
                                   params_,
                                   LOWEST,
                                   &callback_b[i],
                                   pool_.get(),
                                   BoundNetLog()));
  }

  // Make 4 pending requests, 2 per group.

  for (int i = 2; i < 4; ++i) {
    EXPECT_EQ(ERR_IO_PENDING,
              handle_a[i].Init("a",
                               params_,
                               LOWEST,
                               &callback_a[i],
                               pool_.get(),
                               BoundNetLog()));
    EXPECT_EQ(ERR_IO_PENDING,
              handle_b[i].Init("b",
                               params_,
                               LOWEST,
                               &callback_b[i],
                               pool_.get(),
                               BoundNetLog()));
  }

  // Release b's socket first.  The order is important, because in
  // DoReleaseSocket(), we'll process b's released socket, and since both b and
  // a are stalled, but 'a' is lower lexicographically, we'll process group 'a'
  // first, which has a releasing socket, so it refuses to start up another
  // ConnectJob.  So, we used to infinite loop on this.
  handle_b[0].socket()->Disconnect();
  handle_b[0].Reset();
  handle_a[0].socket()->Disconnect();
  handle_a[0].Reset();

  // Used to get stuck here.
  MessageLoop::current()->RunAllPending();

  handle_b[1].socket()->Disconnect();
  handle_b[1].Reset();
  handle_a[1].socket()->Disconnect();
  handle_a[1].Reset();

  for (int i = 2; i < 4; ++i) {
    EXPECT_EQ(OK, callback_b[i].WaitForResult());
    EXPECT_EQ(OK, callback_a[i].WaitForResult());
  }
}

TEST_F(ClientSocketPoolBaseTest,
       ReleasingDisconnectedSocketsMaintainsPriorityOrder) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  EXPECT_EQ(OK, (*requests())[0]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[1]->WaitForResult());
  EXPECT_EQ(2u, completion_count());

  // Releases one connection.
  EXPECT_TRUE(ReleaseOneConnection(ClientSocketPoolTest::NO_KEEP_ALIVE));
  EXPECT_EQ(OK, (*requests())[2]->WaitForResult());

  EXPECT_TRUE(ReleaseOneConnection(ClientSocketPoolTest::NO_KEEP_ALIVE));
  EXPECT_EQ(OK, (*requests())[3]->WaitForResult());
  EXPECT_EQ(4u, completion_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(5));
}

class TestReleasingSocketRequest : public CallbackRunner< Tuple1<int> > {
 public:
  TestReleasingSocketRequest(TestClientSocketPool* pool,
                             int expected_result,
                             bool reset_releasing_handle)
      : pool_(pool),
        expected_result_(expected_result),
        reset_releasing_handle_(reset_releasing_handle) {}

  ClientSocketHandle* handle() { return &handle_; }

  int WaitForResult() {
    return callback_.WaitForResult();
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    if (reset_releasing_handle_)
                      handle_.Reset();
    scoped_refptr<TestSocketParams> con_params(new TestSocketParams());
    EXPECT_EQ(expected_result_, handle2_.Init("a",
                                              con_params,
                                              kDefaultPriority,
                                              &callback2_,
                                              pool_,
                                              BoundNetLog()));
  }

 private:
  TestClientSocketPool* const pool_;
  int expected_result_;
  bool reset_releasing_handle_;
  ClientSocketHandle handle_;
  ClientSocketHandle handle2_;
  TestOldCompletionCallback callback_;
  TestOldCompletionCallback callback2_;
};


TEST_F(ClientSocketPoolBaseTest, AdditionalErrorSocketsDontUseSlot) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("b", kDefaultPriority));

  EXPECT_EQ(static_cast<int>(requests_size()),
            client_socket_factory_.allocation_count());

  connect_job_factory_->set_job_type(
      TestConnectJob::kMockPendingAdditionalErrorStateJob);
  TestReleasingSocketRequest req(pool_.get(), OK, false);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle()->Init("a",
                               params_,
                               kDefaultPriority,
                               &req,
                               pool_.get(),
                               BoundNetLog()));
  // The next job should complete synchronously
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  EXPECT_EQ(ERR_CONNECTION_FAILED, req.WaitForResult());
  EXPECT_FALSE(req.handle()->is_initialized());
  EXPECT_FALSE(req.handle()->socket());
  EXPECT_TRUE(req.handle()->is_ssl_error());
  EXPECT_FALSE(req.handle()->ssl_error_response_info().headers.get() == NULL);
}

// http://crbug.com/44724 regression test.
// We start releasing the pool when we flush on network change.  When that
// happens, the only active references are in the ClientSocketHandles.  When a
// ConnectJob completes and calls back into the last ClientSocketHandle, that
// callback can release the last reference and delete the pool.  After the
// callback finishes, we go back to the stack frame within the now-deleted pool.
// Executing any code that refers to members of the now-deleted pool can cause
// crashes.
TEST_F(ClientSocketPoolBaseTest, CallbackThatReleasesPool) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));

  pool_->Flush();

  // We'll call back into this now.
  callback.WaitForResult();
}

TEST_F(ClientSocketPoolBaseTest, DoNotReuseSocketAfterFlush) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_EQ(ClientSocketHandle::UNUSED, handle.reuse_type());

  pool_->Flush();

  handle.Reset();
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_EQ(ClientSocketHandle::UNUSED, handle.reuse_type());
}

class ConnectWithinCallback : public CallbackRunner< Tuple1<int> > {
 public:
  ConnectWithinCallback(
      const std::string& group_name,
      const scoped_refptr<TestSocketParams>& params,
      TestClientSocketPool* pool)
      : group_name_(group_name), params_(params), pool_(pool) {}

  ~ConnectWithinCallback() {}

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    EXPECT_EQ(ERR_IO_PENDING,
              handle_.Init(group_name_,
                           params_,
                           kDefaultPriority,
                           &nested_callback_,
                           pool_,
                           BoundNetLog()));
  }

  int WaitForResult() {
    return callback_.WaitForResult();
  }

  int WaitForNestedResult() {
    return nested_callback_.WaitForResult();
  }

 private:
  const std::string group_name_;
  const scoped_refptr<TestSocketParams> params_;
  TestClientSocketPool* const pool_;
  ClientSocketHandle handle_;
  TestOldCompletionCallback callback_;
  TestOldCompletionCallback nested_callback_;
};

TEST_F(ClientSocketPoolBaseTest, AbortAllRequestsOnFlush) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);

  // First job will be waiting until it gets aborted.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);

  ClientSocketHandle handle;
  ConnectWithinCallback callback("a", params_, pool_.get());
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));

  // Second job will be started during the first callback, and will
  // asynchronously complete with OK.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  pool_->Flush();
  EXPECT_EQ(ERR_ABORTED, callback.WaitForResult());
  EXPECT_EQ(OK, callback.WaitForNestedResult());
}

// Cancel a pending socket request while we're at max sockets,
// and verify that the backup socket firing doesn't cause a crash.
TEST_F(ClientSocketPoolBaseTest, BackupSocketCancelAtMaxSockets) {
  // Max 4 sockets globally, max 4 sockets per group.
  CreatePool(kDefaultMaxSockets, kDefaultMaxSockets);
  pool_->EnableConnectBackupJobs();

  // Create the first socket and set to ERR_IO_PENDING.  This starts the backup
  // timer.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("bar",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));

  // Start (MaxSockets - 1) connected sockets to reach max sockets.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);
  ClientSocketHandle handles[kDefaultMaxSockets];
  for (int i = 1; i < kDefaultMaxSockets; ++i) {
    TestOldCompletionCallback callback;
    EXPECT_EQ(OK, handles[i].Init("bar",
                                  params_,
                                  kDefaultPriority,
                                  &callback,
                                  pool_.get(),
                                  BoundNetLog()));
  }

  MessageLoop::current()->RunAllPending();

  // Cancel the pending request.
  handle.Reset();

  // Wait for the backup timer to fire (add some slop to ensure it fires)
  base::PlatformThread::Sleep(
      ClientSocketPool::kMaxConnectRetryIntervalMs / 2 * 3);

  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(kDefaultMaxSockets, client_socket_factory_.allocation_count());
}

TEST_F(ClientSocketPoolBaseTest, CancelBackupSocketAfterCancelingAllRequests) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSockets);
  pool_->EnableConnectBackupJobs();

  // Create the first socket and set to ERR_IO_PENDING.  This starts the backup
  // timer.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("bar",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));
  ASSERT_TRUE(pool_->HasGroup("bar"));
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("bar"));

  // Cancel the socket request.  This should cancel the backup timer.  Wait for
  // the backup time to see if it indeed got canceled.
  handle.Reset();
  // Wait for the backup timer to fire (add some slop to ensure it fires)
  base::PlatformThread::Sleep(
      ClientSocketPool::kMaxConnectRetryIntervalMs / 2 * 3);
  MessageLoop::current()->RunAllPending();
  ASSERT_TRUE(pool_->HasGroup("bar"));
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("bar"));
}

TEST_F(ClientSocketPoolBaseTest, CancelBackupSocketAfterFinishingAllRequests) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSockets);
  pool_->EnableConnectBackupJobs();

  // Create the first socket and set to ERR_IO_PENDING.  This starts the backup
  // timer.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("bar",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, handle2.Init("bar",
                                         params_,
                                         kDefaultPriority,
                                         &callback2,
                                         pool_.get(),
                                         BoundNetLog()));
  ASSERT_TRUE(pool_->HasGroup("bar"));
  EXPECT_EQ(2, pool_->NumConnectJobsInGroup("bar"));

  // Cancel request 1 and then complete request 2.  With the requests finished,
  // the backup timer should be cancelled.
  handle.Reset();
  EXPECT_EQ(OK, callback2.WaitForResult());
  // Wait for the backup timer to fire (add some slop to ensure it fires)
  base::PlatformThread::Sleep(
      ClientSocketPool::kMaxConnectRetryIntervalMs / 2 * 3);
  MessageLoop::current()->RunAllPending();
}

// Test delayed socket binding for the case where we have two connects,
// and while one is waiting on a connect, the other frees up.
// The socket waiting on a connect should switch immediately to the freed
// up socket.
TEST_F(ClientSocketPoolBaseTest, DelayedSocketBindingWaitingForConnect) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle1.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback,
                         pool_.get(),
                         BoundNetLog()));
  EXPECT_EQ(OK, callback.WaitForResult());

  // No idle sockets, no pending jobs.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));

  // Create a second socket to the same host, but this one will wait.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);
  ClientSocketHandle handle2;
  EXPECT_EQ(ERR_IO_PENDING,
            handle2.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback,
                         pool_.get(),
                         BoundNetLog()));
  // No idle sockets, and one connecting job.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // Return the first handle to the pool.  This will initiate the delayed
  // binding.
  handle1.Reset();

  MessageLoop::current()->RunAllPending();

  // Still no idle sockets, still one pending connect job.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // The second socket connected, even though it was a Waiting Job.
  EXPECT_EQ(OK, callback.WaitForResult());

  // And we can see there is still one job waiting.
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // Finally, signal the waiting Connect.
  client_socket_factory_.SignalJobs();
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));

  MessageLoop::current()->RunAllPending();
}

// Test delayed socket binding when a group is at capacity and one
// of the group's sockets frees up.
TEST_F(ClientSocketPoolBaseTest, DelayedSocketBindingAtGroupCapacity) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle1.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback,
                         pool_.get(),
                         BoundNetLog()));
  EXPECT_EQ(OK, callback.WaitForResult());

  // No idle sockets, no pending jobs.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));

  // Create a second socket to the same host, but this one will wait.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);
  ClientSocketHandle handle2;
  EXPECT_EQ(ERR_IO_PENDING,
            handle2.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback,
                         pool_.get(),
                         BoundNetLog()));
  // No idle sockets, and one connecting job.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // Return the first handle to the pool.  This will initiate the delayed
  // binding.
  handle1.Reset();

  MessageLoop::current()->RunAllPending();

  // Still no idle sockets, still one pending connect job.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // The second socket connected, even though it was a Waiting Job.
  EXPECT_EQ(OK, callback.WaitForResult());

  // And we can see there is still one job waiting.
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // Finally, signal the waiting Connect.
  client_socket_factory_.SignalJobs();
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));

  MessageLoop::current()->RunAllPending();
}

// Test out the case where we have one socket connected, one
// connecting, when the first socket finishes and goes idle.
// Although the second connection is pending, the second request
// should complete, by taking the first socket's idle socket.
TEST_F(ClientSocketPoolBaseTest, DelayedSocketBindingAtStall) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            handle1.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback,
                         pool_.get(),
                         BoundNetLog()));
  EXPECT_EQ(OK, callback.WaitForResult());

  // No idle sockets, no pending jobs.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));

  // Create a second socket to the same host, but this one will wait.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);
  ClientSocketHandle handle2;
  EXPECT_EQ(ERR_IO_PENDING,
            handle2.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback,
                         pool_.get(),
                         BoundNetLog()));
  // No idle sockets, and one connecting job.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // Return the first handle to the pool.  This will initiate the delayed
  // binding.
  handle1.Reset();

  MessageLoop::current()->RunAllPending();

  // Still no idle sockets, still one pending connect job.
  EXPECT_EQ(0, pool_->IdleSocketCount());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // The second socket connected, even though it was a Waiting Job.
  EXPECT_EQ(OK, callback.WaitForResult());

  // And we can see there is still one job waiting.
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // Finally, signal the waiting Connect.
  client_socket_factory_.SignalJobs();
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));

  MessageLoop::current()->RunAllPending();
}

// Cover the case where on an available socket slot, we have one pending
// request that completes synchronously, thereby making the Group empty.
TEST_F(ClientSocketPoolBaseTest, SynchronouslyProcessOnePendingRequest) {
  const int kUnlimitedSockets = 100;
  const int kOneSocketPerGroup = 1;
  CreatePool(kUnlimitedSockets, kOneSocketPerGroup);

  // Make the first request asynchronous fail.
  // This will free up a socket slot later.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING,
            handle1.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback1,
                         pool_.get(),
                         BoundNetLog()));
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  // Make the second request synchronously fail.  This should make the Group
  // empty.
  connect_job_factory_->set_job_type(TestConnectJob::kMockFailingJob);
  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  // It'll be ERR_IO_PENDING now, but the TestConnectJob will synchronously fail
  // when created.
  EXPECT_EQ(ERR_IO_PENDING,
            handle2.Init("a",
                         params_,
                         kDefaultPriority,
                         &callback2,
                         pool_.get(),
                         BoundNetLog()));

  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback1.WaitForResult());
  EXPECT_EQ(ERR_CONNECTION_FAILED, callback2.WaitForResult());
  EXPECT_FALSE(pool_->HasGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, PreferUsedSocketToUnusedSocket) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSockets);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, handle2.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback2,
                                         pool_.get(),
                                         BoundNetLog()));
  ClientSocketHandle handle3;
  TestOldCompletionCallback callback3;
  EXPECT_EQ(ERR_IO_PENDING, handle3.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback3,
                                         pool_.get(),
                                         BoundNetLog()));

  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_EQ(OK, callback3.WaitForResult());

  // Use the socket.
  EXPECT_EQ(1, handle1.socket()->Write(NULL, 1, NULL));
  EXPECT_EQ(1, handle3.socket()->Write(NULL, 1, NULL));

  handle1.Reset();
  handle2.Reset();
  handle3.Reset();

  EXPECT_EQ(OK, handle1.Init("a",
                             params_,
                             kDefaultPriority,
                             &callback1,
                             pool_.get(),
                             BoundNetLog()));
  EXPECT_EQ(OK, handle2.Init("a",
                             params_,
                             kDefaultPriority,
                             &callback2,
                             pool_.get(),
                             BoundNetLog()));
  EXPECT_EQ(OK, handle3.Init("a",
                             params_,
                             kDefaultPriority,
                             &callback3,
                             pool_.get(),
                             BoundNetLog()));

  EXPECT_TRUE(handle1.socket()->WasEverUsed());
  EXPECT_TRUE(handle2.socket()->WasEverUsed());
  EXPECT_FALSE(handle3.socket()->WasEverUsed());
}

TEST_F(ClientSocketPoolBaseTest, RequestSockets) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(2, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, handle2.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback2,
                                         pool_.get(),
                                         BoundNetLog()));

  EXPECT_EQ(2, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_EQ(OK, callback2.WaitForResult());
  handle1.Reset();
  handle2.Reset();

  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(2, pool_->IdleSocketCountInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsWhenAlreadyHaveAConnectJob) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());

  EXPECT_EQ(2, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, handle2.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback2,
                                         pool_.get(),
                                         BoundNetLog()));

  EXPECT_EQ(2, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_EQ(OK, callback2.WaitForResult());
  handle1.Reset();
  handle2.Reset();

  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(2, pool_->IdleSocketCountInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest,
       RequestSocketsWhenAlreadyHaveMultipleConnectJob) {
  CreatePool(4, 4);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, handle2.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback2,
                                         pool_.get(),
                                         BoundNetLog()));

  ClientSocketHandle handle3;
  TestOldCompletionCallback callback3;
  EXPECT_EQ(ERR_IO_PENDING, handle3.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback3,
                                         pool_.get(),
                                         BoundNetLog()));

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(3, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());

  EXPECT_EQ(3, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  EXPECT_EQ(OK, callback1.WaitForResult());
  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_EQ(OK, callback3.WaitForResult());
  handle1.Reset();
  handle2.Reset();
  handle3.Reset();

  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(3, pool_->IdleSocketCountInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsAtMaxSocketLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSockets);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ASSERT_FALSE(pool_->HasGroup("a"));

  pool_->RequestSockets("a", &params_, kDefaultMaxSockets,
                        BoundNetLog());

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(kDefaultMaxSockets, pool_->NumConnectJobsInGroup("a"));

  ASSERT_FALSE(pool_->HasGroup("b"));

  pool_->RequestSockets("b", &params_, kDefaultMaxSockets,
                        BoundNetLog());

  ASSERT_FALSE(pool_->HasGroup("b"));
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsHitMaxSocketLimit) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSockets);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ASSERT_FALSE(pool_->HasGroup("a"));

  pool_->RequestSockets("a", &params_, kDefaultMaxSockets - 1,
                        BoundNetLog());

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(kDefaultMaxSockets - 1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_FALSE(pool_->IsStalled());

  ASSERT_FALSE(pool_->HasGroup("b"));

  pool_->RequestSockets("b", &params_, kDefaultMaxSockets,
                        BoundNetLog());

  ASSERT_TRUE(pool_->HasGroup("b"));
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("b"));
  EXPECT_FALSE(pool_->IsStalled());
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsCountIdleSockets) {
  CreatePool(4, 4);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));
  ASSERT_EQ(OK, callback1.WaitForResult());
  handle1.Reset();

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(1, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());

  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(1, pool_->IdleSocketCountInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsCountActiveSockets) {
  CreatePool(4, 4);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));
  ASSERT_EQ(OK, callback1.WaitForResult());

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
  EXPECT_EQ(1, pool_->NumActiveSocketsInGroup("a"));

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());

  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
  EXPECT_EQ(1, pool_->NumActiveSocketsInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsSynchronous) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  pool_->RequestSockets("a", &params_, kDefaultMaxSocketsPerGroup,
                        BoundNetLog());

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("b", &params_, kDefaultMaxSocketsPerGroup,
                        BoundNetLog());

  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("b"));
  EXPECT_EQ(kDefaultMaxSocketsPerGroup, pool_->IdleSocketCountInGroup("b"));
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsSynchronousError) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockFailingJob);

  pool_->RequestSockets("a", &params_, kDefaultMaxSocketsPerGroup,
                        BoundNetLog());

  ASSERT_FALSE(pool_->HasGroup("a"));

  connect_job_factory_->set_job_type(
      TestConnectJob::kMockAdditionalErrorStateJob);
  pool_->RequestSockets("a", &params_, kDefaultMaxSocketsPerGroup,
                        BoundNetLog());

  ASSERT_FALSE(pool_->HasGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsMultipleTimesDoesNothing) {
  CreatePool(4, 4);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(2, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());
  EXPECT_EQ(2, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));
  ASSERT_EQ(OK, callback1.WaitForResult());

  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  int rv = handle2.Init("a",
                        params_,
                        kDefaultPriority,
                        &callback2,
                        pool_.get(),
                        BoundNetLog());
  if (rv != OK) {
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_EQ(OK, callback2.WaitForResult());
  }

  handle1.Reset();
  handle2.Reset();

  EXPECT_EQ(2, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(2, pool_->IdleSocketCountInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, RequestSocketsDifferentNumSockets) {
  CreatePool(4, 4);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  pool_->RequestSockets("a", &params_, 1, BoundNetLog());

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());
  EXPECT_EQ(2, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("a", &params_, 3, BoundNetLog());
  EXPECT_EQ(3, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  pool_->RequestSockets("a", &params_, 1, BoundNetLog());
  EXPECT_EQ(3, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, PreconnectJobsTakenByNormalRequests) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  pool_->RequestSockets("a", &params_, 1, BoundNetLog());

  ASSERT_TRUE(pool_->HasGroup("a"));
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));

  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  ASSERT_EQ(OK, callback1.WaitForResult());

  handle1.Reset();

  EXPECT_EQ(1, pool_->IdleSocketCountInGroup("a"));
}

// http://crbug.com/64940 regression test.
TEST_F(ClientSocketPoolBaseTest, PreconnectClosesIdleSocketRemovesGroup) {
  const int kMaxTotalSockets = 3;
  const int kMaxSocketsPerGroup = 2;
  CreatePool(kMaxTotalSockets, kMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  // Note that group name ordering matters here.  "a" comes before "b", so
  // CloseOneIdleSocket() will try to close "a"'s idle socket.

  // Set up one idle socket in "a".
  ClientSocketHandle handle1;
  TestOldCompletionCallback callback1;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("a",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));

  ASSERT_EQ(OK, callback1.WaitForResult());
  handle1.Reset();
  EXPECT_EQ(1, pool_->IdleSocketCountInGroup("a"));

  // Set up two active sockets in "b".
  ClientSocketHandle handle2;
  TestOldCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, handle1.Init("b",
                                         params_,
                                         kDefaultPriority,
                                         &callback1,
                                         pool_.get(),
                                         BoundNetLog()));
  EXPECT_EQ(ERR_IO_PENDING, handle2.Init("b",
                                         params_,
                                         kDefaultPriority,
                                         &callback2,
                                         pool_.get(),
                                         BoundNetLog()));

  ASSERT_EQ(OK, callback1.WaitForResult());
  ASSERT_EQ(OK, callback2.WaitForResult());
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("b"));
  EXPECT_EQ(2, pool_->NumActiveSocketsInGroup("b"));

  // Now we have 1 idle socket in "a" and 2 active sockets in "b".  This means
  // we've maxed out on sockets, since we set |kMaxTotalSockets| to 3.
  // Requesting 2 preconnected sockets for "a" should fail to allocate any more
  // sockets for "a", and "b" should still have 2 active sockets.

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(1, pool_->IdleSocketCountInGroup("a"));
  EXPECT_EQ(0, pool_->NumActiveSocketsInGroup("a"));
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("b"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("b"));
  EXPECT_EQ(2, pool_->NumActiveSocketsInGroup("b"));

  // Now release the 2 active sockets for "b".  This will give us 1 idle socket
  // in "a" and 2 idle sockets in "b".  Requesting 2 preconnected sockets for
  // "a" should result in closing 1 for "b".
  handle1.Reset();
  handle2.Reset();
  EXPECT_EQ(2, pool_->IdleSocketCountInGroup("b"));
  EXPECT_EQ(0, pool_->NumActiveSocketsInGroup("b"));

  pool_->RequestSockets("a", &params_, 2, BoundNetLog());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(1, pool_->IdleSocketCountInGroup("a"));
  EXPECT_EQ(0, pool_->NumActiveSocketsInGroup("a"));
  EXPECT_EQ(0, pool_->NumConnectJobsInGroup("b"));
  EXPECT_EQ(1, pool_->IdleSocketCountInGroup("b"));
  EXPECT_EQ(0, pool_->NumActiveSocketsInGroup("b"));
}

TEST_F(ClientSocketPoolBaseTest, PreconnectWithoutBackupJob) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  pool_->EnableConnectBackupJobs();

  // Make the ConnectJob hang until it times out, shorten the timeout.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);
  connect_job_factory_->set_timeout_duration(
      base::TimeDelta::FromMilliseconds(500));
  pool_->RequestSockets("a", &params_, 1, BoundNetLog());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));

  // Verify the backup timer doesn't create a backup job, by making
  // the backup job a pending job instead of a waiting job, so it
  // *would* complete if it were created.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 1000);
  MessageLoop::current()->Run();
  EXPECT_FALSE(pool_->HasGroup("a"));
}

TEST_F(ClientSocketPoolBaseTest, PreconnectWithBackupJob) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  pool_->EnableConnectBackupJobs();

  // Make the ConnectJob hang forever.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);
  pool_->RequestSockets("a", &params_, 1, BoundNetLog());
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
  MessageLoop::current()->RunAllPending();

  // Make the backup job be a pending job, so it completes normally.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a",
                                        params_,
                                        kDefaultPriority,
                                        &callback,
                                        pool_.get(),
                                        BoundNetLog()));
  // Timer has started, but the backup connect job shouldn't be created yet.
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
  EXPECT_EQ(0, pool_->NumActiveSocketsInGroup("a"));
  ASSERT_EQ(OK, callback.WaitForResult());

  // The hung connect job should still be there, but everything else should be
  // complete.
  EXPECT_EQ(1, pool_->NumConnectJobsInGroup("a"));
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
  EXPECT_EQ(1, pool_->NumActiveSocketsInGroup("a"));
}

class MockLayeredPool : public LayeredPool {
 public:
  MockLayeredPool(TestClientSocketPool* pool,
                  const std::string& group_name)
      : pool_(pool),
        params_(new TestSocketParams),
        group_name_(group_name) {
    pool_->AddLayeredPool(this);
  }

  ~MockLayeredPool() {
    pool_->RemoveLayeredPool(this);
  }

  int RequestSocket(TestClientSocketPool* pool) {
    return handle_.Init(group_name_, params_, kDefaultPriority, &callback_,
                        pool, BoundNetLog());
  }

  int RequestSocketWithoutLimits(TestClientSocketPool* pool) {
    params_->set_ignore_limits(true);
    return handle_.Init(group_name_, params_, kDefaultPriority, &callback_,
                        pool, BoundNetLog());
  }

  bool ReleaseOneConnection() {
    if (!handle_.is_initialized()) {
      return false;
    }
    handle_.socket()->Disconnect();
    handle_.Reset();
    return true;
  }

  MOCK_METHOD0(CloseOneIdleConnection, bool());

 private:
  TestClientSocketPool* const pool_;
  scoped_refptr<TestSocketParams> params_;
  ClientSocketHandle handle_;
  TestOldCompletionCallback callback_;
  const std::string group_name_;
};

TEST_F(ClientSocketPoolBaseTest, FailToCloseIdleSocketsNotHeldByLayeredPool) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  MockLayeredPool mock_layered_pool(pool_.get(), "foo");
  EXPECT_CALL(mock_layered_pool, CloseOneIdleConnection())
      .WillOnce(Return(false));
  EXPECT_EQ(OK, mock_layered_pool.RequestSocket(pool_.get()));
  EXPECT_FALSE(pool_->CloseOneIdleConnectionInLayeredPool());
}

TEST_F(ClientSocketPoolBaseTest, ForciblyCloseIdleSocketsHeldByLayeredPool) {
  CreatePool(kDefaultMaxSockets, kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  MockLayeredPool mock_layered_pool(pool_.get(), "foo");
  EXPECT_EQ(OK, mock_layered_pool.RequestSocket(pool_.get()));
  EXPECT_CALL(mock_layered_pool, CloseOneIdleConnection())
      .WillOnce(Invoke(&mock_layered_pool,
                       &MockLayeredPool::ReleaseOneConnection));
  EXPECT_TRUE(pool_->CloseOneIdleConnectionInLayeredPool());
}

TEST_F(ClientSocketPoolBaseTest, CloseIdleSocketsHeldByLayeredPoolWhenNeeded) {
  CreatePool(1, 1);
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  MockLayeredPool mock_layered_pool(pool_.get(), "foo");
  EXPECT_EQ(OK, mock_layered_pool.RequestSocket(pool_.get()));
  EXPECT_CALL(mock_layered_pool, CloseOneIdleConnection())
      .WillOnce(Invoke(&mock_layered_pool,
                       &MockLayeredPool::ReleaseOneConnection));
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(OK, handle.Init("a",
                            params_,
                            kDefaultPriority,
                            &callback,
                            pool_.get(),
                            BoundNetLog()));
}

TEST_F(ClientSocketPoolBaseTest,
       CloseMultipleIdleSocketsHeldByLayeredPoolWhenNeeded) {
  CreatePool(1, 1);
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  MockLayeredPool mock_layered_pool1(pool_.get(), "foo");
  EXPECT_EQ(OK, mock_layered_pool1.RequestSocket(pool_.get()));
  EXPECT_CALL(mock_layered_pool1, CloseOneIdleConnection())
      .Times(1).WillRepeatedly(Invoke(&mock_layered_pool1,
                                      &MockLayeredPool::ReleaseOneConnection));
  MockLayeredPool mock_layered_pool2(pool_.get(), "bar");
  EXPECT_EQ(OK, mock_layered_pool2.RequestSocketWithoutLimits(pool_.get()));
  EXPECT_CALL(mock_layered_pool2, CloseOneIdleConnection())
      .Times(2).WillRepeatedly(Invoke(&mock_layered_pool2,
                                      &MockLayeredPool::ReleaseOneConnection));
  ClientSocketHandle handle;
  TestOldCompletionCallback callback;
  EXPECT_EQ(OK, handle.Init("a",
                            params_,
                            kDefaultPriority,
                            &callback,
                            pool_.get(),
                            BoundNetLog()));
}

}  // namespace

}  // namespace net
