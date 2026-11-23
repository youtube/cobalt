// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FinishOrContinuePageHandlerFactory, FinishOrContinuePageHandlerRemote} from '../finish_or_continue.mojom-webui.js';
import type {FinishOrContinuePageHandlerInterface} from '../finish_or_continue.mojom-webui.js';

export interface FinishOrContinueBrowserProxy {
  handler: FinishOrContinuePageHandlerInterface;

  matchMedia(query: string): MediaQueryList;
}

export class FinishOrContinueBrowserProxyImpl implements
    FinishOrContinueBrowserProxy {
  handler: FinishOrContinuePageHandlerInterface;

  private constructor() {
    this.handler = new FinishOrContinuePageHandlerRemote();

    const factory = FinishOrContinuePageHandlerFactory.getRemote();
    factory.createFinishOrContinuePageHandler(
        (this.handler as FinishOrContinuePageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): FinishOrContinueBrowserProxy {
    return instance || (instance = new FinishOrContinueBrowserProxyImpl());
  }

  static setInstance(proxy: FinishOrContinueBrowserProxy) {
    instance = proxy;
  }

  matchMedia(query: string): MediaQueryList {
    return window.matchMedia(query);
  }
}

let instance: FinishOrContinueBrowserProxy|null = null;
