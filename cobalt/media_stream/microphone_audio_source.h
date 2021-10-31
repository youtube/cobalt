// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_STREAM_MICROPHONE_AUDIO_SOURCE_H_
#define COBALT_MEDIA_STREAM_MICROPHONE_AUDIO_SOURCE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "cobalt/media_stream/media_stream_audio_source.h"
#include "cobalt/speech/microphone_manager.h"

namespace cobalt {
namespace media_capture {
FORWARD_DECLARE_TEST(GetUserMediaTest, PendingPromise);
FORWARD_DECLARE_TEST(GetUserMediaTest, MicrophoneStoppedRejectedPromise);
FORWARD_DECLARE_TEST(GetUserMediaTest, MicrophoneErrorRejectedPromise);
FORWARD_DECLARE_TEST(GetUserMediaTest, MicrophoneSuccessFulfilledPromise);
}  // namespace media_capture
namespace media_stream {

class MicrophoneAudioSource : public MediaStreamAudioSource {
 public:
  using CompletionCallback = speech::MicrophoneManager::CompletionCallback;
  using SuccessfulOpenCallback =
      speech::MicrophoneManager::SuccessfulOpenCallback;
  using ErrorCallback = speech::MicrophoneManager::ErrorCallback;

  explicit MicrophoneAudioSource(const speech::Microphone::Options& options,
                                 const SuccessfulOpenCallback& successful_open,
                                 const CompletionCallback& completion,
                                 const ErrorCallback& error);

 private:
  FRIEND_TEST_ALL_PREFIXES(::cobalt::media_capture::GetUserMediaTest,
                           PendingPromise);
  FRIEND_TEST_ALL_PREFIXES(::cobalt::media_capture::GetUserMediaTest,
                           MicrophoneStoppedRejectedPromise);
  FRIEND_TEST_ALL_PREFIXES(::cobalt::media_capture::GetUserMediaTest,
                           MicrophoneErrorRejectedPromise);
  FRIEND_TEST_ALL_PREFIXES(::cobalt::media_capture::GetUserMediaTest,
                           MicrophoneSuccessFulfilledPromise);

  MicrophoneAudioSource(const MicrophoneAudioSource&) = delete;
  MicrophoneAudioSource& operator=(const MicrophoneAudioSource&) = delete;
  ~MicrophoneAudioSource() { EnsureSourceIsStopped(); }

  bool EnsureSourceIsStarted() override;

  void EnsureSourceIsStopped() override;

  // MicrophoneManager callbacks.
  std::unique_ptr<cobalt::speech::Microphone> CreateMicrophone(
      const cobalt::speech::Microphone::Options& options,
      int buffer_size_bytes);

  void OnDataReceived(
      std::unique_ptr<MediaStreamAudioTrack::AudioBus> audio_bus);

  void OnDataCompletion();
  void OnMicrophoneOpen();
  void OnMicrophoneError(speech::MicrophoneManager::MicrophoneError error,
                         std::string error_message);

  scoped_refptr<base::SingleThreadTaskRunner> javascript_thread_task_runner_;

  base::WeakPtrFactory<MicrophoneAudioSource> weak_ptr_factory_;

  // These are passed into |microphone_manager_| below, so they must be
  // defined before it.
  SuccessfulOpenCallback successful_open_callback_;
  CompletionCallback completion_callback_;
  ErrorCallback error_callback_;

  speech::MicrophoneManager microphone_manager_;

  THREAD_CHECKER(thread_checker_);

  friend class base::RefCounted<MicrophoneAudioSource>;
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MICROPHONE_AUDIO_SOURCE_H_
