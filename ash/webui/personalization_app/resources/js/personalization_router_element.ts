// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This router component hooks into the current url path and query
 * parameters to display sections of the personalization SWA.
 */

import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-query-params.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GooglePhotosAlbum, TopicSource, WallpaperCollection} from './../personalization_app.mojom-webui.js';
import {isAmbientModeAllowed} from './load_time_booleans.js';
import {logPersonalizationPathUMA} from './personalization_metrics_logger.js';
import {getTemplate} from './personalization_router_element.html.js';

export enum Paths {
  AMBIENT = '/ambient',
  AMBIENT_ALBUMS = '/ambient/albums',
  COLLECTION_IMAGES = '/wallpaper/collection',
  COLLECTIONS = '/wallpaper',
  GOOGLE_PHOTOS_COLLECTION = '/wallpaper/google-photos',
  LOCAL_COLLECTION = '/wallpaper/local',
  ROOT = '/',
  SEA_PEN_COLLECTION = '/wallpaper/sea-pen',
  USER = '/user',
}

export enum ScrollableTarget {
  TOPIC_SOURCE_LIST = 'topic-source-list'
}

export interface QueryParams {
  id?: string;
  googlePhotosAlbumId?: string;
  // If present, expected to always be 'true'.
  googlePhotosAlbumIsShared?: 'true';
  topicSource?: string;
  scrollTo?: ScrollableTarget;
  seaPenTemplateId?: string;
}

export function isPathValid(path: string|null): boolean {
  return !!path && Object.values(Paths).includes(path as Paths);
}

export function isAmbientPath(path: string|null): boolean {
  return !!path && path.startsWith(Paths.AMBIENT);
}

export function isAmbientPathAllowed(path: string|null): boolean {
  return isAmbientPath(path) && isAmbientModeAllowed();
}

export function isAmbientPathNotAllowed(path: string|null): boolean {
  return isAmbientPath(path) && !isAmbientModeAllowed();
}

export class PersonalizationRouterElement extends PolymerElement {
  static get is() {
    return 'personalization-router';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      path_: {
        type: String,
        observer: 'onPathChanged_',
      },

      query_: {
        type: String,
      },

      queryParams_: {
        type: Object,
      },
    };
  }
  private path_: string;
  private query_: string;
  private queryParams_: QueryParams;

  static instance(): PersonalizationRouterElement {
    return document.querySelector(PersonalizationRouterElement.is) as
        PersonalizationRouterElement;
  }

  static reloadAtRoot() {
    window.location.replace(Paths.ROOT);
  }

  /**
   * Reload the application at the collections page.
   */
  static reloadAtWallpaper() {
    window.location.replace(Paths.COLLECTIONS);
  }

  /**
   * Reload the application at the ambient subpage.
   */
  static reloadAtAmbient() {
    window.location.replace(Paths.AMBIENT);
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  get collectionId() {
    if (this.path_ !== Paths.COLLECTION_IMAGES) {
      return null;
    }
    return this.queryParams_.id;
  }

  /**
   * Navigate to the selected collection id. Assumes validation of the
   * collection has already happened.
   */
  selectCollection(collection: WallpaperCollection) {
    document.title = collection.name;
    this.goToRoute(Paths.COLLECTION_IMAGES, {id: collection.id});
  }

  /** Navigate to a specific album in the Google Photos collection page. */
  selectGooglePhotosAlbum(album: GooglePhotosAlbum) {
    this.goToRoute(
        Paths.GOOGLE_PHOTOS_COLLECTION,
        {
          googlePhotosAlbumId: album.id,
          // Only include key if album is shared.
          ...(album.isShared ? {googlePhotosAlbumIsShared: 'true'} : false),
        },
    );
  }

  /** Navigate to albums subpage of specific topic source. */
  selectAmbientAlbums(topicSource: TopicSource) {
    this.goToRoute(Paths.AMBIENT_ALBUMS, {topicSource: topicSource.toString()});
  }

  selectSeaPenTemplate(templateId: string) {
    this.goToRoute(Paths.SEA_PEN_COLLECTION, {seaPenTemplateId: templateId});
  }

  goToRoute(path: Paths, queryParams: QueryParams = {}) {
    this.setProperties({path_: path, queryParams_: queryParams});
  }

  private shouldShowRootPage_(path: string|null): boolean {
    // If the ambient mode is not allowed, will not show Ambient/AmbientAlbums
    // subpages.
    return (path === Paths.ROOT) || (isAmbientPathNotAllowed(path));
  }

  private shouldShowAmbientSubpage_(path: string|null): boolean {
    return isAmbientPathAllowed(path);
  }

  private shouldShowUserSubpage_(path: string|null): boolean {
    return path === Paths.USER;
  }

  private shouldShowWallpaperSubpage_(path: string|null): boolean {
    return !!path && path.startsWith(Paths.COLLECTIONS);
  }

  private shouldShowBreadcrumb_(path: string|null): boolean {
    return path !== Paths.ROOT;
  }

  /**
   * When entering a wrong path or navigating to Ambient/AmbientAlbums
   * subpages, but the ambient mode is not allowed, reset path to root.
   */
  private onPathChanged_(path: string|null) {
    // Navigates to the top of the subpage.
    window.scrollTo(0, 0);

    if (!isPathValid(path) || isAmbientPathNotAllowed(path)) {
      // Reset the path to root.
      this.setProperties({path_: Paths.ROOT, queryParams_: {}});
    }

    if (isPathValid(path)) {
      logPersonalizationPathUMA(path as Paths);
    }
    // Update the page title when the path changes.
    // TODO(b/228967523): Wallpaper related pages have been handled in their
    // specific Polymer elements so they are skipped here. See if we can move
    // them here.
    switch (path) {
      case Paths.ROOT:
        document.title = loadTimeData.getString('personalizationTitle');
        break;
      case Paths.AMBIENT:
        document.title = loadTimeData.getString('screensaverLabel');
        break;
      case Paths.AMBIENT_ALBUMS: {
        assert(!!this.queryParams_.topicSource);
        if (this.queryParams_.topicSource ===
            TopicSource.kGooglePhotos.toString()) {
          document.title =
              loadTimeData.getString('ambientModeTopicSourceGooglePhotos');
        } else {
          document.title =
              loadTimeData.getString('ambientModeTopicSourceArtGallery');
        }
        break;
      }
      case Paths.GOOGLE_PHOTOS_COLLECTION: {
        document.title = loadTimeData.getString('googlePhotosLabel');
        break;
      }
      case Paths.LOCAL_COLLECTION: {
        document.title = loadTimeData.getString('myImagesLabel');
        break;
      }
      case Paths.USER:
        document.title = loadTimeData.getString('avatarLabel');
        break;
    }
  }
}

customElements.define(
    PersonalizationRouterElement.is, PersonalizationRouterElement);
