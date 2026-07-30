// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SignInPromoPageCallbackRouter, SignInPromoPageHandlerRemote} from 'chrome://intro/sign_in_promo.mojom-webui.js';
import type {SignInPromoPageRemote} from 'chrome://intro/sign_in_promo.mojom-webui.js';
import type {SignInPromoBrowserProxy} from 'chrome://intro/sign_in_promo_browser_proxy.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestSignInPromoBrowserProxy implements SignInPromoBrowserProxy {
  callbackRouter: SignInPromoPageCallbackRouter;
  handler: TestMock<SignInPromoPageHandlerRemote>&SignInPromoPageHandlerRemote;
  page: SignInPromoPageRemote;
  private mediaQueryList_: FakeMediaQueryList = new FakeMediaQueryList('dummy');
  private disclaimerResolver_: PromiseResolver<{disclaimer: string}> =
      new PromiseResolver();

  constructor() {
    this.callbackRouter = new SignInPromoPageCallbackRouter();
    this.handler = TestMock.fromClass(SignInPromoPageHandlerRemote);
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler.setResultFor(
        'getManagedDeviceDisclaimer', this.disclaimerResolver_.promise);
  }

  resolveDisclaimer(disclaimer: string) {
    this.disclaimerResolver_.resolve({disclaimer});
  }

  matchMedia(_query: string): MediaQueryList {
    return this.mediaQueryList_ as unknown as MediaQueryList;
  }

  setMatchMediaMatches(matches: boolean): void {
    this.mediaQueryList_.matches = matches;
  }
}
