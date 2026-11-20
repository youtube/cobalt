// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IntroPageCallbackRouter} from 'chrome://intro/intro.mojom-webui.js';
import type {IntroBrowserProxy} from 'chrome://intro/intro_browser_proxy.js';

import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';

export class TestIntroMojoBrowserProxy implements IntroBrowserProxy {
  callbackRouter: IntroPageCallbackRouter = new IntroPageCallbackRouter();
  private mediaQueryList_: FakeMediaQueryList = new FakeMediaQueryList('dummy');

  matchMedia(_query: string): MediaQueryList {
    return this.mediaQueryList_;
  }

  setMatchMediaMatches(matches: boolean): void {
    this.mediaQueryList_.matches = matches;
  }
}
