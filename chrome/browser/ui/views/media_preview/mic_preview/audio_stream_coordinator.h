// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_AUDIO_STREAM_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_AUDIO_STREAM_COORDINATOR_H_

#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "components/capture_mode/audio_capturer.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "ui/views/view_tracker.h"

class AudioStreamView;

// Sets up, updates and maintains the AudioStreamView.
class AudioStreamCoordinator {
 public:
  explicit AudioStreamCoordinator(
      views::View& parent_view,
      media_preview_metrics::Context metrics_context);
  AudioStreamCoordinator(const AudioStreamCoordinator&) = delete;
  AudioStreamCoordinator& operator=(const AudioStreamCoordinator&) = delete;
  ~AudioStreamCoordinator();

  // Initializes AudioCapturer, and request to start receiving feed.
  void ConnectToDevice(mojo::PendingRemote<media::mojom::AudioStreamFactory>
                           audio_stream_factory,
                       const std::string& device_id,
                       int sample_rate);

  void Stop();

  // Called when the preview is about to close.
  void OnClosing();

  void SetAudioBusReceivedCallbackForTest(base::RepeatingClosure callback) {
    audio_bus_received_callback_for_test_ = std::move(callback);
  }

  capture_mode::AudioCapturer* GetAudioCapturerForTest() {
    return audio_capturing_callback_.get();
  }

 private:
  void OnAudioCaptured(std::unique_ptr<media::AudioBus> audio_bus,
                       base::TimeTicks capture_time);

  AudioStreamView* GetAudioStreamView();

  void OnReceivedFirstAudioSample();

  const media_preview_metrics::Context metrics_context_;

  views::ViewTracker audio_stream_view_tracker_;
  std::unique_ptr<capture_mode::AudioCapturer> audio_capturing_callback_;

  // Used to compute rolling average.
  float last_audio_level_ = 0;

  // Runs when a new audio bus is received. Used for testing.
  base::RepeatingClosure audio_bus_received_callback_for_test_;

  const base::TimeTicks audio_stream_construction_time_;

  std::optional<base::TimeTicks> audio_stream_request_time_;
  std::optional<base::TimeTicks> audio_stream_start_time_;

  base::WeakPtrFactory<AudioStreamCoordinator> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MIC_PREVIEW_AUDIO_STREAM_COORDINATOR_H_
