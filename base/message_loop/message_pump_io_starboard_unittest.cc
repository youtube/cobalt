// Copyright 2024 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/message_loop/message_pump_io_starboard.h"

#include <unistd.h>

#include <memory>

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#endif

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class MessagePumpIOStarboardTest : public testing::Test {
 protected:
  MessagePumpIOStarboardTest()
      : task_environment_(std::make_unique<test::SingleThreadTaskEnvironment>(
            test::SingleThreadTaskEnvironment::MainThreadType::DEFAULT)),
        io_thread_("MessagePumpIOStarboardTestIOThread") {}
  ~MessagePumpIOStarboardTest() override = default;

  void SetUp() override {
    Thread::Options options(MessagePumpType::IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(std::move(options)));
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
    socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    socket_ = SbSocketCreate(SbSocketAddressType::kSbSocketAddressTypeIpv4, SbSocketProtocol::kSbSocketProtocolTcp);
    SbSocketIsValid(socket_);
#endif
  }

  void TearDown() override {
    // Some tests watch |pipefds_| from the |io_thread_|. The |io_thread_| must
    // thus be joined to ensure those watches are complete before closing the
    // pipe.
    io_thread_.Stop();

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
    close(socket_);
#else
    SbSocketDestroy(socket_);
#endif
  }

  std::unique_ptr<MessagePumpIOStarboard> CreateMessagePump() {
    return std::make_unique<MessagePumpIOStarboard>();
  }

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  int socket() {
    return socket_;
  }
#else
  SbSocket socket() {
    return socket_;
  }
#endif

  scoped_refptr<SingleThreadTaskRunner> io_runner() const {
    return io_thread_.task_runner();
  }

  void SimulateIOEvent(MessagePumpIOStarboard::SocketWatcher* controller) {
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
    MessagePumpIOStarboard::OnPosixSocketWaiterNotification(nullptr,
                                                            -1,
                                                            controller,
                                                            (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite));

#else

    MessagePumpIOStarboard::OnSocketWaiterNotification(nullptr,
                                                       nullptr,
                                                       controller,
                                                       (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite));
#endif
  }

  std::unique_ptr<test::SingleThreadTaskEnvironment> task_environment_;

 private:
  Thread io_thread_;
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  int socket_;
#else
  SbSocket socket_;
#endif
};

namespace {

// Concrete implementation of MessagePumpIOStarboard::Watcher that does
// nothing useful.
class StupidWatcher : public MessagePumpIOStarboard::Watcher {
 public:
  ~StupidWatcher() override = default;

  // MessagePumpIOStarboard::Watcher interface
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  void OnSocketReadyToRead(int socket) override {}
  void OnSocketReadyToWrite(int socket) override {}
#else
  void OnSocketReadyToRead(SbSocket socket) override {}
  void OnSocketReadyToWrite(SbSocket socket) override {}
#endif
};

// Death tests not supported.
TEST_F(MessagePumpIOStarboardTest, DISABLED_QuitOutsideOfRun) {
  std::unique_ptr<MessagePumpIOStarboard> pump = CreateMessagePump();
  ASSERT_DCHECK_DEATH(pump->Quit());
}

class BaseWatcher : public MessagePumpIOStarboard::Watcher {
 public:
  BaseWatcher() = default;
  ~BaseWatcher() override = default;

  // MessagePumpIOStarboard::Watcher interface
#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  void OnSocketReadyToRead(int socket) override { NOTREACHED(); }
  void OnSocketReadyToWrite(int socket) override { NOTREACHED(); }
#else
  void OnSocketReadyToRead(SbSocket socket) override { NOTREACHED(); }
  void OnSocketReadyToWrite(SbSocket socket) override { NOTREACHED(); }
#endif
};

class DeleteWatcher : public BaseWatcher {
 public:
  explicit DeleteWatcher(
      std::unique_ptr<MessagePumpIOStarboard::SocketWatcher> controller)
      : controller_(std::move(controller)) {}

  ~DeleteWatcher() override { DCHECK(!controller_); }

  MessagePumpIOStarboard::SocketWatcher* controller() {
    return controller_.get();
  }

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  void OnSocketReadyToWrite(int socket) override {
#else
  void OnSocketReadyToWrite(SbSocket socket) override {
#endif
    DCHECK(controller_);
    controller_.reset();
  }

 private:
  std::unique_ptr<MessagePumpIOStarboard::SocketWatcher> controller_;
};

// Fails on some platforms.
TEST_F(MessagePumpIOStarboardTest, DISABLED_DeleteWatcher) {
  DeleteWatcher delegate(
      std::make_unique<MessagePumpIOStarboard::SocketWatcher>(FROM_HERE));
  std::unique_ptr<MessagePumpIOStarboard> pump = CreateMessagePump();
  pump->Watch(socket(),
      /*persistent=*/false,
     MessagePumpIOStarboard::WATCH_READ_WRITE,
     delegate.controller(),
     &delegate);
  SimulateIOEvent(delegate.controller());
}

class StopWatcher : public BaseWatcher {
 public:
  explicit StopWatcher(MessagePumpIOStarboard::SocketWatcher* controller)
      : controller_(controller) {}

  ~StopWatcher() override = default;

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  void OnSocketReadyToWrite(int socket) override {
#else
  void OnSocketReadyToWrite(SbSocket socket) override {
#endif
    controller_->StopWatchingSocket();
  }

 private:
  raw_ptr<MessagePumpIOStarboard::SocketWatcher> controller_ = nullptr;
};

// Fails on some platforms.
TEST_F(MessagePumpIOStarboardTest, DISABLED_StopWatcher) {
  std::unique_ptr<MessagePumpIOStarboard> pump = CreateMessagePump();
  MessagePumpIOStarboard::SocketWatcher controller(FROM_HERE);
  StopWatcher delegate(&controller);
  pump->Watch(socket(),
      /*persistent=*/false,
     MessagePumpIOStarboard::WATCH_READ_WRITE,
     &controller,
     &delegate);
  SimulateIOEvent(&controller);
}

void QuitMessageLoopAndStart(OnceClosure quit_closure) {
  std::move(quit_closure).Run();

  RunLoop runloop(RunLoop::Type::kNestableTasksAllowed);
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        runloop.QuitClosure());
  runloop.Run();
}

class NestedPumpWatcher : public MessagePumpIOStarboard::Watcher {
 public:
  NestedPumpWatcher() = default;
  ~NestedPumpWatcher() override = default;

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  void OnSocketReadyToRead(int socket) override {
#else
  void OnSocketReadyToRead(SbSocket socket) override {
#endif
    RunLoop runloop;
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&QuitMessageLoopAndStart, runloop.QuitClosure()));
    runloop.Run();
  }

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  void OnSocketReadyToWrite(int socket) override {}
};
#else
  void OnSocketReadyToWrite(SbSocket socket) override {}
};
#endif

// Fails on some platforms.
TEST_F(MessagePumpIOStarboardTest, DISABLED_NestedPumpWatcher) {
  NestedPumpWatcher delegate;
  std::unique_ptr<MessagePumpIOStarboard> pump = CreateMessagePump();
  MessagePumpIOStarboard::SocketWatcher controller(FROM_HERE);
  pump->Watch(socket(),
      /*persistent=*/false,
     MessagePumpIOStarboard::WATCH_READ,
     &controller,
     &delegate);
  SimulateIOEvent(&controller);
}

void FatalClosure() {
  FAIL() << "Reached fatal closure.";
}

class QuitWatcher : public BaseWatcher {
 public:
  QuitWatcher(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

#if SB_API_VERSION >= 16 //--|| SB_IS(MODULAR)
  void OnSocketReadyToRead(int socket) override {
#else
  void OnSocketReadyToRead(SbSocket socket) override {
#endif
    // Post a fatal closure to the MessageLoop before we quit it.
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&FatalClosure));

    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

void WriteSocketWrapper(MessagePumpIOStarboard* pump,
                        WaitableEvent* event) {
  pump->ScheduleWork();
}

// Fails on some platforms.
TEST_F(MessagePumpIOStarboardTest, DISABLED_QuitWatcher) {
  // Delete the old TaskEnvironment so that we can manage our own one here.
  task_environment_.reset();

  std::unique_ptr<MessagePumpIOStarboard> executor_pump = CreateMessagePump();
  MessagePumpIOStarboard* pump = executor_pump.get();
  SingleThreadTaskExecutor executor(std::move(executor_pump));
  RunLoop run_loop;
  QuitWatcher delegate(run_loop.QuitClosure());
  MessagePumpIOStarboard::SocketWatcher controller(FROM_HERE);
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC,
                      WaitableEvent::InitialState::NOT_SIGNALED);
  std::unique_ptr<WaitableEventWatcher> watcher(new WaitableEventWatcher);

  // Tell the pump to watch the pipe.
  pump->Watch(socket(),
        /*persistent=*/false,
       MessagePumpIOStarboard::WATCH_READ,
       &controller,
       &delegate);

  // Make the IO thread wait for |event| before writing to pipefds[1].
  const char buf = 0;
  WaitableEventWatcher::EventCallback write_socket_task =
      BindOnce(&WriteSocketWrapper, base::Unretained(pump));
  io_runner()->PostTask(
      FROM_HERE, BindOnce(IgnoreResult(&WaitableEventWatcher::StartWatching),
                          Unretained(watcher.get()), &event,
                          std::move(write_socket_task), io_runner()));

  // Queue |event| to signal on |sequence_manager|.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&event)));

  // Now run the MessageLoop.
  run_loop.Run();

  // StartWatching can move |watcher| to IO thread. Release on IO thread.
  io_runner()->PostTask(FROM_HERE, BindOnce(&WaitableEventWatcher::StopWatching,
                                            Owned(watcher.release())));
}

}  // namespace

}  // namespace base
