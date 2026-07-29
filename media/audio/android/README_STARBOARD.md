# Starboard Low Latency Microphone Path (Android)

This document describes the low-latency microphone capture path implemented for Cobalt on Android, also known as the "Fast-Track" microphone path.

Reference Bug: [b/483713292](https://b.corp.google.com/issues/483713292)

---

## Overview

The standard Chromium audio capture path on Android suffered from severe, unusable latency (taking up to 7 seconds for the microphone to become ready). This rendered voice search effectively broken, as the first few syllables spoken by the user were consistently lost, and the delayed initialization often led to unusable search results.

### Cobalt-Specific Constraints
The latency was exceptionally severe in Cobalt due to a combination of platform factors:
*   **Resource-Constrained Devices:** Cobalt runs on low-end Living Room devices (Smart TVs, TV boxes, dongles) with limited CPU and memory.
*   **Single-Process Architecture:** Cobalt runs in single-process mode. While Chromium's IPC (Mojo) is still used for modularity, it translates to inter-thread communication within the same process.
*   **Main Thread Contention:** The overhead of thread-switching and message serialization, combined with heavy JavaScript execution on the main thread, created severe CPU contention. This delayed the processing of audio IPC messages, compounding the initialization latency.

To resolve this, Starboard implements a **"Fast-Track"** audio capture path that:
1.  **Pre-starts** the audio hardware recording asynchronously as soon as the user media request is initiated.
2.  **Resolves the JavaScript promise immediately**, allowing the UI to transition to the "Listening" state instantly while the hardware is still warming up.
3.  **Bypasses complex channel/sample-rate negotiations** by hardcoding parameters to **16kHz Mono** (Starboard spec).
4.  **Bypasses JS-side downsampling** by forcing the WebAudio context to 16kHz Mono end-to-end.

---

## Architectural Details

### 1. Initiation and Fast-Track Setup (`MediaStreamManager`)
When the web application calls `navigator.mediaDevices.getUserMedia({audio: true, video: false})`, the request is handled by `MediaStreamManager::SetUpRequest` on the Browser IO thread.

*   **Detection:** If the request is audio-only and `USE_STARBOARD_MEDIA` is enabled, it is routed to the fast-track path.
*   **Permission Check:** On Android, we post a task to request OS-level runtime permission (`StarboardAudioInputStream::RequestRuntimePermission`) on the UI thread.
*   **Pre-Starting Hardware:** If permission is granted, `MediaStreamManager::CompleteFastTrackSetUp` is called:
    *   It hardcodes the audio parameters to **16kHz Mono** (PCM Low Latency, 16000Hz, 1024 samples per buffer).
    *   It generates a unique `session_token` and encodes it into the `device_id` as `fast-track-<session_token>`.
    *   It calls `AudioManager::PreStartStream(session_token, params)` to warm up the hardware.
    *   It immediately calls `HandleAccessRequestResponse` with `OK`, resolving the JS `getUserMedia()` promise before the hardware is fully ready.

### 2. Pre-Starting the Stream (`AudioManagerAndroid`)
In `AudioManagerAndroid::PreStartStream`:
*   It creates a `PreStartedEntry` in the `pre_started_streams_` map, keyed by the `session_id`.
*   It instantiates a `StarboardAudioInputStream` and calls `Open()` on it.
*   `StarboardAudioInputStream::Open()` immediately initializes OpenSL ES and starts the hardware recorder (`SL_RECORDSTATE_RECORDING`).
*   Once opened, the stream is parked in the `PreStartedEntry` and the `open_event` is signaled.

### 3. Consuming the Pre-started Stream
When the web application attaches the returned media stream to a WebAudio node or MediaRecorder, the renderer process initiates an IPC connection to create the audio input stream.

*   The IPC request carries the `device_id` containing the `fast-track-<session_id>` prefix.
*   `AudioManagerAndroid::MakeAudioInputStream` intercepts this request, extracts the `session_id`, and retrieves the parked `PreStartedEntry` from the map.
*   It waits for the `open_event` to ensure the hardware is open, and then returns the pre-started stream directly, avoiding any additional warmup delay.

---

## Native Implementation (`StarboardAudioInputStream`)

`StarboardAudioInputStream` is a simplified version of Chromium's `OpenSLESInputStream` specifically optimized for Cobalt:
*   **Fixed Format:** Hardcoded to 16kHz Mono PCM.
*   **Audio Preset:** Uses `SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION` for recording. This preset is **critical** on Google Android TV (ATV) devices (such as Chromecast/Kirkwood) because it triggers the Android system's dynamic audio routing. This routing is required to start the remote service (`go/atv-remote-service`) which captures audio from Bluetooth Low Energy (BLE) remote control microphones.
*   **Immediate Start:** Starts hardware recording in `Open()` rather than waiting for `Start()`.

---

## Optimizations (Feature Flag: `kCobaltAudioCaptureFastTrack`)

When `media::kCobaltAudioCaptureFastTrack` is enabled, several end-to-end optimizations are activated:

1.  **WebAudio Context Alignment:** `AudioContext::Create` forces the default sample rate to **16kHz** if no rate is specified. This aligns the JS engine with the native 16kHz capture, completely bypassing the heavy `OfflineAudioContext` downsampling that the YouTube application normally performs in JavaScript.
2.  **Mono End-to-End:** `WebAudioMediaStreamAudioSink` and `MediaStreamAudioSourceHandler` are forced to Mono mode, preventing latency-inducing channel upmixers in Chromium.
3.  **Increased Buffer Queue:** `AudioInputDevice` increases the shared memory segment count from 10 to **32** (`kStarboardRequestedSharedMemoryCount`). This larger queue prevents audio glitches and dropouts caused by renderer thread jank.
