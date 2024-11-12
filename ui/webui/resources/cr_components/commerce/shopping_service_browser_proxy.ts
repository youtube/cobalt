// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import type {ProductSpecificationsSet} from './shared.mojom-webui.js';
import type {BookmarkProductInfo, PriceInsightsInfo, ProductInfo, ProductSpecifications, ProductSpecificationsFeatureState, UrlInfo, UserFeedback} from './shopping_service.mojom-webui.js';
import {PageCallbackRouter, ShoppingServiceHandlerFactory, ShoppingServiceHandlerRemote} from './shopping_service.mojom-webui.js';

let instance: ShoppingServiceBrowserProxy|null = null;

export interface ShoppingServiceBrowserProxy {
  getAllPriceTrackedBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  getAllShoppingBookmarkProductInfo():
      Promise<{productInfos: BookmarkProductInfo[]}>;
  trackPriceForBookmark(bookmarkId: bigint): void;
  untrackPriceForBookmark(bookmarkId: bigint): void;
  getProductInfoForCurrentUrl(): Promise<{productInfo: ProductInfo}>;
  getPriceInsightsInfoForCurrentUrl():
      Promise<{priceInsightsInfo: PriceInsightsInfo}>;
  showInsightsSidePanelUi(): void;
  getUrlInfosForProductTabs(): Promise<{urlInfos: UrlInfo[]}>;
  getUrlInfosForRecentlyViewedTabs(): Promise<{urlInfos: UrlInfo[]}>;
  isShoppingListEligible(): Promise<{eligible: boolean}>;
  getShoppingCollectionBookmarkFolderId(): Promise<{collectionId: bigint}>;
  getPriceTrackingStatusForCurrentUrl(): Promise<{tracked: boolean}>;
  setPriceTrackingStatusForCurrentUrl(track: boolean): void;
  openUrlInNewTab(url: Url): void;
  switchToOrOpenTab(url: Url): void;
  getParentBookmarkFolderNameForCurrentUrl(): Promise<{name: String16}>;
  showBookmarkEditorForCurrentUrl(): void;
  showFeedbackForPriceInsights(): void;
  getCallbackRouter(): PageCallbackRouter;
  getPriceInsightsInfoForUrl(url: Url):
      Promise<{priceInsightsInfo: PriceInsightsInfo}>;
  getProductInfoForUrl(url: Url): Promise<{productInfo: ProductInfo}>;
  getProductSpecificationsForUrls(urls: Url[]):
      Promise<{productSpecs: ProductSpecifications}>;
  getAllProductSpecificationsSets():
      Promise<{sets: ProductSpecificationsSet[]}>;
  getProductSpecificationsSetByUuid(uuid: Uuid):
      Promise<{set: ProductSpecificationsSet | null}>;
  addProductSpecificationsSet(name: string, urls: Url[]):
      Promise<{createdSet: ProductSpecificationsSet | null}>;
  deleteProductSpecificationsSet(uuid: Uuid): void;
  setNameForProductSpecificationsSet(uuid: Uuid, name: string):
      Promise<{updatedSet: ProductSpecificationsSet | null}>;
  setUrlsForProductSpecificationsSet(uuid: Uuid, urls: Url[]):
      Promise<{updatedSet: ProductSpecificationsSet | null}>;
  setProductSpecificationsUserFeedback(feedback: UserFeedback): void;
  getProductSpecificationsFeatureState():
      Promise<{state: ProductSpecificationsFeatureState | null}>;
}

export class ShoppingServiceBrowserProxyImpl implements
    ShoppingServiceBrowserProxy {
  handler: ShoppingServiceHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new ShoppingServiceHandlerRemote();

    const factory = ShoppingServiceHandlerFactory.getRemote();
    factory.createShoppingServiceHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getAllPriceTrackedBookmarkProductInfo() {
    return this.handler.getAllPriceTrackedBookmarkProductInfo();
  }

  getAllShoppingBookmarkProductInfo() {
    return this.handler.getAllShoppingBookmarkProductInfo();
  }

  trackPriceForBookmark(bookmarkId: bigint) {
    this.handler.trackPriceForBookmark(bookmarkId);
  }

  untrackPriceForBookmark(bookmarkId: bigint) {
    this.handler.untrackPriceForBookmark(bookmarkId);
  }

  getProductInfoForCurrentUrl() {
    return this.handler.getProductInfoForCurrentUrl();
  }

  getProductInfoForUrl(url: Url) {
    return this.handler.getProductInfoForUrl(url);
  }

  getPriceInsightsInfoForCurrentUrl() {
    return this.handler.getPriceInsightsInfoForCurrentUrl();
  }

  getPriceInsightsInfoForUrl(url: Url) {
    return this.handler.getPriceInsightsInfoForUrl(url);
  }

  getProductSpecificationsForUrls(urls: Url[]) {
    return this.handler.getProductSpecificationsForUrls(urls);
  }

  getUrlInfosForProductTabs() {
    return this.handler.getUrlInfosForProductTabs();
  }

  getUrlInfosForRecentlyViewedTabs() {
    return this.handler.getUrlInfosForRecentlyViewedTabs();
  }

  showInsightsSidePanelUi() {
    this.handler.showInsightsSidePanelUI();
  }

  isShoppingListEligible() {
    return this.handler.isShoppingListEligible();
  }

  getShoppingCollectionBookmarkFolderId() {
    return this.handler.getShoppingCollectionBookmarkFolderId();
  }

  getPriceTrackingStatusForCurrentUrl() {
    return this.handler.getPriceTrackingStatusForCurrentUrl();
  }

  setPriceTrackingStatusForCurrentUrl(track: boolean) {
    this.handler.setPriceTrackingStatusForCurrentUrl(track);
  }

  openUrlInNewTab(url: Url) {
    this.handler.openUrlInNewTab(url);
  }

  switchToOrOpenTab(url: Url) {
    this.handler.switchToOrOpenTab(url);
  }

  getParentBookmarkFolderNameForCurrentUrl() {
    return this.handler.getParentBookmarkFolderNameForCurrentUrl();
  }

  showBookmarkEditorForCurrentUrl() {
    this.handler.showBookmarkEditorForCurrentUrl();
  }

  showFeedbackForPriceInsights() {
    this.handler.showFeedbackForPriceInsights();
  }

  getAllProductSpecificationsSets() {
    return this.handler.getAllProductSpecificationsSets();
  }

  getProductSpecificationsSetByUuid(uuid: Uuid) {
    return this.handler.getProductSpecificationsSetByUuid(uuid);
  }

  addProductSpecificationsSet(name: string, urls: Url[]) {
    return this.handler.addProductSpecificationsSet(name, urls);
  }

  deleteProductSpecificationsSet(uuid: Uuid) {
    this.handler.deleteProductSpecificationsSet(uuid);
  }

  setNameForProductSpecificationsSet(uuid: Uuid, name: string) {
    return this.handler.setNameForProductSpecificationsSet(uuid, name);
  }

  setUrlsForProductSpecificationsSet(uuid: Uuid, urls: Url[]) {
    return this.handler.setUrlsForProductSpecificationsSet(uuid, urls);
  }

  setProductSpecificationsUserFeedback(feedback: UserFeedback) {
    this.handler.setProductSpecificationsUserFeedback(feedback);
  }

  getProductSpecificationsFeatureState() {
    return this.handler.getProductSpecificationsFeatureState();
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): ShoppingServiceBrowserProxy {
    return instance || (instance = new ShoppingServiceBrowserProxyImpl());
  }

  static setInstance(obj: ShoppingServiceBrowserProxy) {
    instance = obj;
  }
}
