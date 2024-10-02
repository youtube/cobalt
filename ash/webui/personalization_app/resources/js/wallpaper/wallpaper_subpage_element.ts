// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays the wallpaper section of the
 * personalization SWA.
 */

import {CurrentWallpaper, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {isGooglePhotosIntegrationEnabled} from '../load_time_booleans.js';
import {Paths, PersonalizationRouter, QueryParams} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './wallpaper_subpage_element.html.js';

export class WallpaperSubpage extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path: String,
      queryParams: Object,
      currentSelected_: {
        type: Object,
        value: null,
        observer: 'onCurrentSelectedChanged_',
      },
      isGooglePhotosIntegrationEnabled_: {
        type: Boolean,
        value() {
          return isGooglePhotosIntegrationEnabled();
        },
      },
      isGooglePhotosAlbumShared_: {
        type: Boolean,
        computed: 'computeIsGooglePhotosAlbumShared_(queryParams)',
      },
    };
  }

  path: string;
  queryParams: QueryParams;
  private currentSelected_: CurrentWallpaper|null;
  private isGooglePhotosIntegrationEnabled_: boolean;
  private isGooglePhotosAlbumShared_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();
    this.watch('currentSelected_', state => state.wallpaper.currentSelected);
    this.updateFromStore();
  }

  private onCurrentSelectedChanged_(value: CurrentWallpaper|null) {
    if (value && value.type === WallpaperType.kPolicy) {
      PersonalizationRouter.reloadAtRoot();
    }
  }

  private computeIsGooglePhotosAlbumShared_(queryParams?: QueryParams):
      boolean {
    return !!queryParams && queryParams.googlePhotosAlbumIsShared === 'true';
  }

  private shouldShowCollections_(path: string): boolean {
    return path === Paths.COLLECTIONS;
  }

  private shouldShowCollectionImages_(path: string): boolean {
    return path === Paths.COLLECTION_IMAGES;
  }

  private shouldShowGooglePhotosCollection_(path: string): boolean {
    return this.isGooglePhotosIntegrationEnabled_ &&
        path === Paths.GOOGLE_PHOTOS_COLLECTION;
  }

  private shouldShowLocalCollection_(path: string): boolean {
    return path === Paths.LOCAL_COLLECTION;
  }
}

customElements.define(WallpaperSubpage.is, WallpaperSubpage);
