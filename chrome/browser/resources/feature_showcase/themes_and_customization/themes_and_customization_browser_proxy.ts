// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ThemesAndCustomizationPageHandlerInterface} from '../themes_and_customization.mojom-webui.js';
import {ThemesAndCustomizationPageHandlerFactory, ThemesAndCustomizationPageHandlerRemote} from '../themes_and_customization.mojom-webui.js';

export interface ThemesAndCustomizationBrowserProxy {
  handler: ThemesAndCustomizationPageHandlerInterface;
}

export class ThemesAndCustomizationBrowserProxyImpl implements
    ThemesAndCustomizationBrowserProxy {
  handler: ThemesAndCustomizationPageHandlerInterface;

  private constructor() {
    this.handler = new ThemesAndCustomizationPageHandlerRemote();
    ThemesAndCustomizationPageHandlerFactory.getRemote()
        .createThemesAndCustomizationPageHandler(
            (this.handler as ThemesAndCustomizationPageHandlerRemote)
                .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): ThemesAndCustomizationBrowserProxy {
    return instance ||
        (instance = new ThemesAndCustomizationBrowserProxyImpl());
  }

  static setInstance(proxy: ThemesAndCustomizationBrowserProxy) {
    instance = proxy;
  }
}

let instance: ThemesAndCustomizationBrowserProxy|null = null;
