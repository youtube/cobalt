// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TEST_UTIL_H_
#define CHROME_BROWSER_DICTATION_TEST_UTIL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;
namespace extensions {
class Extension;
}

namespace dictation {

class ListenerStreamProvider;

inline constexpr std::string_view kDictationTestExtensionId =
    "dfihfgggpgemecjdjahibncmmjlfjggp";

// Loads an extension that provides an implementation of the connector
// extension. The test extension has hooks that tests can use to control the
// behavior of the extension.
const extensions::Extension* LoadTestExtension(Profile* profile);

// Same as above but puts the test extension into "manual" mode which prevents
// it from starting the speech API or responding to any events. Tests using this
// will manually simulate API calls from the extension using the send methods
// below.
const extensions::Extension* LoadTestExtensionInManualMode(Profile* profile);

// Sets a canned string that the test extension will replay when the test
// starts a stream.
void SetMockTranscript(Profile* profile, const std::string& transcript);

// Simulates the connector extension sending a transcript update. Returns
// immediately, use WaitForTranscriptUpdate below to await the change in the
// browser process.
void ExtensionSendTranscriptUpdate(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id,
    extensions::api::dictation_private::TranscriptionType type,
    std::string_view data);

// Simulates the connector extension sending a stream state update. Returns
// immediately, use WaitForStreamState to await the change in the browser
// process.
void ExtensionSendStreamStateUpdate(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id,
    extensions::api::dictation_private::StreamState state);

// Waits for the given stream provider to enter the specified state.
void WaitForStreamState(ListenerStreamProvider* provider,
                        StreamProvider::StreamState state);

// Waits for the given stream provider to receive any transcript update.
void WaitForTranscriptUpdate(ListenerStreamProvider* provider);

class MockStreamProvider : public StreamProvider {
 public:
  MockStreamProvider();
  ~MockStreamProvider() override;

  MOCK_METHOD(void,
              BindToTargetAndConnect,
              (std::unique_ptr<Target> target),
              (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void,
              OnTranscriptionUpdated,
              (const std::string& data, bool is_final),
              (override));
  MOCK_METHOD(void, OnStreamStateChanged, (StreamState state), (override));
  MOCK_METHOD(StreamState, GetState, (), (const, override));
};

class MockSessionUi : public SessionUi {
 public:
  MockSessionUi();
  ~MockSessionUi() override;
};

class MockSessionControllerDelegate : public SessionControllerDelegate {
 public:
  MockSessionControllerDelegate();
  ~MockSessionControllerDelegate() override;

  MOCK_METHOD(std::unique_ptr<StreamProvider>,
              CreateStreamProvider,
              (SessionController & controller),
              (const, override));
  MOCK_METHOD(std::unique_ptr<SessionUi>,
              CreateUi,
              (SessionController & controller),
              (const, override));
  MOCK_METHOD(void, EndSession, (), (override));
};

class MockTarget : public Target {
 public:
  MockTarget();
  ~MockTarget() override;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TEST_UTIL_H_
