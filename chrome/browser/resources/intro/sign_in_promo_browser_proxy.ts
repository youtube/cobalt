// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SignInPromoPageCallbackRouter, SignInPromoPageHandlerFactory, SignInPromoPageHandlerRemote} from './sign_in_promo.mojom-webui.js';
import type {SignInPromoPageHandlerInterface} from './sign_in_promo.mojom-webui.js';

export interface SignInPromoBrowserProxy {
  callbackRouter: SignInPromoPageCallbackRouter;
  handler: SignInPromoPageHandlerInterface;

  matchMedia(query: string): MediaQueryList;
}

export class SignInPromoBrowserProxyImpl implements SignInPromoBrowserProxy {
  callbackRouter: SignInPromoPageCallbackRouter;
  handler: SignInPromoPageHandlerInterface;

  private constructor() {
    this.callbackRouter = new SignInPromoPageCallbackRouter();
    this.handler = new SignInPromoPageHandlerRemote();

    const factory = SignInPromoPageHandlerFactory.getRemote();
    factory.createSignInPromoPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as SignInPromoPageHandlerRemote)
            .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): SignInPromoBrowserProxy {
    return instance || (instance = new SignInPromoBrowserProxyImpl());
  }

  static setInstance(proxy: SignInPromoBrowserProxy) {
    instance = proxy;
  }

  matchMedia(query: string): MediaQueryList {
    return window.matchMedia(query);
  }
}

let instance: SignInPromoBrowserProxy|null = null;
