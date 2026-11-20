// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The type of transcription update.
enum TranscriptionType {
  "partial",
  "final"
};

// The current state of the dictation stream.
enum StreamState {
  "initializing",
  "failed",
  "transcribing",
  "complete"
};

dictionary StartStreamDetails {
  // The unique identifier of the stream.
  required long streamId;
  // The current page context details.
  required DOMString pageContext;
  // The existing content of the editable field.
  required DOMString editableContent;
};

callback OnStartStreamListener = undefined (StartStreamDetails details);

interface OnStartStreamEvent : ExtensionEvent {
  static undefined addListener(OnStartStreamListener listener);
  static undefined removeListener(OnStartStreamListener listener);
  static boolean hasListener(OnStartStreamListener listener);
};

dictionary EndStreamDetails {
  // The unique identifier of the stream.
  required long streamId;
};

callback OnEndStreamListener = undefined (EndStreamDetails details);

interface OnEndStreamEvent : ExtensionEvent {
  static undefined addListener(OnEndStreamListener listener);
  static undefined removeListener(OnEndStreamListener listener);
  static boolean hasListener(OnEndStreamListener listener);
};

dictionary UpdateTranscriptionDetails {
  // The unique identifier of the dictation stream.
  required long streamId;
  // The type of transcription update.
  required TranscriptionType type;
  // The transcribed text data.
  required DOMString data;
};

dictionary SetStreamStateDetails {
  required long streamId;
  required StreamState state;
};

// The dictationPrivate API is a private API used by the dictation extension.
interface DictationPrivate {
  // Sends the transcription to the browser.
  static Promise<undefined> updateTranscription(
      UpdateTranscriptionDetails details);

  // Notifies the browser of stream state changes.
  static Promise<undefined> setStreamState(SetStreamStateDetails details);

  // Fired to instruct the extension to start capturing microphone audio and
  // connecting to transcription service.
  static attribute OnStartStreamEvent onStartStream;

  // Fired to instruct the extension to finish transcribing.
  static attribute OnEndStreamEvent onEndStream;
};

partial interface Browser {
  static attribute DictationPrivate dictationPrivate;
};
