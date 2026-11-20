// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The WebIDL compiler autogenerates an enum value called kNone, so we can't use
// "none" here to represent "object-fit: none".
enum ObjectFit {
  "fill",
  "contain",
  "cover",
  "object-fit-none",
  "scale-down"
};

dictionary ImageData {
  // The image data, either as webp bytes or a data URL.
  (ArrayBuffer or DOMString) value;
  // The object-fit attribute of the image.
  ObjectFit objectFit;
};

// Use the <code>chrome.indigoPrivate</code> API for specific browser
// functionality that the Indigo extension needs.
interface IndigoPrivate {
  // Notifies the browser that the extension frame is ready to be rendered.
  // Returns the active "invocationId" for this replacement frame, which should
  // be used as the instanceId to filter onRegenerateStarted events.
  // |PromiseValue|: invocationId
  static Promise<long> readyToRender();

  // Returns the original image data.
  // |PromiseValue|: imageData
  static Promise<ImageData> getOriginalImage();

  // Returns the replacement image data.
  // |PromiseValue|: imageData
  static Promise<ImageData> getReplacementImage();

  // Fired when image regeneration has started. getReplacementImage() can be
  // called after to retrieve the regenerated image.
  [supportsFilters] static attribute OnRegenerateStartedEvent
      onRegenerateStarted;
};

callback OnRegenerateStartedListener = undefined ();

interface OnRegenerateStartedEvent : ExtensionEvent {
  static undefined addListener(OnRegenerateStartedListener listener);
  static undefined removeListener(OnRegenerateStartedListener listener);
  static boolean hasListener(OnRegenerateStartedListener listener);
};

partial interface Browser {
  static attribute IndigoPrivate indigoPrivate;
};
