/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "sdk/android/native_api/peerconnection/peer_connection_factory.h"

#include <jni.h>

#include <cstddef>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/enable_media_with_defaults.h"
#include "api/environment/environment.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "media/engine/internal_decoder_factory.h"
#include "media/engine/internal_encoder_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"
#include "sdk/android/generated_native_unittests_jni/PeerConnectionFactoryInitializationHelper_jni.h"
#include "sdk/android/native_api/audio_device_module/audio_device_android.h"
#include "sdk/android/native_api/jni/application_context_provider.h"
#include "sdk/android/native_api/jni/jvm.h"
#include "sdk/android/src/jni/pc/owned_factory_and_threads.h"
#include "test/create_test_environment.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {
namespace {

// Create native peer connection factory, that will be wrapped by java one
webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> CreateTestPCF(
    JNIEnv* jni,
    webrtc::Thread* network_thread,
    webrtc::Thread* worker_thread,
    webrtc::Thread* signaling_thread,
    const Environment& env) {
  // talk/ assumes pretty widely that the current Thread is ThreadManager'd, but
  // ThreadManager only WrapCurrentThread()s the thread where it is first
  // created.  Since the semantics around when auto-wrapping happens in
  // webrtc/rtc_base/ are convoluted, we simply wrap here to avoid having to
  // think about ramifications of auto-wrapping there.
  webrtc::ThreadManager::Instance()->WrapCurrentThread();

  PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.network_thread = network_thread;
  pcf_deps.worker_thread = worker_thread;
  pcf_deps.signaling_thread = signaling_thread;
  pcf_deps.env = env;

  pcf_deps.adm =
      CreateJavaAudioDeviceModule(jni, *pcf_deps.env, GetAppContext(jni).obj());
  pcf_deps.video_encoder_factory =
      std::make_unique<webrtc::InternalEncoderFactory>();
  pcf_deps.video_decoder_factory =
      std::make_unique<webrtc::InternalDecoderFactory>();
  EnableMediaWithDefaults(pcf_deps);

  auto factory = CreateModularPeerConnectionFactory(std::move(pcf_deps));
  RTC_LOG(LS_INFO) << "PeerConnectionFactory created: " << factory.get();
  RTC_CHECK(factory) << "Failed to create the peer connection factory; "
                        "WebRTC/libjingle init likely failed on this device";

  return factory;
}

TEST(PeerConnectionFactoryTest, NativeToJavaPeerConnectionFactory) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();

  RTC_LOG(LS_INFO) << "Initializing java peer connection factory.";
  jni::Java_PeerConnectionFactoryInitializationHelper_initializeFactoryForTests(
      jni);
  RTC_LOG(LS_INFO) << "Java peer connection factory initialized.";

  auto socket_server = std::make_unique<webrtc::PhysicalSocketServer>();

  // Create threads.
  auto network_thread = std::make_unique<webrtc::Thread>(socket_server.get());
  network_thread->SetName("network_thread", nullptr);
  RTC_CHECK(network_thread->Start()) << "Failed to start thread";

  std::unique_ptr<webrtc::Thread> worker_thread = webrtc::Thread::Create();
  worker_thread->SetName("worker_thread", nullptr);
  RTC_CHECK(worker_thread->Start()) << "Failed to start thread";

  std::unique_ptr<webrtc::Thread> signaling_thread = webrtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", NULL);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  const Environment env = CreateTestEnvironment();
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      CreateTestPCF(jni, network_thread.get(), worker_thread.get(),
                    signaling_thread.get(), env);

  jobject java_factory = NativeToJavaPeerConnectionFactory(
      jni, factory, std::move(socket_server), std::move(network_thread),
      std::move(worker_thread), std::move(signaling_thread), env);

  RTC_LOG(LS_INFO) << java_factory;

  EXPECT_NE(java_factory, nullptr);
}

class DestructionNotifierThread : public Thread {
 public:
  DestructionNotifierThread(std::unique_ptr<SocketServer> ss,
                            Event* absl_nonnull on_destroy)
      : Thread(std::move(ss)), on_destroy_(on_destroy) {}
  ~DestructionNotifierThread() override {
    Stop();
    on_destroy_->Set();
  }

 private:
  Event* absl_nonnull const on_destroy_;
};

// Verifies that `OwnedFactoryAndThreads` destroys the signaling thread last.
// If the signaling thread is destroyed before the worker thread, tasks running
// on the worker thread during teardown (e.g. proxy releases) that access the
// signaling thread will trigger a Use-After-Free crash.
// If the destruction order is correct, the worker thread is joined and stopped
// while the signaling thread is still alive, enabling tasks to exit safely.
TEST(PeerConnectionFactoryTest, OwnedFactoryAndThreadsDestructionOrder) {
  JNIEnv* jni = AttachCurrentThreadIfNeeded();
  jni::Java_PeerConnectionFactoryInitializationHelper_initializeFactoryForTests(
      jni);

  Event signaling_thread_destroyed;
  auto socket_server = std::make_unique<PhysicalSocketServer>();

  auto network_thread = std::make_unique<Thread>(socket_server.get());
  network_thread->Start();

  auto worker_thread = Thread::Create();
  worker_thread->Start();

  auto signaling_socket_server = std::make_unique<NullSocketServer>();
  auto signaling_thread = std::make_unique<DestructionNotifierThread>(
      std::move(signaling_socket_server), &signaling_thread_destroyed);
  signaling_thread->Start();

  worker_thread->PostTask([&signaling_thread_destroyed] {
    while (!signaling_thread_destroyed.Wait(TimeDelta::Millis(1))) {
      if (Thread::Current()->IsQuitting()) {
        // Worker thread is stopping while signaling thread is still alive.
        // Correct teardown sequence. Exit safely.
        return;
      }
    }
    // The signaling thread is being destroyed while the worker thread is still
    // running. Attempting to post a task to the deleted signaling thread will
    // trigger a Use-After-Free crash. This block will only be reached if the
    // incorrect member destruction order in OwnedFactoryAndThreads is active.
    FAIL()
        << "Signaling thread destroyed before worker thread finished teardown.";
  });

  const Environment env = CreateTestEnvironment();
  // We don't need a real factory for this test, nullptr is fine.
  {
    jni::OwnedFactoryAndThreads owned_factory(
        std::move(socket_server), std::move(network_thread),
        std::move(worker_thread), std::move(signaling_thread), env, nullptr);
  }
}

}  // namespace
}  // namespace test
}  // namespace webrtc
