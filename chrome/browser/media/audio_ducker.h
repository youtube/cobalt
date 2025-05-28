// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_AUDIO_DUCKER_H_
#define CHROME_BROWSER_MEDIA_AUDIO_DUCKER_H_

#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

// The AudioDucker ducks other audio in Chrome besides what is playing through
// its associated Page's WebContents.
//
// TODO(https://crbug.com/382316461): This should also duck audio from
// applications other than Chrome.
class AudioDucker : public content::PageUserData<AudioDucker>,
                    public content::WebContentsObserver {
 public:
  enum class AudioDuckingState {
    // The AudioDucker is not currently ducking other audio.
    kNoDucking,

    // The AudioDucker is currently ducking other audio.
    kDucking,
  };

  AudioDucker(const AudioDucker&) = delete;
  AudioDucker& operator=(const AudioDucker&) = delete;
  ~AudioDucker() override;

  // Ducks all audio in Chrome except audio playback in our associated Page's
  // WebContents. Returns true if ducking was started successfully or if we're
  // already ducking.
  bool StartDuckingOtherAudio();

  // Stops ducking started by `StartDuckingOtherAudio()`. Does nothing if we're
  // not currently ducking. Returns true if ducking was stopped successfully or
  // if we're not currently ducking.
  bool StopDuckingOtherAudio();

  AudioDuckingState GetAudioDuckingState() const { return ducking_state_; }

 private:
  explicit AudioDucker(content::Page& page);

  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();

  // content::WebContentsObserver:
  void MediaSessionCreated(content::MediaSession*) override;

  void StartDuckingImpl();

  content::WebContents* GetWebContents() const;

  // Binds |audio_focus_remote_| if it's not already bound. Returns true if
  // |audio_focus_remote_| is already bound or has become bound.
  bool BindToAudioFocusManagerIfNecessary();

  AudioDuckingState ducking_state_ = AudioDuckingState::kNoDucking;
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
};

#endif  // CHROME_BROWSER_MEDIA_AUDIO_DUCKER_H_
