// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "base/test/test_timeouts.h"
#include "media/audio/audio_output_device.h"
#include "media/audio/sample_rates.h"
#include "media/audio/shared_memory_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::CancelableSyncSocket;
using base::SharedMemory;
using base::SyncSocket;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::WithArgs;
using testing::StrictMock;

namespace media {

namespace {

class MockRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() {}
  virtual ~MockRenderCallback() {}

  MOCK_METHOD2(Render, int(AudioBus* dest, int audio_delay_milliseconds));
  MOCK_METHOD3(RenderIO, void(AudioBus* source,
                              AudioBus* dest,
                              int audio_delay_milliseconds));
  MOCK_METHOD0(OnRenderError, void());
};

class MockAudioOutputIPC : public AudioOutputIPC {
 public:
  MockAudioOutputIPC() {}
  virtual ~MockAudioOutputIPC() {}

  MOCK_METHOD1(AddDelegate, int(AudioOutputIPCDelegate* delegate));
  MOCK_METHOD1(RemoveDelegate, void(int stream_id));

  MOCK_METHOD3(CreateStream,
      void(int stream_id, const AudioParameters& params, int input_channels));
  MOCK_METHOD1(PlayStream, void(int stream_id));
  MOCK_METHOD1(CloseStream, void(int stream_id));
  MOCK_METHOD2(SetVolume, void(int stream_id, double volume));
  MOCK_METHOD1(PauseStream, void(int stream_id));
  MOCK_METHOD1(FlushStream, void(int stream_id));
};

// Creates a copy of a SyncSocket handle that we can give to AudioOutputDevice.
// On Windows this means duplicating the pipe handle so that AudioOutputDevice
// can call CloseHandle() (since ownership has been transferred), but on other
// platforms, we just copy the same socket handle since AudioOutputDevice on
// those platforms won't actually own the socket (FileDescriptor.auto_close is
// false).
bool DuplicateSocketHandle(SyncSocket::Handle socket_handle,
                           SyncSocket::Handle* copy) {
#if defined(OS_WIN)
  HANDLE process = GetCurrentProcess();
  ::DuplicateHandle(process, socket_handle, process, copy,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
  return *copy != NULL;
#else
  *copy = socket_handle;
  return *copy != -1;
#endif
}

ACTION_P2(SendPendingBytes, socket, pending_bytes) {
  socket->Send(&pending_bytes, sizeof(pending_bytes));
}

// Used to terminate a loop from a different thread than the loop belongs to.
// |loop| should be a MessageLoopProxy.
ACTION_P(QuitLoop, loop) {
  loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

}  // namespace.

class AudioOutputDeviceTest : public testing::Test {
 public:
  AudioOutputDeviceTest()
      : default_audio_parameters_(AudioParameters::AUDIO_PCM_LINEAR,
                                  CHANNEL_LAYOUT_STEREO,
                                  48000, 16, 1024),
        audio_device_(new AudioOutputDevice(
            &audio_output_ipc_, io_loop_.message_loop_proxy())) {
  }

  ~AudioOutputDeviceTest() {}

  int CalculateMemorySize(bool synchronized_io);

  void StartAudioDevice(bool synchronized_io);
  void CreateStream(bool synchronized_io);
  void ExpectRenderCallback(bool synchronized_io);
  void WaitUntilRenderCallback();
  void StopAudioDevice();

 protected:
  // Used to clean up TLS pointers that the test(s) will initialize.
  // Must remain the first member of this class.
  base::ShadowingAtExitManager at_exit_manager_;
  MessageLoopForIO io_loop_;
  const AudioParameters default_audio_parameters_;
  StrictMock<MockRenderCallback> callback_;
  StrictMock<MockAudioOutputIPC> audio_output_ipc_;
  scoped_refptr<AudioOutputDevice> audio_device_;

 private:
  SharedMemory shared_memory_;
  CancelableSyncSocket browser_socket_;
  CancelableSyncSocket renderer_socket_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDeviceTest);
};

static const int kStreamId = 123;

int AudioOutputDeviceTest::CalculateMemorySize(bool synchronized_io) {
  int kInputChannels = synchronized_io ? 2 : 0;

  // Calculate output and input memory size.
  int output_memory_size =
      AudioBus::CalculateMemorySize(default_audio_parameters_);

  int frames = default_audio_parameters_.frames_per_buffer();
  int input_memory_size =
      AudioBus::CalculateMemorySize(kInputChannels, frames);

  int io_buffer_size = output_memory_size + input_memory_size;

  // This is where it gets a bit hacky.  The shared memory contract between
  // AudioOutputDevice and its browser side counter part includes a bit more
  // than just the audio data, so we must call TotalSharedMemorySizeInBytes()
  // to get the actual size needed to fit the audio data plus the extra data.
  return TotalSharedMemorySizeInBytes(io_buffer_size);
}

void AudioOutputDeviceTest::StartAudioDevice(bool synchronized_io) {
  const int kInputChannels = synchronized_io ? 2 : 0;

  if (synchronized_io) {
    // Request output and synchronized input.
    audio_device_->InitializeIO(default_audio_parameters_,
                                kInputChannels,
                                &callback_);
  } else {
    // Output only.
    audio_device_->Initialize(default_audio_parameters_, &callback_);
  }

  audio_device_->Start();

  EXPECT_CALL(audio_output_ipc_, AddDelegate(audio_device_.get()))
      .WillOnce(Return(kStreamId));
  EXPECT_CALL(audio_output_ipc_, CreateStream(kStreamId, _, _));

  io_loop_.RunAllPending();
}

void AudioOutputDeviceTest::CreateStream(bool synchronized_io) {
  const int kMemorySize = CalculateMemorySize(synchronized_io);
  ASSERT_TRUE(shared_memory_.CreateAndMapAnonymous(kMemorySize));
  memset(shared_memory_.memory(), 0xff, kMemorySize);

  ASSERT_TRUE(CancelableSyncSocket::CreatePair(&browser_socket_,
                                               &renderer_socket_));

  // Create duplicates of the handles we pass to AudioOutputDevice since
  // ownership will be transferred and AudioOutputDevice is responsible for
  // freeing.
  SyncSocket::Handle audio_device_socket = SyncSocket::kInvalidHandle;
  ASSERT_TRUE(DuplicateSocketHandle(renderer_socket_.handle(),
                                    &audio_device_socket));
  base::SharedMemoryHandle duplicated_memory_handle;
  ASSERT_TRUE(shared_memory_.ShareToProcess(base::GetCurrentProcessHandle(),
                                            &duplicated_memory_handle));

  audio_device_->OnStreamCreated(duplicated_memory_handle, audio_device_socket,
                                 PacketSizeInBytes(kMemorySize));
  io_loop_.RunAllPending();
}

void AudioOutputDeviceTest::ExpectRenderCallback(bool synchronized_io) {
  // We should get a 'play' notification when we call OnStreamCreated().
  // Respond by asking for some audio data.  This should ask our callback
  // to provide some audio data that AudioOutputDevice then writes into the
  // shared memory section.
  const int kMemorySize = CalculateMemorySize(synchronized_io);
  EXPECT_CALL(audio_output_ipc_, PlayStream(kStreamId))
      .WillOnce(SendPendingBytes(&browser_socket_, kMemorySize));

  // We expect calls to our audio renderer callback, which returns the number
  // of frames written to the memory section.
  // Here's the second place where it gets hacky:  There's no way for us to
  // know (without using a sleep loop!) when the AudioOutputDevice has finished
  // writing the interleaved audio data into the shared memory section.
  // So, for the sake of this test, we consider the call to Render a sign
  // of success and quit the loop.
  if (synchronized_io) {
    // For synchronized I/O, we expect RenderIO().
    EXPECT_CALL(callback_, RenderIO(_, _, _))
        .WillOnce(QuitLoop(io_loop_.message_loop_proxy()));
  } else {
    // For output only we expect Render().
    const int kNumberOfFramesToProcess = 0;
    EXPECT_CALL(callback_, Render(_, _))
        .WillOnce(DoAll(
            QuitLoop(io_loop_.message_loop_proxy()),
            Return(kNumberOfFramesToProcess)));
  }
}

void AudioOutputDeviceTest::WaitUntilRenderCallback() {
  // Don't hang the test if we never get the Render() callback.
  io_loop_.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(),
                           TestTimeouts::action_timeout());
  io_loop_.Run();
}

void AudioOutputDeviceTest::StopAudioDevice() {
  EXPECT_CALL(audio_output_ipc_, CloseStream(kStreamId));
  EXPECT_CALL(audio_output_ipc_, RemoveDelegate(kStreamId));

  audio_device_->Stop();
  io_loop_.RunAllPending();
}

// The simplest test for AudioOutputDevice.  Used to test construction of
// AudioOutputDevice and that the runtime environment is set up correctly.
TEST_F(AudioOutputDeviceTest, Initialize) {
  audio_device_->Initialize(default_audio_parameters_, &callback_);
  io_loop_.RunAllPending();
}

// Similar to Initialize() test, but using synchronized I/O.
TEST_F(AudioOutputDeviceTest, InitializeIO) {
  const int kInputChannels = 2;  // Test stereo synchronized input.
  audio_device_->InitializeIO(default_audio_parameters_,
                              kInputChannels,
                              &callback_);
  io_loop_.RunAllPending();
}

// Calls Start() followed by an immediate Stop() and check for the basic message
// filter messages being sent in that case.
TEST_F(AudioOutputDeviceTest, StartStop) {
  StartAudioDevice(false);
  StopAudioDevice();
}

TEST_F(AudioOutputDeviceTest, DISABLED_StopBeforeRender) {
  StartAudioDevice(false);

  // Call Stop() but don't run the IO loop yet.
  audio_device_->Stop();

  // Expect us to shutdown IPC but not to render anything despite the stream
  // getting created.
  EXPECT_CALL(audio_output_ipc_, CloseStream(kStreamId));
  EXPECT_CALL(audio_output_ipc_, RemoveDelegate(kStreamId));
  CreateStream(false);
}

// Full test with output only.
TEST_F(AudioOutputDeviceTest, CreateStream) {
  StartAudioDevice(false);
  ExpectRenderCallback(false);
  CreateStream(false);
  WaitUntilRenderCallback();
  StopAudioDevice();
}

// Same as CreateStream() test, but also tests synchronized I/O.
TEST_F(AudioOutputDeviceTest, CreateStreamWithInput) {
  StartAudioDevice(true);
  ExpectRenderCallback(true);
  CreateStream(true);
  WaitUntilRenderCallback();
  StopAudioDevice();
}

}  // namespace media.
