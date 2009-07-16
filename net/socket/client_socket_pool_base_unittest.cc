// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/scoped_vector.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kDefaultMaxSocketsPerGroup = 2;

const int kDefaultPriority = 5;

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

  // ClientSocket methods:

  virtual int Connect(CompletionCallback* callback) {
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

class TestSocketRequest : public CallbackRunner< Tuple1<int> > {
 public:
  TestSocketRequest(
      ClientSocketPool* pool,
      std::vector<TestSocketRequest*>* request_order)
      : handle(pool), request_order_(request_order) {}

  ClientSocketHandle handle;

  int WaitForResult() {
    return callback_.WaitForResult();
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    completion_count++;
    request_order_->push_back(this);
  }

  static size_t completion_count;

 private:
  std::vector<TestSocketRequest*>* request_order_;
  TestCompletionCallback callback_;
};

size_t TestSocketRequest::completion_count = 0;

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
                 const ClientSocketPoolBase::Request& request,
                 ConnectJob::Delegate* delegate,
                 MockClientSocketFactory* client_socket_factory)
      : ConnectJob(group_name, request.handle, delegate),
        job_type_(job_type),
        client_socket_factory_(client_socket_factory),
        method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  // ConnectJob methods:

  virtual int Connect() {
    AddressList ignored;
    client_socket_factory_->CreateTCPClientSocket(ignored);
    switch (job_type_) {
      case kMockJob:
        return DoConnect(true /* successful */, false /* sync */);
      case kMockFailingJob:
        return DoConnect(false /* error */, false /* sync */);
      case kMockPendingJob:
        set_load_state(LOAD_STATE_CONNECTING);
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
               &TestConnectJob::DoConnect,
               true /* successful */,
               true /* async */));
        return ERR_IO_PENDING;
      case kMockPendingFailingJob:
        set_load_state(LOAD_STATE_CONNECTING);
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
               &TestConnectJob::DoConnect,
               false /* error */,
               true  /* async */));
        return ERR_IO_PENDING;
      case kMockWaitingJob:
        client_socket_factory_->WaitForSignal(this);
        waiting_success_ = true;
        return ERR_IO_PENDING;
      case kMockAdvancingLoadStateJob:
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
                &TestConnectJob::AdvanceLoadState, load_state()));
        return ERR_IO_PENDING;
      default:
        NOTREACHED();
        return ERR_FAILED;
    }
  }

  void Signal() {
    DoConnect(waiting_success_, true /* async */);
  }

 private:
  int DoConnect(bool succeed, bool was_async) {
    int result = ERR_CONNECTION_FAILED;
    if (succeed) {
      result = OK;
      set_socket(new MockClientSocket());
      socket()->Connect(NULL);
    }

    if (was_async)
      delegate()->OnConnectJobComplete(result, this);
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

  DISALLOW_COPY_AND_ASSIGN(TestConnectJob);
};

class TestConnectJobFactory : public ClientSocketPoolBase::ConnectJobFactory {
 public:
  explicit TestConnectJobFactory(MockClientSocketFactory* client_socket_factory)
      : job_type_(TestConnectJob::kMockJob),
        client_socket_factory_(client_socket_factory) {}

  virtual ~TestConnectJobFactory() {}

  void set_job_type(TestConnectJob::JobType job_type) { job_type_ = job_type; }

  // ConnectJobFactory methods:

  virtual ConnectJob* NewConnectJob(
      const std::string& group_name,
      const ClientSocketPoolBase::Request& request,
      ConnectJob::Delegate* delegate) const {
    return new TestConnectJob(job_type_,
                              group_name,
                              request,
                              delegate,
                              client_socket_factory_);
  }

 private:
  TestConnectJob::JobType job_type_;
  MockClientSocketFactory* const client_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectJobFactory);
};

class TestClientSocketPool : public ClientSocketPool {
 public:
  TestClientSocketPool(
      int max_sockets_per_group,
      ClientSocketPoolBase::ConnectJobFactory* connect_job_factory)
      : base_(new ClientSocketPoolBase(
          max_sockets_per_group, connect_job_factory)) {}

  virtual int RequestSocket(
      const std::string& group_name,
      const HostResolver::RequestInfo& resolve_info,
      int priority,
      ClientSocketHandle* handle,
      CompletionCallback* callback) {
    return base_->RequestSocket(
        group_name, resolve_info, priority, handle, callback);
  }

  virtual void CancelRequest(
      const std::string& group_name,
      const ClientSocketHandle* handle) {
    base_->CancelRequest(group_name, handle);
  }

  virtual void ReleaseSocket(
      const std::string& group_name,
      ClientSocket* socket) {
    base_->ReleaseSocket(group_name, socket);
  }

  virtual void CloseIdleSockets() {
    base_->CloseIdleSockets();
  }

  virtual int IdleSocketCount() const { return base_->idle_socket_count(); }

  virtual int IdleSocketCountInGroup(const std::string& group_name) const {
    return base_->IdleSocketCountInGroup(group_name);
  }

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const {
    return base_->GetLoadState(group_name, handle);
  }

  int MaxSocketsPerGroup() const {
    return base_->max_sockets_per_group();
  }

 private:
  const scoped_refptr<ClientSocketPoolBase> base_;

  DISALLOW_COPY_AND_ASSIGN(TestClientSocketPool);
};

void MockClientSocketFactory::SignalJobs() {
  for (std::vector<TestConnectJob*>::iterator it = waiting_jobs_.begin();
       it != waiting_jobs_.end(); ++it) {
    (*it)->Signal();
  }
  waiting_jobs_.clear();
}

class ClientSocketPoolBaseTest : public testing::Test {
 protected:
  ClientSocketPoolBaseTest()
      : ignored_request_info_("ignored", 80),
        connect_job_factory_(
            new TestConnectJobFactory(&client_socket_factory_)) {}

  void CreatePool(int max_sockets_per_group) {
    DCHECK(!pool_.get());
    pool_ = new TestClientSocketPool(max_sockets_per_group,
                                     connect_job_factory_);
  }

  virtual void SetUp() {
    TestSocketRequest::completion_count = 0;
  }

  virtual void TearDown() {
    // The tests often call Reset() on handles at the end which may post
    // DoReleaseSocket() tasks.
    MessageLoop::current()->RunAllPending();
    // Need to delete |pool_| before we turn late binding back off.
    // TODO(willchan): Remove this line when late binding becomes the default.
    pool_ = NULL;
    ClientSocketPoolBase::EnableLateBindingOfSockets(false);
  }

  int StartRequest(const std::string& group_name, int priority) {
    DCHECK(pool_.get());
    TestSocketRequest* request = new TestSocketRequest(pool_.get(),
                                                       &request_order_);
    requests_.push_back(request);
    int rv = request->handle.Init(group_name, ignored_request_info_, priority,
                                  request);
    if (rv != ERR_IO_PENDING)
      request_order_.push_back(request);
    return rv;
  }

  static const int kIndexOutOfBounds;
  static const int kRequestNotFound;

  int GetOrderOfRequest(size_t index) {
    index--;
    if (index >= requests_.size())
      return kIndexOutOfBounds;

    for (size_t i = 0; i < request_order_.size(); i++)
      if (requests_[index] == request_order_[i])
        return i + 1;

    return kRequestNotFound;
  }

  enum KeepAlive {
    KEEP_ALIVE,
    NO_KEEP_ALIVE,
  };

  void ReleaseAllConnections(KeepAlive keep_alive) {
    bool released_one;
    do {
      released_one = false;
      ScopedVector<TestSocketRequest>::iterator i;
      for (i = requests_.begin(); i != requests_.end(); ++i) {
        if ((*i)->handle.is_initialized()) {
          if (keep_alive == NO_KEEP_ALIVE)
            (*i)->handle.socket()->Disconnect();
          (*i)->handle.Reset();
          MessageLoop::current()->RunAllPending();
          released_one = true;
        }
      }
    } while (released_one);
  }

  HostResolver::RequestInfo ignored_request_info_;
  MockClientSocketFactory client_socket_factory_;
  TestConnectJobFactory* const connect_job_factory_;
  scoped_refptr<TestClientSocketPool> pool_;
  ScopedVector<TestSocketRequest> requests_;
  std::vector<TestSocketRequest*> request_order_;
};

// static
const int ClientSocketPoolBaseTest::kIndexOutOfBounds = -1;

// static
const int ClientSocketPoolBaseTest::kRequestNotFound = -2;

TEST_F(ClientSocketPoolBaseTest, BasicSynchronous) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  TestCompletionCallback callback;
  ClientSocketHandle handle(pool_.get());
  EXPECT_EQ(OK, handle.Init("a", ignored_request_info_, kDefaultPriority,
                            &callback));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, BasicAsynchronous) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  int rv = req.handle.Init("a", ignored_request_info_, 0, &req);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &req.handle));
  EXPECT_EQ(OK, req.WaitForResult());
  EXPECT_TRUE(req.handle.is_initialized());
  EXPECT_TRUE(req.handle.socket());
  req.handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, InitConnectionFailure) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockFailingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
}

TEST_F(ClientSocketPoolBaseTest, InitConnectionAsynchronousFailure) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", ignored_request_info_, 5, &req));
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &req.handle));
  EXPECT_EQ(ERR_CONNECTION_FAILED, req.WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest, PendingRequests) {
  CreatePool(kDefaultMaxSocketsPerGroup);

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
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup,
            TestSocketRequest::completion_count);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(6, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(3, GetOrderOfRequest(5));
  EXPECT_EQ(5, GetOrderOfRequest(6));
  EXPECT_EQ(7, GetOrderOfRequest(7));
}

TEST_F(ClientSocketPoolBaseTest, PendingRequests_NoKeepAlive) {
  CreatePool(kDefaultMaxSocketsPerGroup);

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
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup,
            TestSocketRequest::completion_count);
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending connect job will be cancelled and should not call back into
// ClientSocketPoolBase.
TEST_F(ClientSocketPoolBaseTest, CancelRequestClearGroup) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
  req.handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, TwoRequestsCancelOne) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  TestSocketRequest req2(pool_.get(), &request_order_);

  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
  EXPECT_EQ(ERR_IO_PENDING,
            req2.handle.Init("a", ignored_request_info_,
                             kDefaultPriority, &req2));

  req.handle.Reset();

  EXPECT_EQ(OK, req2.WaitForResult());
  req2.handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, ConnectCancelConnect) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  TestCompletionCallback callback;
  TestSocketRequest req(pool_.get(), &request_order_);

  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", ignored_request_info_,
                        kDefaultPriority, &callback));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", ignored_request_info_,
                        kDefaultPriority, &callback2));

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, CancelRequest) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));

  // Cancel a request.
  size_t index_to_cancel = kDefaultMaxSocketsPerGroup + 2;
  EXPECT_FALSE(requests_[index_to_cancel]->handle.is_initialized());
  requests_[index_to_cancel]->handle.Reset();

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(kDefaultMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup - 1,
            TestSocketRequest::completion_count);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(5, GetOrderOfRequest(3));
  EXPECT_EQ(3, GetOrderOfRequest(4));
  EXPECT_EQ(kRequestNotFound, GetOrderOfRequest(5));  // Canceled request.
  EXPECT_EQ(4, GetOrderOfRequest(6));
  EXPECT_EQ(6, GetOrderOfRequest(7));
}

class RequestSocketCallback : public CallbackRunner< Tuple1<int> > {
 public:
  RequestSocketCallback(ClientSocketHandle* handle,
                        TestConnectJobFactory* test_connect_job_factory,
                        TestConnectJob::JobType next_job_type)
      : handle_(handle),
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
      int rv = handle_->Init(
          "a", HostResolver::RequestInfo("www.google.com", 80),
          kDefaultPriority, this);
      switch (next_job_type_) {
        case TestConnectJob::kMockJob:
          EXPECT_EQ(OK, rv);
          break;
        case TestConnectJob::kMockPendingJob:
          EXPECT_EQ(ERR_IO_PENDING, rv);
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
  bool within_callback_;
  TestConnectJobFactory* const test_connect_job_factory_;
  TestConnectJob::JobType next_job_type_;
  TestCompletionCallback callback_;
};

TEST_F(ClientSocketPoolBaseTest, RequestPendingJobTwice) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  RequestSocketCallback callback(
      &handle, connect_job_factory_, TestConnectJob::kMockPendingJob);
  int rv = handle.Init(
      "a", ignored_request_info_, kDefaultPriority, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, RequestPendingJobThenSynchronous) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  RequestSocketCallback callback(
      &handle, connect_job_factory_, TestConnectJob::kMockJob);
  int rv = handle.Init(
      "a", ignored_request_info_, kDefaultPriority, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(ClientSocketPoolBaseTest, CancelActiveRequestWithPendingRequests) {
  CreatePool(kDefaultMaxSocketsPerGroup);

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
    ASSERT_FALSE(requests_[i]->handle.is_initialized());
    requests_[i]->handle.Reset();
  }

  // Let's wait for the rest to complete now.
  for (size_t i = kDefaultMaxSocketsPerGroup; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult());
    requests_[i]->handle.Reset();
  }

  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup,
            TestSocketRequest::completion_count);
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(ClientSocketPoolBaseTest, FailingActiveRequestWithPendingRequests) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  scoped_ptr<TestSocketRequest> reqs[kDefaultMaxSocketsPerGroup * 2 + 1];

  // Queue up all the requests
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));
    int rv = reqs[i]->handle.Init("a", ignored_request_info_,
                                  kDefaultPriority, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  for (size_t i = 0; i < arraysize(reqs); ++i)
    EXPECT_EQ(ERR_CONNECTION_FAILED, reqs[i]->WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest, CancelActiveRequestThenRequestSocket) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  TestSocketRequest req(pool_.get(), &request_order_);
  int rv = req.handle.Init(
      "a", ignored_request_info_, kDefaultPriority, &req);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel the active request.
  req.handle.Reset();

  rv = req.handle.Init("a", ignored_request_info_, kDefaultPriority, &req);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req.WaitForResult());

  EXPECT_FALSE(req.handle.is_reused());
  EXPECT_EQ(1U, TestSocketRequest::completion_count);
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// A pending asynchronous job completes, which will free up a socket slot.  The
// next job finishes synchronously.  The callback for the asynchronous job
// should be first though.
TEST_F(ClientSocketPoolBaseTest, PendingJobCompletionOrder) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  // First two jobs are async.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  // Start job 1 (async error).
  TestSocketRequest req1(pool_.get(), &request_order_);
  int rv = req1.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Start job 2 (async error).
  TestSocketRequest req2(pool_.get(), &request_order_);
  rv = req2.handle.Init("a", ignored_request_info_, kDefaultPriority, &req2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // The pending job is sync.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  // Request 3 does not have a ConnectJob yet.  It's just pending.
  TestSocketRequest req3(pool_.get(), &request_order_);
  rv = req3.handle.Init("a", ignored_request_info_, kDefaultPriority, &req3);
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
  CreatePool(kDefaultMaxSocketsPerGroup);
  ClientSocketPoolBase::EnableLateBindingOfSockets(false);

  // Start job 1 (async OK)
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  TestSocketRequest req1(pool_.get(), &request_order_);
  int rv = req1.handle.Init("a", ignored_request_info_, 5, &req1);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req1.WaitForResult());

  // Job 1 finished OK.  Start job 2 (also async OK).  Release socket 1.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);

  TestSocketRequest req2(pool_.get(), &request_order_);
  rv = req2.handle.Init("a", ignored_request_info_, 5, &req2);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  req1.handle.Reset();
  MessageLoop::current()->RunAllPending();  // Run the DoReleaseSocket()

  // Job 2 is pending. Start request 3 (which has no associated job since it
  // will use the idle socket).

  TestSocketRequest req3(pool_.get(), &request_order_);
  rv = req3.handle.Init("a", ignored_request_info_, 5, &req3);
  EXPECT_EQ(OK, rv);

  EXPECT_FALSE(req2.handle.socket());
  client_socket_factory_.SignalJobs();
  EXPECT_EQ(OK, req2.WaitForResult());

  ASSERT_EQ(2U, request_order_.size());
  EXPECT_EQ(&req1, request_order_[0]);
  EXPECT_EQ(&req2, request_order_[1]);
  EXPECT_EQ(0, pool_->IdleSocketCountInGroup("a"));
}

class ClientSocketPoolBaseTest_LateBinding : public ClientSocketPoolBaseTest {
 protected:
  virtual void SetUp() {
    ClientSocketPoolBaseTest::SetUp();
    ClientSocketPoolBase::EnableLateBindingOfSockets(true);
  }
};

TEST_F(ClientSocketPoolBaseTest_LateBinding, BasicSynchronous) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  TestCompletionCallback callback;
  ClientSocketHandle handle(pool_.get());
  EXPECT_EQ(OK, handle.Init("a", ignored_request_info_, kDefaultPriority,
                            &callback));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, BasicAsynchronous) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  int rv = req.handle.Init("a", ignored_request_info_, 0, &req);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &req.handle));
  EXPECT_EQ(OK, req.WaitForResult());
  EXPECT_TRUE(req.handle.is_initialized());
  EXPECT_TRUE(req.handle.socket());
  req.handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, InitConnectionFailure) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockFailingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
}

TEST_F(ClientSocketPoolBaseTest_LateBinding,
       InitConnectionAsynchronousFailure) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", ignored_request_info_, 5, &req));
  EXPECT_EQ(LOAD_STATE_CONNECTING, pool_->GetLoadState("a", &req.handle));
  EXPECT_EQ(ERR_CONNECTION_FAILED, req.WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, PendingRequests) {
  CreatePool(kDefaultMaxSocketsPerGroup);

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
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup,
            TestSocketRequest::completion_count);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(6, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(3, GetOrderOfRequest(5));
  EXPECT_EQ(5, GetOrderOfRequest(6));
  EXPECT_EQ(7, GetOrderOfRequest(7));
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, PendingRequests_NoKeepAlive) {
  CreatePool(kDefaultMaxSocketsPerGroup);

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
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup,
            TestSocketRequest::completion_count);
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending connect job will be cancelled and should not call back into
// ClientSocketPoolBase.
TEST_F(ClientSocketPoolBaseTest_LateBinding, CancelRequestClearGroup) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
  req.handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, TwoRequestsCancelOne) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  TestSocketRequest req2(pool_.get(), &request_order_);

  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
  EXPECT_EQ(ERR_IO_PENDING,
            req2.handle.Init("a", ignored_request_info_,
                             kDefaultPriority, &req2));

  req.handle.Reset();

  EXPECT_EQ(OK, req2.WaitForResult());
  req2.handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, ConnectCancelConnect) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  TestCompletionCallback callback;
  TestSocketRequest req(pool_.get(), &request_order_);

  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", ignored_request_info_,
                        kDefaultPriority, &callback));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", ignored_request_info_,
                        kDefaultPriority, &callback2));

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, CancelRequest) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 3));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 4));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 2));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", 1));

  // Cancel a request.
  size_t index_to_cancel = kDefaultMaxSocketsPerGroup + 2;
  EXPECT_FALSE(requests_[index_to_cancel]->handle.is_initialized());
  requests_[index_to_cancel]->handle.Reset();

  ReleaseAllConnections(KEEP_ALIVE);

  EXPECT_EQ(kDefaultMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup - 1,
            TestSocketRequest::completion_count);

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(5, GetOrderOfRequest(3));
  EXPECT_EQ(3, GetOrderOfRequest(4));
  EXPECT_EQ(kRequestNotFound, GetOrderOfRequest(5));  // Canceled request.
  EXPECT_EQ(4, GetOrderOfRequest(6));
  EXPECT_EQ(6, GetOrderOfRequest(7));
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, RequestPendingJobTwice) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  RequestSocketCallback callback(
      &handle, connect_job_factory_, TestConnectJob::kMockPendingJob);
  int rv = handle.Init(
      "a", ignored_request_info_, kDefaultPriority, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest_LateBinding, RequestPendingJobThenSynchronous) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  RequestSocketCallback callback(
      &handle, connect_job_factory_, TestConnectJob::kMockJob);
  int rv = handle.Init(
      "a", ignored_request_info_, kDefaultPriority, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(ClientSocketPoolBaseTest_LateBinding,
       CancelActiveRequestWithPendingRequests) {
  CreatePool(kDefaultMaxSocketsPerGroup);

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
    ASSERT_FALSE(requests_[i]->handle.is_initialized());
    requests_[i]->handle.Reset();
  }

  // Let's wait for the rest to complete now.
  for (size_t i = kDefaultMaxSocketsPerGroup; i < requests_.size(); ++i) {
    EXPECT_EQ(OK, requests_[i]->WaitForResult());
    requests_[i]->handle.Reset();
  }

  EXPECT_EQ(requests_.size() - kDefaultMaxSocketsPerGroup,
            TestSocketRequest::completion_count);
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(ClientSocketPoolBaseTest_LateBinding,
       FailingActiveRequestWithPendingRequests) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  scoped_ptr<TestSocketRequest> reqs[kDefaultMaxSocketsPerGroup * 2 + 1];

  // Queue up all the requests
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));
    int rv = reqs[i]->handle.Init("a", ignored_request_info_,
                                  kDefaultPriority, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  for (size_t i = 0; i < arraysize(reqs); ++i)
    EXPECT_EQ(ERR_CONNECTION_FAILED, reqs[i]->WaitForResult());
}

TEST_F(ClientSocketPoolBaseTest_LateBinding,
       CancelActiveRequestThenRequestSocket) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  TestSocketRequest req(pool_.get(), &request_order_);
  int rv = req.handle.Init(
      "a", ignored_request_info_, kDefaultPriority, &req);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Cancel the active request.
  req.handle.Reset();

  rv = req.handle.Init("a", ignored_request_info_, kDefaultPriority, &req);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req.WaitForResult());

  EXPECT_FALSE(req.handle.is_reused());
  EXPECT_EQ(1U, TestSocketRequest::completion_count);
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// When requests and ConnectJobs are not coupled, the request will get serviced
// by whatever comes first.
TEST_F(ClientSocketPoolBaseTest_LateBinding, ReleaseSockets) {
  CreatePool(kDefaultMaxSocketsPerGroup);

  // Start job 1 (async OK)
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  TestSocketRequest req1(pool_.get(), &request_order_);
  int rv = req1.handle.Init("a", ignored_request_info_, 5, &req1);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, req1.WaitForResult());

  // Job 1 finished OK.  Start job 2 (also async OK).  Request 3 is pending
  // without a job.
  connect_job_factory_->set_job_type(TestConnectJob::kMockWaitingJob);

  TestSocketRequest req2(pool_.get(), &request_order_);
  rv = req2.handle.Init("a", ignored_request_info_, 5, &req2);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  TestSocketRequest req3(pool_.get(), &request_order_);
  rv = req3.handle.Init("a", ignored_request_info_, 5, &req3);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Both Requests 2 and 3 are pending.  We release socket 1 which should
  // service request 2.  Request 3 should still be waiting.
  req1.handle.Reset();
  MessageLoop::current()->RunAllPending();  // Run the DoReleaseSocket()
  ASSERT_TRUE(req2.handle.socket());
  EXPECT_EQ(OK, req2.WaitForResult());
  EXPECT_FALSE(req3.handle.socket());

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
  CreatePool(kDefaultMaxSocketsPerGroup);
  // First two jobs are async.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  TestSocketRequest req1(pool_.get(), &request_order_);
  int rv = req1.handle.Init("a", ignored_request_info_, 5, &req1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  TestSocketRequest req2(pool_.get(), &request_order_);
  rv = req2.handle.Init("a", ignored_request_info_, 5, &req2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // The pending job is sync.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  TestSocketRequest req3(pool_.get(), &request_order_);
  rv = req3.handle.Init("a", ignored_request_info_, 5, &req3);
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
  CreatePool(kDefaultMaxSocketsPerGroup);
  connect_job_factory_->set_job_type(
      TestConnectJob::kMockAdvancingLoadStateJob);

  TestSocketRequest req1(pool_.get(), &request_order_);
  int rv = req1.handle.Init("a", ignored_request_info_, 5, &req1);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_IDLE, req1.handle.GetLoadState());

  MessageLoop::current()->RunAllPending();

  TestSocketRequest req2(pool_.get(), &request_order_);
  rv = req2.handle.Init("a", ignored_request_info_, 5, &req2);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, req1.handle.GetLoadState());
  EXPECT_EQ(LOAD_STATE_WAITING_FOR_CACHE, req2.handle.GetLoadState());
}

}  // namespace

}  // namespace net
