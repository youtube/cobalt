// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserHost, ScrollToParams} from '../../glic_api/glic_api.js';
import type {WebClientHost, WebClientInitialStatePrivate} from '../request_types.js';
import type {PendingReceiver, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

import {AnnotationHostDef} from './annotation_types.js';
import type {AnnotationHost} from './annotation_types.js';

type Constructor<T = {}> = new (...args: any[]) => T;

export function glicBrowserHostAnnotationMixin<T extends Constructor>(base: T) {
  return class extends base implements Partial<GlicBrowserHost> {
    annotationSender?: PostMessageRemote<AnnotationHost>;
    annotationReceiver?: PendingReceiver<AnnotationHost>;
    mainClientRemote?: PostMessageRemote<WebClientHost>;

    initializeAnnotation(
        initialState: WebClientInitialStatePrivate, router: PostMessageRouter,
        clientRemote: PostMessageRemote<WebClientHost>) {
      if (!initialState.enableScrollTo) {
        this.scrollTo = undefined;
        this.dropScrollToHighlight = undefined;
        return;
      }

      const {remote, receiver} = router.newPipeWithRemote(AnnotationHostDef);
      this.annotationSender = remote;
      this.annotationReceiver = receiver;
      this.mainClientRemote = clientRemote;
    }

    async scrollTo?(params: ScrollToParams): Promise<void> {
      this.ensureAnnotationHandlerCreated();
      return this.annotationSender!.requestWithResponse('scrollTo', {params});
    }

    dropScrollToHighlight?(): void {
      this.ensureAnnotationHandlerCreated();
      this.annotationSender!.requestNoResponse(
          'dropScrollToHighlight', undefined);
    }

    ensureAnnotationHandlerCreated() {
      if (this.annotationReceiver === undefined) {
        return;
      }
      this.mainClientRemote!.requestNoResponse('createAnnotationHandler', {
        annotationReceiver: this.annotationReceiver,
      });
      this.annotationReceiver = undefined;
    }
  };
}
