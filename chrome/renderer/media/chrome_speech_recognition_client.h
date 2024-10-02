// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_
#define CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/base/audio_buffer.h"
#include "media/base/speech_recognition_client.h"
#include "media/mojo/common/audio_data_s16_converter.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrame;
}  // namespace content

class ChromeSpeechRecognitionClient
    : public content::RenderFrameObserver,
      public media::SpeechRecognitionClient,
      public media::mojom::SpeechRecognitionBrowserObserver,
      public media::AudioDataS16Converter {
 public:
  using SendAudioToSpeechRecognitionServiceCallback =
      base::RepeatingCallback<void(media::mojom::AudioDataS16Ptr audio_data)>;
  using InitializeCallback = base::RepeatingCallback<void()>;

  explicit ChromeSpeechRecognitionClient(
      content::RenderFrame* render_frame,
      media::SpeechRecognitionClient::OnReadyCallback callback);
  ChromeSpeechRecognitionClient(const ChromeSpeechRecognitionClient&) = delete;
  ChromeSpeechRecognitionClient& operator=(
      const ChromeSpeechRecognitionClient&) = delete;
  ~ChromeSpeechRecognitionClient() override;

  // content::RenderFrameObserver
  void OnDestruct() override;

  // media::SpeechRecognitionClient
  void AddAudio(scoped_refptr<media::AudioBuffer> buffer) override;
  void AddAudio(std::unique_ptr<media::AudioBus> audio_bus,
                int sample_rate,
                media::ChannelLayout channel_layout) override;
  bool IsSpeechRecognitionAvailable() override;
  void SetOnReadyCallback(
      SpeechRecognitionClient::OnReadyCallback callback) override;

  // Callback executed when the recognizer is bound. Sets the flag indicating
  // whether the speech recognition service supports multichannel audio.
  void OnRecognizerBound(bool is_multichannel_supported);

  // media::mojom::SpeechRecognitionBrowserObserver
  void SpeechRecognitionAvailabilityChanged(
      bool is_speech_recognition_available) override;
  void SpeechRecognitionLanguageChanged(const std::string& language) override;

 private:
  // Initialize the speech recognition client and construct all of the mojo
  // pipes.
  void Initialize();

  // Resets the mojo pipe to the speech recognition recognizer and speech
  // recognition service. Maintains the pipe to the browser so that it may be
  // notified when to reinitialize the pipes.
  void Reset();

  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr audio_data);

  // Called when the speech recognition context or the speech recognition
  // recognizer is disconnected. Sends an error message to the UI and halts
  // future transcriptions.
  void OnRecognizerDisconnected();

  ChromeSpeechRecognitionClient::InitializeCallback initialize_callback_;

  media::SpeechRecognitionClient::OnReadyCallback on_ready_callback_;

  base::RepeatingClosure reset_callback_;

  // Sends audio to the speech recognition thread on the renderer thread.
  SendAudioToSpeechRecognitionServiceCallback send_audio_callback_;

  mojo::Receiver<media::mojom::SpeechRecognitionBrowserObserver>
      speech_recognition_availability_observer_{this};
  mojo::Remote<media::mojom::SpeechRecognitionClientBrowserInterface>
      speech_recognition_client_browser_interface_;

  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;
  mojo::Remote<media::mojom::SpeechRecognitionRecognizer>
      speech_recognition_recognizer_;

  // Whether all mojo pipes are bound to the speech recognition service.
  bool is_recognizer_bound_ = false;

  // A flag indicating whether the speech recognition service supports
  // multichannel audio.
  bool is_multichannel_supported_ = false;

  base::WeakPtrFactory<ChromeSpeechRecognitionClient> weak_factory_{this};
};

#endif  // CHROME_RENDERER_MEDIA_CHROME_SPEECH_RECOGNITION_CLIENT_H_
