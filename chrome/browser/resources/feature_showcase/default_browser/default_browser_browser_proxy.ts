// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DefaultBrowserPageHandlerInterface} from '../default_browser.mojom-webui.js';
import {DefaultBrowserPageHandlerFactory, DefaultBrowserPageHandlerRemote} from '../default_browser.mojom-webui.js';

export interface DefaultBrowserBrowserProxy {
  handler: DefaultBrowserPageHandlerInterface;
}

export class DefaultBrowserBrowserProxyImpl implements
    DefaultBrowserBrowserProxy {
  handler: DefaultBrowserPageHandlerInterface;

  private constructor() {
    this.handler = new DefaultBrowserPageHandlerRemote();
    DefaultBrowserPageHandlerFactory.getRemote().createPageHandler(
        (this.handler as DefaultBrowserPageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): DefaultBrowserBrowserProxy {
    return instance || (instance = new DefaultBrowserBrowserProxyImpl());
  }

  static setInstance(proxy: DefaultBrowserBrowserProxy) {
    instance = proxy;
  }
}

let instance: DefaultBrowserBrowserProxy|null = null;
