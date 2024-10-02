// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer component that displays the currently selected
 * wallpaper.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../common/icons.html.js';
import '../../css/wallpaper.css.js';
import '../../css/cros_button_style.css.js';
import './info_svg_element.js';
import './google_photos_shared_album_dialog_element.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {CurrentWallpaper, GooglePhotosPhoto, WallpaperLayout, WallpaperType} from '../../personalization_app.mojom-webui.js';
import {isGooglePhotosSharedAlbumsEnabled, isPersonalizationJellyEnabled} from '../load_time_booleans.js';
import {Paths} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getCheckmarkIcon, isNonEmptyArray} from '../utils.js';

import {getLocalStorageAttribution, getWallpaperLayoutEnum, getWallpaperSrc} from './utils.js';
import {getDailyRefreshState, selectGooglePhotosAlbum, setCurrentWallpaperLayout, setDailyRefreshCollectionId, updateDailyRefreshWallpaper} from './wallpaper_controller.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';
import {WallpaperObserver} from './wallpaper_observer.js';
import {getTemplate} from './wallpaper_selected_element.html.js';
import {DailyRefreshState} from './wallpaper_state.js';

export class WallpaperSelected extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-selected';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current collection id to display.
       */
      collectionId: String,

      /**
       * Whether the google photos album is shared.
       */
      isGooglePhotosAlbumShared: {
        type: Boolean,
        value: false,
      },

      /**
       * The current Google Photos Album id to display.
       */
      googlePhotosAlbumId: String,

      /**
       * The current path of the page.
       */
      path: String,

      photosByAlbumId_: Object,

      image_: {
        type: Object,
        observer: 'onImageChanged_',
      },

      imageTitle_: {
        type: String,
        computed: 'computeImageTitle_(image_, dailyRefreshState_)',
      },

      imageOtherAttribution_: {
        type: Array,
        computed: 'computeImageOtherAttribution_(image_)',
      },

      dailyRefreshState_: Object,

      isLoading_: Boolean,

      hasError_: {
        type: Boolean,
        computed: 'computeHasError_(image_, isLoading_, error_)',
      },

      shouldShowDailyRefreshConfirmationDialog_: Boolean,

      showImage_: {
        type: Boolean,
        computed: 'computeShowImage_(image_, isLoading_)',
      },

      shouldShowLayoutOptions_: {
        type: Boolean,
        computed:
            'computeShouldShowLayoutOptions_(image_, path, googlePhotosAlbumId)',
      },

      shouldShowDescriptionButton_: {
        type: Boolean,
        computed: 'computeShouldShowDescriptionButton_(image_)',
      },

      shouldShowDescriptionDialog_: Boolean,

      showCollectionOptions_: {
        type: Boolean,
        computed: 'computeShowCollectionOptions_(path)',
      },

      showDailyRefreshButton_: {
        type: Boolean,
        computed:
            'isDailyRefreshable_(path,googlePhotosAlbumId,photosByAlbumId_)',
      },

      showRefreshButton_: {
        type: Boolean,
        computed:
            'computeShowRefreshButton_(collectionId,googlePhotosAlbumId,dailyRefreshState_)',
      },

      dailyRefreshIcon_: {
        type: String,
        computed:
            'computeDailyRefreshIcon_(collectionId,googlePhotosAlbumId,dailyRefreshState_)',
      },

      ariaPressed_: {
        type: String,
        computed:
            'computeAriaPressed_(collectionId,googlePhotosAlbumId,dailyRefreshState_)',
      },

      fillIcon_: {
        type: String,
        computed: 'computeFillIcon_(image_)',
      },

      centerIcon_: {
        type: String,
        computed: 'computeCenterIcon_(image_)',
      },

      error_: {
        type: String,
        value: null,
      },

      googlePhotosSharedAlbumsEnabled_: {
        type: Boolean,
        value() {
          return isGooglePhotosSharedAlbumsEnabled();
        },
      },
    };
  }

  // Only one of |collectionId| and |googlePhotosAlbumId| should ever be set,
  // since we can't be in a Backdrop collection or a Google Photos album
  // simultaneously
  collectionId: string|undefined;
  isGooglePhotosAlbumShared: boolean;
  googlePhotosAlbumId: string|undefined;
  path: string;
  private image_: CurrentWallpaper|null;
  private imageTitle_: string;
  private imageOtherAttribution_: string[];
  private dailyRefreshState_: DailyRefreshState|null;
  private isLoading_: boolean;
  private hasError_: boolean;
  private shouldShowDailyRefreshConfirmationDialog_: boolean;
  private showImage_: boolean;
  private shouldShowLayoutOptions_: boolean;
  private shouldShowDescriptionButton_: boolean;
  private shouldShowDescriptionDialog_: boolean;
  private showCollectionOptions_: boolean;
  private showRefreshButton_: boolean;
  private dailyRefreshIcon_: string;
  private ariaPressed_: string;
  private fillIcon_: string;
  private centerIcon_: string;
  private error_: string;
  private googlePhotosSharedAlbumsEnabled_: boolean;
  private photosByAlbumId_: Record<string, GooglePhotosPhoto[]|null|undefined>|
      undefined;

  override connectedCallback() {
    super.connectedCallback();
    WallpaperObserver.initWallpaperObserverIfNeeded();
    this.watch('error_', state => state.error);
    this.watch('image_', state => state.wallpaper.currentSelected);
    this.watch(
        'isLoading_',
        state => state.wallpaper.loading.setImage > 0 ||
            state.wallpaper.loading.selected ||
            state.wallpaper.loading.refreshWallpaper);
    this.watch('dailyRefreshState_', state => state.wallpaper.dailyRefresh);
    this.watch(
        'photosByAlbumId_',
        state => state.wallpaper.googlePhotos.photosByAlbumId);
    this.updateFromStore();
    getDailyRefreshState(getWallpaperProvider(), this.getStore());
  }

  private computeShowImage_(image: CurrentWallpaper|null, loading: boolean):
      boolean {
    // Specifically check === false to avoid undefined case while component is
    // initializing.
    return loading === false && !!image;
  }

  private computeImageTitle_(
      image: CurrentWallpaper|null,
      dailyRefreshState: DailyRefreshState|null): string {
    if (!image) {
      return this.i18n('unknownImageAttribution');
    }
    if (image.type === WallpaperType.kDefault) {
      return this.i18n('defaultWallpaper');
    }
    const isDailyRefreshActive = !!dailyRefreshState;
    if (isNonEmptyArray(image.attribution)) {
      const title = image.attribution[0];
      return isDailyRefreshActive ? this.i18n('dailyRefresh') + ': ' + title :
                                    title;
    } else {
      // Fallback to cached attribution.
      const attribution = getLocalStorageAttribution(image.key);
      if (isNonEmptyArray(attribution)) {
        const title = attribution[0];
        return isDailyRefreshActive ? this.i18n('dailyRefresh') + ': ' + title :
                                      title;
      }
    }
    return this.i18n('unknownImageAttribution');
  }

  private computeImageOtherAttribution_(image: CurrentWallpaper|
                                        null): string[] {
    if (!image) {
      return [];
    }
    if (isNonEmptyArray(image.attribution)) {
      return image.attribution.slice(1);
    }
    // Fallback to cached attribution.
    const attribution = getLocalStorageAttribution(image.key);
    if (isNonEmptyArray(attribution)) {
      return attribution.slice(1);
    }
    return [];
  }

  private computeShouldShowLayoutOptions_(
      image: CurrentWallpaper|null, path: string,
      googlePhotosAlbumId: string): boolean {
    return !!image &&
        ((image.type === WallpaperType.kCustomized &&
              path === Paths.LOCAL_COLLECTION ||
          (image.type === WallpaperType.kOnceGooglePhotos &&
           path === Paths.GOOGLE_PHOTOS_COLLECTION && !googlePhotosAlbumId)));
  }

  private computeShouldShowDescriptionButton_(image: CurrentWallpaper|null) {
    // Only show the description dialog if title and content exist.
    return isPersonalizationJellyEnabled() && image?.descriptionContent &&
        image?.descriptionTitle;
  }

  private computeShowCollectionOptions_(path: string): boolean {
    return path === Paths.COLLECTION_IMAGES ||
        path === Paths.GOOGLE_PHOTOS_COLLECTION;
  }

  private computeShowRefreshButton_(
      collectionId: string|undefined, googlePhotosAlbumId: string|undefined,
      dailyRefreshState: DailyRefreshState|null) {
    return (!collectionId && !googlePhotosAlbumId) ?
        false :
        this.isDailyRefreshId_(
            collectionId! || googlePhotosAlbumId!, dailyRefreshState);
  }

  private getWallpaperSrc_(image: CurrentWallpaper|null): string|null {
    return getWallpaperSrc(image);
  }

  private getCenterAriaPressed_(image: CurrentWallpaper): string {
    return (!!image && image.layout === WallpaperLayout.kCenter).toString();
  }

  private getFillAriaPressed_(image: CurrentWallpaper): string {
    return (!!image && image.layout === WallpaperLayout.kCenterCropped)
        .toString();
  }

  private computeFillIcon_(image: CurrentWallpaper): string {
    if (!!image && image.layout === WallpaperLayout.kCenterCropped) {
      return getCheckmarkIcon();
    }
    return 'personalization:layout_fill';
  }

  private computeCenterIcon_(image: CurrentWallpaper): string {
    if (!!image && image.layout === WallpaperLayout.kCenter) {
      return getCheckmarkIcon();
    }
    return 'personalization:layout_center';
  }

  private onClickLayoutIcon_(event: Event) {
    const eventTarget = event.currentTarget as HTMLElement;
    const layout = getWallpaperLayoutEnum(eventTarget.dataset['layout']!);
    setCurrentWallpaperLayout(layout, getWallpaperProvider(), this.getStore());
  }

  private computeDailyRefreshIcon_(
      collectionId: string, googlePhotosAlbumId: string,
      dailyRefreshState: DailyRefreshState|null): string {
    if (this.isDailyRefreshId_(
            collectionId || googlePhotosAlbumId, dailyRefreshState)) {
      return getCheckmarkIcon();
    }
    return 'personalization:change-daily';
  }

  private computeAriaPressed_(
      collectionId: string|undefined, googlePhotosAlbumId: string|undefined,
      dailyRefreshState: DailyRefreshState|null): string {
    if (this.isDailyRefreshId_(
            collectionId || googlePhotosAlbumId, dailyRefreshState)) {
      return 'true';
    }
    return 'false';
  }

  private onClickDailyRefreshToggle_() {
    const dailyRefreshEnabled = this.isDailyRefreshId_(
        this.collectionId || this.googlePhotosAlbumId, this.dailyRefreshState_);
    if (dailyRefreshEnabled) {
      this.disableDailyRefresh_();
    } else {
      this.enableDailyRefresh_();
    }
  }

  private isDailyRefreshable_(
      path: string, googlePhotosAlbumId: string|undefined,
      photosByAlbumId: Record<string, GooglePhotosPhoto[]|null|undefined>) {
    const isNonEmptyGooglePhotosAlbum = !!googlePhotosAlbumId &&
        !!photosByAlbumId &&
        isNonEmptyArray(photosByAlbumId[googlePhotosAlbumId]);
    return path === Paths.COLLECTION_IMAGES ||
        (path === Paths.GOOGLE_PHOTOS_COLLECTION &&
         isNonEmptyGooglePhotosAlbum);
  }

  /**
   * Determine the current collection view belongs to the collection that is
   * enabled with daily refresh. If true, highlight the toggle and display the
   * refresh button
   */
  private isDailyRefreshId_(
      id: string|undefined,
      dailyRefreshState: DailyRefreshState|null): boolean {
    return dailyRefreshState ? id === dailyRefreshState.id : false;
  }

  private disableDailyRefresh_() {
    if (this.googlePhotosAlbumId) {
      assert(!this.collectionId);
      selectGooglePhotosAlbum(
          /*albumId=*/ '', getWallpaperProvider(), this.getStore());
    } else {
      setDailyRefreshCollectionId(
          /*collectionId=*/ '', getWallpaperProvider(), this.getStore());
    }
  }

  private enableDailyRefresh_() {
    if (this.googlePhotosAlbumId) {
      assert(!this.collectionId);
      if (this.googlePhotosSharedAlbumsEnabled_ &&
          this.isGooglePhotosAlbumShared) {
        this.shouldShowDailyRefreshConfirmationDialog_ = true;
      } else {
        this.enableGooglePhotosAlbumDailyRefresh_();
      }
    } else {
      setDailyRefreshCollectionId(
          this.collectionId!, getWallpaperProvider(), this.getStore());
    }
  }

  private enableGooglePhotosAlbumDailyRefresh_() {
    selectGooglePhotosAlbum(
        this.googlePhotosAlbumId!, getWallpaperProvider(), this.getStore());
  }

  private showDescriptionDialog_() {
    assert(
        isPersonalizationJellyEnabled(),
        'description dialog only available if personalization jelly enabled');
    assert(
        this.shouldShowDescriptionButton_,
        'description dialog can only be opened if button is visible');
    this.shouldShowDescriptionDialog_ = true;
  }

  private closeDescriptionDialog_() {
    this.shouldShowDescriptionDialog_ = false;
  }

  private closeDailyRefreshConfirmationDialog_() {
    this.shouldShowDailyRefreshConfirmationDialog_ = false;
    this.shadowRoot!.getElementById('dailyRefresh')?.focus();
  }

  private onAcceptDailyRefreshDialog_() {
    this.enableGooglePhotosAlbumDailyRefresh_();
    this.closeDailyRefreshConfirmationDialog_();
  }

  private onClickUpdateDailyRefreshWallpaper_() {
    updateDailyRefreshWallpaper(getWallpaperProvider(), this.getStore());
  }

  /**
   * Determine whether there is an error in showing selected image. An error
   * happens when there is no previously loaded image and either no new image
   * is being loaded or there is an error from upstream.
   */
  private computeHasError_(
      image: CurrentWallpaper|null, loading: boolean,
      error: string|null): boolean {
    return (!loading || !!error) && !image;
  }

  private getAriaLabel_(
      image: CurrentWallpaper|null,
      dailyRefreshState: DailyRefreshState|null): string {
    if (!image) {
      return this.i18n('currentlySet') + ' ' +
          this.i18n('unknownImageAttribution');
    }
    if (image.type === WallpaperType.kDefault) {
      return `${this.i18n('currentlySet')} ${this.i18n('defaultWallpaper')}`;
    }
    const isDailyRefreshActive = !!dailyRefreshState;
    if (isNonEmptyArray(image.attribution)) {
      return isDailyRefreshActive ?
          [
            this.i18n('currentlySet'),
            this.i18n('dailyRefresh'),
            ...image.attribution,
          ].join(' ') :
          [this.i18n('currentlySet'), ...image.attribution].join(' ');
    }
    // Fallback to cached attribution.
    const attribution = getLocalStorageAttribution(image.key);
    if (isNonEmptyArray(attribution)) {
      return isDailyRefreshActive ?
          [
            this.i18n('currentlySet'),
            this.i18n('dailyRefresh'),
            ...image.attribution,
          ].join(' ') :
          [this.i18n('currentlySet'), ...attribution].join(' ');
    }
    return this.i18n('currentlySet') + ' ' +
        this.i18n('unknownImageAttribution');
  }

  /**
   * Returns hidden state of loading placeholder.
   */
  private showPlaceholders_(loading: boolean, showImage: boolean): boolean {
    return loading || !showImage;
  }

  /**
   * Cache the attribution in local storage when image is updated
   * Populate the attribution map in local storage when image is updated
   */
  private async onImageChanged_(
      newImage: CurrentWallpaper|null, oldImage: CurrentWallpaper|null) {
    const attributionMap =
        JSON.parse((window.localStorage['attribution'] || '{}'));
    if (attributionMap.size == 0 ||
        !!newImage && !!oldImage && newImage.key !== oldImage.key) {
      if (newImage) {
        attributionMap[newImage.key] = newImage.attribution;
      }
      if (oldImage) {
        delete attributionMap[oldImage.key];
      }
      window.localStorage['attribution'] = JSON.stringify(attributionMap);
    }
  }

  /**
   * Return a container class depending on loading state.
   */
  private getContainerClass_(isLoading: boolean, showImage: boolean): string {
    return this.showPlaceholders_(isLoading, showImage) ? 'loading' : '';
  }
}

customElements.define(WallpaperSelected.is, WallpaperSelected);
