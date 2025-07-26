// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app.js';

export {BrowserProxyImpl} from 'chrome://resources/cr_components/history_clusters/browser_proxy.js';
export {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/history_clusters/history_clusters.mojom-webui.js';
export {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
export {PageHandlerRemote as HistoryEmbeddingsPageHandlerRemote} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
export type {HistoryClustersAppElement} from './app.js';
