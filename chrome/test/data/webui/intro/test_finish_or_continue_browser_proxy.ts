// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FinishOrContinuePageHandlerRemote} from 'chrome://intro/finish_or_continue.mojom-webui.js';
import type {FinishOrContinueBrowserProxy} from 'chrome://intro/finish_or_continue/finish_or_continue_browser_proxy.js';
import {FakeMediaQueryList} from 'chrome://webui-test/fake_media_query_list.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export class TestFinishOrContinueBrowserProxy implements
    FinishOrContinueBrowserProxy {
  handler: TestMock<FinishOrContinuePageHandlerRemote>&
      FinishOrContinuePageHandlerRemote;
  private mediaQueryList_: FakeMediaQueryList = new FakeMediaQueryList('dummy');

  constructor() {
    this.handler = TestMock.fromClass(FinishOrContinuePageHandlerRemote);
  }

  matchMedia(_query: string): MediaQueryList {
    return this.mediaQueryList_ as unknown as MediaQueryList;
  }

  setMatchMediaMatches(matches: boolean): void {
    this.mediaQueryList_.matches = matches;
  }
}
