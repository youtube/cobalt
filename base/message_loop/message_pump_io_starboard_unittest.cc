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
        event_(WaitableEvent::ResetPolicy::AUTOMATIC,
               WaitableEvent::InitialState::NOT_SIGNALED),
        io_thread_("MessagePumpIOStarboardTestIOThread") {}
  ~MessagePumpIOStarboardTest() override = default;

  void SetUp() override {
    Thread::Options options(MessagePumpType::IO, 0);
    ASSERT_TRUE(io_thread_.StartWithOptions(std::move(options)));
    socket_ = SbSocketCreate(SbSocketAddressType::kSbSocketAddressTypeIpv4, SbSocketProtocol::kSbSocketProtocolTcp);
    SbSocketIsValid(socket_);
  }

  void TearDown() override {
    // Some tests watch |pipefds_| from the |io_thread_|. The |io_thread_| must
    // thus be joined to ensure those watches are complete before closing the
    // pipe.
    io_thread_.Stop();

    SbSocketDestroy(socket_);
  }

  std::unique_ptr<MessagePumpIOStarboard> CreateMessagePump() {
    return std::make_unique<MessagePumpIOStarboard>();
  }

  SbSocket socket() {
    return socket_;
  }

  scoped_refptr<SingleThreadTaskRunner> io_runner() const {
    return io_thread_.task_runner();
  }

  void SimulateIOEvent(MessagePumpIOStarboard::SocketWatcher* controller) {
    MessagePumpIOStarboard::OnSocketWaiterNotification(
        nullptr, nullptr, controller,
        (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite));
  }

  std::unique_ptr<test::SingleThreadTaskEnvironment> task_environment_;

  WaitableEvent event_;

 private:
  Thread io_thread_;
  SbSocket socket_;
};

namespace {

// Concrete implementation of MessagePumpIOStarboard::Watcher that does
// nothing useful.
class StupidWatcher : public MessagePumpIOStarboard::Watcher {
 public:
  ~StupidWatcher() override = default;

  // MessagePumpIOStarboard::Watcher interface
  void OnSocketReadyToRead(SbSocket socket) override {}
  void OnSocketReadyToWrite(SbSocket socket) override {}
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
  void OnSocketReadyToRead(SbSocket socket) override { NOTREACHED(); }
  void OnSocketReadyToWrite(SbSocket socket) override { NOTREACHED(); }
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

  void OnSocketReadyToWrite(SbSocket socket) override {
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
              (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite),
              delegate.controller(), &delegate);
  SimulateIOEvent(delegate.controller());
}

class StopWatcher : public BaseWatcher {
 public:
  explicit StopWatcher(MessagePumpIOStarboard::SocketWatcher* controller)
      : controller_(controller) {}

  ~StopWatcher() override = default;

  void OnSocketReadyToWrite(SbSocket socket) override {
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
              (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite),
              &controller, &delegate);
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

  void OnSocketReadyToRead(SbSocket socket) override {
    RunLoop runloop;
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&QuitMessageLoopAndStart, runloop.QuitClosure()));
    runloop.Run();
  }

  void OnSocketReadyToWrite(SbSocket socket) override {}
};

// Fails on some platforms.
TEST_F(MessagePumpIOStarboardTest, DISABLED_NestedPumpWatcher) {
  NestedPumpWatcher delegate;
  std::unique_ptr<MessagePumpIOStarboard> pump = CreateMessagePump();
  MessagePumpIOStarboard::SocketWatcher controller(FROM_HERE);
  pump->Watch(socket(),
              /*persistent=*/false, kSbSocketWaiterInterestRead, &controller,
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

  void OnSocketReadyToRead(SbSocket socket) override {
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
  std::unique_ptr<WaitableEventWatcher> watcher(new WaitableEventWatcher);

  // Tell the pump to watch the pipe.
  pump->Watch(socket(),
              /*persistent=*/false, kSbSocketWaiterInterestRead, &controller,
              &delegate);

  // Make the IO thread wait for |event_| before writing to pipefds[1].
  const char buf = 0;
  WaitableEventWatcher::EventCallback write_socket_task =
      BindOnce(&WriteSocketWrapper, base::Unretained(pump));
  io_runner()->PostTask(
      FROM_HERE, BindOnce(IgnoreResult(&WaitableEventWatcher::StartWatching),
                          Unretained(watcher.get()), &event_,
                          std::move(write_socket_task), io_runner()));

  // Queue task to signal |event_|.
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&event_)));

  // Now run the MessageLoop.
  run_loop.Run();

  // StartWatching can move |watcher| to IO thread. Release on IO thread.
  io_runner()->PostTask(FROM_HERE, BindOnce(&WaitableEventWatcher::StopWatching,
                                            Owned(watcher.release())));
}

}  // namespace

}  // namespace base
