// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_SELECTION_TEST_BASE_H_
#define CHROMEOS_ASH_COMPONENTS_AUDIO_AUDIO_DEVICE_SELECTION_TEST_BASE_H_

#include <inttypes.h>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ActiveNodeObserver;

class AudioDeviceSelectionTestBase : public testing::Test {
 public:
  AudioDeviceSelectionTestBase();
  ~AudioDeviceSelectionTestBase() override;

 protected:
  void SetUp() override;

  void TearDown() override;

  // Register audio devices available in each test case.
  // Each call to NewInputNode or NewOutputNode returns an AudioNode
  // with an auto-incremented node ID, starting at 1.
  AudioNode NewInputNode(const std::string& type) {
    return NewNode(true, type);
  }
  AudioNode NewOutputNode(const std::string& type) {
    return NewNode(false, type);
  }

  void Plug(AudioNode node);
  void Unplug(const AudioNode& node);
  void Select(const AudioNode& node);

  uint64_t ActiveInputNodeId();
  uint64_t ActiveOutputNodeId();

 private:
  AudioNode NewNode(bool is_input, const std::string& type);

 protected:
  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;

 private:
  // Test services
  std::unique_ptr<ActiveNodeObserver> active_node_observer_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<CrasAudioHandler, DanglingUntriaged | ExperimentalAsh>
      cras_audio_handler_ = nullptr;  // Not owned.
  raw_ptr<FakeCrasAudioClient, DanglingUntriaged | ExperimentalAsh>
      fake_cras_audio_client_ = nullptr;  // Not owned.
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  // Counters
  uint64_t node_count_ = 0;
  uint64_t plugged_time_ = 0;
};
}  // namespace ash

#endif
