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
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"
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

namespace media {

namespace {

class MockRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() {}
  virtual ~MockRenderCallback() {}

  MOCK_METHOD3(Render, int(const std::vector<float*>& audio_data,
                           int number_of_frames,
                           int audio_delay_milliseconds));
  MOCK_METHOD0(OnRenderError, void());
};

class MockAudioOutputIPC : public AudioOutputIPC {
 public:
  MockAudioOutputIPC() {}
  virtual ~MockAudioOutputIPC() {}

  MOCK_METHOD1(AddDelegate, int(AudioOutputIPCDelegate* delegate));
  MOCK_METHOD1(RemoveDelegate, void(int stream_id));

  MOCK_METHOD2(CreateStream,
      void(int stream_id, const AudioParameters& params));
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

// Zeros out |number_of_frames| in all channel buffers pointed to by
// the |audio_data| vector.
void ZeroAudioData(int number_of_frames,
                   const std::vector<float*>& audio_data) {
  std::vector<float*>::const_iterator it = audio_data.begin();
  for (; it != audio_data.end(); ++it) {
    float* channel = *it;
    for (int j = 0; j < number_of_frames; ++j) {
      channel[j] = 0.0f;
    }
  }
}

}  // namespace.

class AudioOutputDeviceTest : public testing::Test {
 public:
  AudioOutputDeviceTest()
      : default_audio_parameters_(AudioParameters::AUDIO_PCM_LINEAR,
                                  CHANNEL_LAYOUT_STEREO,
                                  48000, 16, 1024),
        stream_id_(-1) {
  }

  ~AudioOutputDeviceTest() {}

  AudioOutputDevice* CreateAudioDevice() {
    return new AudioOutputDevice(
        &audio_output_ipc_, io_loop_.message_loop_proxy());
  }

  void set_stream_id(int stream_id) { stream_id_ = stream_id; }

 protected:
  // Used to clean up TLS pointers that the test(s) will initialize.
  // Must remain the first member of this class.
  base::ShadowingAtExitManager at_exit_manager_;
  MessageLoopForIO io_loop_;
  const AudioParameters default_audio_parameters_;
  MockRenderCallback callback_;
  MockAudioOutputIPC audio_output_ipc_;
  int stream_id_;
};

// The simplest test for AudioOutputDevice.  Used to test construction of
// AudioOutputDevice and that the runtime environment is set up correctly.
TEST_F(AudioOutputDeviceTest, Initialize) {
  scoped_refptr<AudioOutputDevice> audio_device(CreateAudioDevice());
  audio_device->Initialize(default_audio_parameters_, &callback_);
  io_loop_.RunAllPending();
}

// Calls Start() followed by an immediate Stop() and check for the basic message
// filter messages being sent in that case.
TEST_F(AudioOutputDeviceTest, StartStop) {
  scoped_refptr<AudioOutputDevice> audio_device(CreateAudioDevice());
  audio_device->Initialize(default_audio_parameters_, &callback_);

  EXPECT_CALL(audio_output_ipc_, AddDelegate(audio_device.get()))
      .WillOnce(Return(1));
  EXPECT_CALL(audio_output_ipc_, RemoveDelegate(1)).WillOnce(Return());

  audio_device->Start();
  audio_device->Stop();

  EXPECT_CALL(audio_output_ipc_, CreateStream(_, _));
  EXPECT_CALL(audio_output_ipc_, CloseStream(_));

  io_loop_.RunAllPending();
}

// Starts an audio stream, creates a shared memory section + SyncSocket pair
// that AudioOutputDevice must use for audio data.  It then sends a request for
// a single audio packet and quits when the packet has been sent.
TEST_F(AudioOutputDeviceTest, CreateStream) {
  scoped_refptr<AudioOutputDevice> audio_device(CreateAudioDevice());
  audio_device->Initialize(default_audio_parameters_, &callback_);

  EXPECT_CALL(audio_output_ipc_, AddDelegate(audio_device.get()))
      .WillOnce(Return(1));
  EXPECT_CALL(audio_output_ipc_, RemoveDelegate(1)).WillOnce(Return());

  audio_device->Start();

  EXPECT_CALL(audio_output_ipc_, CreateStream(_, _))
      .WillOnce(WithArgs<0>(
          Invoke(this, &AudioOutputDeviceTest::set_stream_id)));


  EXPECT_EQ(stream_id_, -1);
  io_loop_.RunAllPending();

  // OnCreateStream() must have been called and we should have a valid
  // stream id.
  ASSERT_NE(stream_id_, -1);

  // This is where it gets a bit hacky.  The shared memory contract between
  // AudioOutputDevice and its browser side counter part includes a bit more
  // than just the audio data, so we must call TotalSharedMemorySizeInBytes()
  // to get the actual size needed to fit the audio data plus the extra data.
  int memory_size = TotalSharedMemorySizeInBytes(
      default_audio_parameters_.GetBytesPerBuffer());
  SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAndMapAnonymous(memory_size));
  memset(shared_memory.memory(), 0xff, memory_size);

  CancelableSyncSocket browser_socket, renderer_socket;
  ASSERT_TRUE(CancelableSyncSocket::CreatePair(&browser_socket,
                                               &renderer_socket));

  // Create duplicates of the handles we pass to AudioOutputDevice since
  // ownership will be transferred and AudioOutputDevice is responsible for
  // freeing.
  SyncSocket::Handle audio_device_socket = SyncSocket::kInvalidHandle;
  ASSERT_TRUE(DuplicateSocketHandle(renderer_socket.handle(),
                                    &audio_device_socket));
  base::SharedMemoryHandle duplicated_memory_handle;
  ASSERT_TRUE(shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                           &duplicated_memory_handle));

  // We should get a 'play' notification when we call OnStreamCreated().
  // Respond by asking for some audio data.  This should ask our callback
  // to provide some audio data that AudioOutputDevice then writes into the
  // shared memory section.
  EXPECT_CALL(audio_output_ipc_, PlayStream(stream_id_))
      .WillOnce(SendPendingBytes(&browser_socket, memory_size));

  // We expect calls to our audio renderer callback, which returns the number
  // of frames written to the memory section.
  // Here's the second place where it gets hacky:  There's no way for us to
  // know (without using a sleep loop!) when the AudioOutputDevice has finished
  // writing the interleaved audio data into the shared memory section.
  // So, for the sake of this test, we consider the call to Render a sign
  // of success and quit the loop.

  // A note on the call to ZeroAudioData():
  // Valgrind caught a bug in AudioOutputDevice::AudioThreadCallback::Process()
  // whereby we always interleaved all the frames in the buffer regardless
  // of how many were actually rendered.  So to keep the benefits of that
  // test, we explicitly pass 0 in here as the number of frames to
  // ZeroAudioData().  Other tests might want to pass the requested number
  // by using WithArgs<1, 0>(Invoke(&ZeroAudioData)) and set the return
  // value accordingly.
  const int kNumberOfFramesToProcess = 0;

  EXPECT_CALL(callback_, Render(_, _, _))
      .WillOnce(DoAll(
          WithArgs<0>(Invoke(
              testing::CreateFunctor(&ZeroAudioData,
                  kNumberOfFramesToProcess))),
          QuitLoop(io_loop_.message_loop_proxy()),
          Return(kNumberOfFramesToProcess)));

  audio_device->OnStreamCreated(duplicated_memory_handle, audio_device_socket,
                                memory_size);

  io_loop_.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(),
                           TestTimeouts::action_timeout());
  io_loop_.Run();

  // Close the stream sequence.
  EXPECT_CALL(audio_output_ipc_, CloseStream(stream_id_));

  audio_device->Stop();
  io_loop_.RunAllPending();
}

}  // namespace media.
