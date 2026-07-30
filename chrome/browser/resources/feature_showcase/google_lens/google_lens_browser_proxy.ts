// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GoogleLensPageHandlerInterface} from '../google_lens.mojom-webui.js';
import {GoogleLensPageHandlerFactory, GoogleLensPageHandlerRemote} from '../google_lens.mojom-webui.js';

export interface GoogleLensBrowserProxy {
  handler: GoogleLensPageHandlerInterface;
}

export class GoogleLensBrowserProxyImpl implements GoogleLensBrowserProxy {
  handler: GoogleLensPageHandlerInterface;

  private constructor() {
    this.handler = new GoogleLensPageHandlerRemote();
    GoogleLensPageHandlerFactory.getRemote().createGoogleLensPageHandler(
        (this.handler as GoogleLensPageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): GoogleLensBrowserProxy {
    return instance || (instance = new GoogleLensBrowserProxyImpl());
  }

  static setInstance(proxy: GoogleLensBrowserProxy) {
    instance = proxy;
  }
}

let instance: GoogleLensBrowserProxy|null = null;
