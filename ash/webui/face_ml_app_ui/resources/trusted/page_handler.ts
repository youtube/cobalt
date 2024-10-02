// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from '/ash/webui/face_ml_app_ui/mojom/face_ml_app_ui.mojom-webui.js';

// Used to make calls on the remote PageHandler interface. Singleton that client
// modules can use directly.
export const pageHandler = new PageHandlerRemote();

// Use this subscribe to events e.g.
// `callbackRouter.onEventOccurred.addListener(handleEvent)`.
export const callbackRouter = new PageCallbackRouter();

// Use PageHandlerFactory to create a connection to PageHandler.
const factoryRemote = PageHandlerFactory.getRemote();
factoryRemote.createPageHandler(
    pageHandler.$.bindNewPipeAndPassReceiver(),
    callbackRouter.$.bindNewPipeAndPassRemote());
