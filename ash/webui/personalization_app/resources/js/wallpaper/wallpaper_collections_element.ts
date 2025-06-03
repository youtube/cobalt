// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Polymer element that fetches and displays a list of WallpaperCollection
 * objects.
 */

import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../css/wallpaper.css.js';
import '../../common/icons.html.js';
import '../../css/common.css.js';

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GooglePhotosEnablementState, WallpaperCollection, WallpaperImage} from '../../personalization_app.mojom-webui.js';
import {isGooglePhotosIntegrationEnabled, isPersonalizationJellyEnabled, isSeaPenEnabled, isTimeOfDayWallpaperEnabled} from '../load_time_booleans.js';
import {Paths, PersonalizationRouterElement} from '../personalization_router_element.js';
import {WithPersonalizationStore} from '../personalization_store.js';
import {getCountText, isImageDataUrl, isNonEmptyArray, isSelectionEvent} from '../utils.js';

import {DefaultImageSymbol, kDefaultImageSymbol, kMaximumLocalImagePreviews} from './constants.js';
import {getLoadingPlaceholderAnimationDelay, getLoadingPlaceholders, getPathOrSymbol} from './utils.js';
import {getTemplate} from './wallpaper_collections_element.html.js';
import {fetchGooglePhotosEnabled, fetchLocalData, getDefaultImageThumbnail, initializeBackdropData} from './wallpaper_controller.js';
import {WallpaperGridItemSelectedEvent} from './wallpaper_grid_item_element.js';
import {getWallpaperProvider} from './wallpaper_interface_provider.js';

const kGooglePhotosCollectionId = 'google_photos_';
const kLocalCollectionId = 'local_';
const kSeaPenId = 'sea_pen_';

enum TileType {
  IMAGE_GOOGLE_PHOTOS = 'image_google_photos',
  IMAGE_LOCAL = 'image_local',
  IMAGE_ONLINE = 'image_online',
  LOADING = 'loading',
  SEA_PEN = 'sea_pen',
}

interface LoadingTile {
  type: TileType.LOADING;
  id: string;
}

interface GooglePhotosTile {
  disabled: boolean;
  id: typeof kGooglePhotosCollectionId;
  name: string;
  type: TileType.IMAGE_GOOGLE_PHOTOS;
  preview: [Url];
}

interface SeaPenTile {
  disabled: boolean;
  id: typeof kSeaPenId;
  name: string;
  preview: [Url];
  type: TileType.SEA_PEN;
}

interface LocalTile {
  count: string;
  disabled: boolean;
  id: typeof kLocalCollectionId;
  name: string;
  preview: Url[];
  type: TileType.IMAGE_LOCAL;
}

interface OnlineTile {
  count: string;
  disabled: boolean;
  id: string;
  info: string;
  name: string;
  preview: Url[];
  type: TileType.IMAGE_ONLINE;
}

type Tile = LoadingTile|GooglePhotosTile|LocalTile|OnlineTile|SeaPenTile;

// "regular" backdrop collections are displayed differently than the special
// "timeOfDay" wallpaper collection. Split them to make them easier to handle.
// The special "timeOfDay" collection may not exist depending on the device and
// features enabled.
interface SplitCollections {
  regular: WallpaperCollection[];
  timeOfDay: WallpaperCollection|null;
}

/**
 * Before switching entirely to WallpaperGridItem, have to deal with a mix of
 * keyboard and mouse events and a custom WallpaperGridItemSelected event.
 */
type OnCollectionSelectedEvent =
    (MouseEvent|KeyboardEvent|WallpaperGridItemSelectedEvent)&
    {model: {item: Tile}};

function collectionsError(
    collections: WallpaperCollection[]|null,
    collectionsLoading: boolean): boolean {
  return !collectionsLoading && !isNonEmptyArray(collections);
}

function localImagesError(
    localImages: Array<FilePath|DefaultImageSymbol>|null,
    localImagesLoading: boolean): boolean {
  return !localImagesLoading && !isNonEmptyArray(localImages);
}

function hasError(
    collections: WallpaperCollection[]|null, collectionsLoading: boolean,
    localImages: Array<FilePath|DefaultImageSymbol>|null,
    localImagesLoading: boolean): boolean {
  return localImagesError(localImages, localImagesLoading) &&
      collectionsError(collections, collectionsLoading);
}

/** Returns the tile to display for the Google Photos collection. */
function getGooglePhotosTile(enablementState: GooglePhotosEnablementState):
    GooglePhotosTile {
  return {
    disabled: enablementState !== GooglePhotosEnablementState.kEnabled,
    id: kGooglePhotosCollectionId,
    name: loadTimeData.getString('googlePhotosLabel'),
    type: TileType.IMAGE_GOOGLE_PHOTOS,
    preview: [{url: 'chrome://personalization/images/google_photos.svg'}],
  };
}

function getImages(
    localImages: Array<FilePath|DefaultImageSymbol>,
    localImageData: Record<string|DefaultImageSymbol, Url>): Url[] {
  if (!localImageData || !Array.isArray(localImages)) {
    return [];
  }
  const result = [];
  for (const image of localImages) {
    const key = getPathOrSymbol(image);
    const data = localImageData[key];
    if (isImageDataUrl(data)) {
      result.push(data);
    }
    // Add at most |kMaximumLocalImagePreviews| thumbnail urls.
    if (result.length >= kMaximumLocalImagePreviews) {
      break;
    }
  }
  return result;
}

/**
 * A common display format between local images and WallpaperCollection.
 * Get the first displayable image with data from the list of possible images.
 */
function getLocalTile(
    localImages: Array<FilePath|DefaultImageSymbol>|null,
    localImagesLoading: boolean,
    localImageData: Record<FilePath['path']|DefaultImageSymbol, Url>):
    LocalTile|LoadingTile {
  if (localImagesLoading) {
    return {type: TileType.LOADING, id: kLocalCollectionId};
  }

  if (!localImages || localImages.length === 0) {
    // TODO(b/282050032): After Jelly is launched, remove the preview image.
    return {
      count: getCountText(0),
      disabled: true,
      id: kLocalCollectionId,
      name: loadTimeData.getString('myImagesLabel'),
      preview: [{url: 'chrome://personalization/images/no_images.svg'}],
      type: TileType.IMAGE_LOCAL,
    };
  }

  const imagesToDisplay = getImages(localImages, localImageData);

  // Count all images that failed to load and subtract them from "My Images"
  // count.
  const failureCount = Object.values(localImageData).reduce((result, next) => {
    return !isImageDataUrl(next) ? result + 1 : result;
  }, 0);

  const successCount = localImages.length - failureCount;

  return {
    count: getCountText(successCount),
    disabled: successCount <= 0,
    id: kLocalCollectionId,
    name: loadTimeData.getString('myImagesLabel'),
    preview: imagesToDisplay,
    type: TileType.IMAGE_LOCAL,
  };
}

function getOnlineTile(
    collection: WallpaperCollection, imageCount: number|null): OnlineTile {
  return {
    count: getCountText(imageCount || 0),
    // If `imageCount` is null or 0, this collection failed to load and the user
    // cannot select it.
    disabled: !imageCount,
    id: collection.id,
    info: collection.descriptionContent,
    name: collection.name,
    preview: collection.previews,
    type: TileType.IMAGE_ONLINE,
  };
}

function getSeaPenTile(): SeaPenTile {
  return {
    disabled: false,
    id: kSeaPenId,
    name: 'Sea Pen',
    type: TileType.SEA_PEN,
    // TODO(b/299359804): Replace with the real preview.
    preview: [{url: 'chrome://personalization/images/google_photos.svg'}],
  };
}

function getTemporaryBackdropCollectionId(index: number) {
  return `backdrop_collection_${index}`;
}

function isTimeOfDay({id}: WallpaperCollection|Tile): boolean {
  return id === loadTimeData.getString('timeOfDayWallpaperCollectionId');
}

export interface WallpaperCollectionsElement {
  $: {
    grid: IronListElement,
  };
}

export class WallpaperCollectionsElement extends WithPersonalizationStore {
  static get is() {
    return 'wallpaper-collections';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Hidden state of this element. Used to notify of visibility changes. */
      hidden: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'onHiddenChanged_',
      },

      collections_: {
        type: Array,
        observer: 'onCollectionsChanged_',
      },

      /**
       * Wallpaper collections split out into "regular" collections, and the
       * special time of day collection.
       */
      splitCollections_: Object,

      images_: Object,

      imagesLoading_: Object,

      /**
       * Whether the user is allowed to access Google Photos.
       */
      googlePhotosEnabled_: {
        type: Number,
        observer: 'onGooglePhotosEnabledChanged_',
      },

      /**
       * Mapping of collection id to number of images. Loads in progressively
       * after collections_.
       */
      imageCounts_: {
        type: Object,
        computed: 'computeImageCounts_(images_, imagesLoading_)',
      },

      localImages_: Array,

      localImagesLoading_: Boolean,

      /**
       * Stores a mapping of local image id to thumbnail data.
       */
      localImageData_: {
        type: Object,
        value: {},
      },

      /**
       * List of tiles to be displayed to the user.
       */
      tiles_: {
        type: Array,
        value() {
          // Fill the view with loading tiles. Will be adjusted to the correct
          // number of tiles when collections are received.
          const placeholders = getLoadingPlaceholders<LoadingTile>(
              () => ({type: TileType.LOADING, id: ''}));

          let currentIndex = 0;
          // Time of day tile.
          if (isTimeOfDayWallpaperEnabled()) {
            placeholders[currentIndex].id =
                loadTimeData.getString('timeOfDayWallpaperCollectionId');
            currentIndex++;
          }

          if (isSeaPenEnabled()) {
            placeholders[currentIndex].id = kSeaPenId;
            currentIndex++;
          }

          // Local images tile.
          placeholders[currentIndex].id = kLocalCollectionId;
          currentIndex++;

          // Google Photos tile.
          if (isGooglePhotosIntegrationEnabled()) {
            placeholders[currentIndex].id = kGooglePhotosCollectionId;
            currentIndex++;
          }

          // The rest of the backdrop tiles. Actual number will be adjusted once
          // collections are received. The actual id is not important as long as
          // they are unique.
          const firstBackdropIndex = currentIndex;
          while (currentIndex < placeholders.length) {
            placeholders[currentIndex].id = getTemporaryBackdropCollectionId(
                currentIndex - firstBackdropIndex);
            currentIndex++;
          }

          return placeholders;
        },
      },

      hasError_: Boolean,

      isPersonalizationJellyEnabled_: {
        type: Boolean,
        value() {
          return isPersonalizationJellyEnabled();
        },
      },

      isSeaPenEnabled_: {
        type: Boolean,
        value() {
          return isSeaPenEnabled();
        },
      },
    };
  }

  override hidden: boolean;
  private collections_: WallpaperCollection[]|null;
  private splitCollections_: SplitCollections|null;
  private images_: Record<string, WallpaperImage[]|null>;
  private imagesLoading_: Record<string, boolean>;
  private imageCounts_: Record<string, number|null>;
  private googlePhotosEnabled_: GooglePhotosEnablementState|undefined;
  private seaPenEnabled_: boolean;
  private localImages_: Array<FilePath|DefaultImageSymbol>|null;
  private localImagesLoading_: boolean;
  private localImageData_: Record<string|DefaultImageSymbol, Url>;
  private tiles_: Tile[];
  private hasError_: boolean;
  private isPersonalizationJellyEnabled_: boolean;

  static get observers() {
    return [
      'onLocalImagesChanged_(localImages_, localImagesLoading_, localImageData_)',
      'onCollectionLoaded_(splitCollections_, imageCounts_)',
    ];
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<WallpaperCollectionsElement['hasError_']>(
        'hasError_',
        state => hasError(
            state.wallpaper.backdrop.collections,
            state.wallpaper.loading.collections, state.wallpaper.local.images,
            state.wallpaper.loading.local.images));
    this.watch<WallpaperCollectionsElement['collections_']>(
        'collections_', state => state.wallpaper.backdrop.collections);
    this.watch<WallpaperCollectionsElement['images_']>(
        'images_', state => state.wallpaper.backdrop.images);
    this.watch<WallpaperCollectionsElement['imagesLoading_']>(
        'imagesLoading_', state => state.wallpaper.loading.images);
    this.watch<WallpaperCollectionsElement['googlePhotosEnabled_']>(
        'googlePhotosEnabled_', state => state.wallpaper.googlePhotos.enabled);
    this.watch<WallpaperCollectionsElement['localImages_']>(
        'localImages_', state => state.wallpaper.local.images);
    // Treat as loading if either loading local images list or loading the
    // default image thumbnail. This prevents rapid churning of the UI on first
    // load.
    this.watch<WallpaperCollectionsElement['localImagesLoading_']>(
        'localImagesLoading_',
        state => state.wallpaper.loading.local.images ||
            state.wallpaper.loading.local.data[kDefaultImageSymbol]);
    this.watch<WallpaperCollectionsElement['localImageData_']>(
        'localImageData_', state => state.wallpaper.local.data);
    this.updateFromStore();
    initializeBackdropData(getWallpaperProvider(), this.getStore());
    getDefaultImageThumbnail(getWallpaperProvider(), this.getStore());
    fetchLocalData(getWallpaperProvider(), this.getStore());
    window.addEventListener('focus', () => {
      fetchLocalData(getWallpaperProvider(), this.getStore());
    });
    if (isGooglePhotosIntegrationEnabled()) {
      fetchGooglePhotosEnabled(getWallpaperProvider(), this.getStore());
    }
    this.setSeaPenTile_();
  }

  /**
   * Tiles are laid out
   * `[time_of_day?, local, google_photos?, ...regular backdrop tiles...]`.
   * Get the index of the first regular backdrop tile.
   * @returns the index of the first regular backdrop tile.
   */
  private getFirstRegularBackdropTileIndex(): number {
    const firstBackdropIndex = this.tiles_.findIndex(
        tile => tile.id !== kLocalCollectionId &&
            tile.id !== kGooglePhotosCollectionId && !isTimeOfDay(tile) &&
            tile.id !== kSeaPenId);
    assert(
        firstBackdropIndex > 0,
        'first backdrop index must always be greater than 0');
    return firstBackdropIndex;
  }

  /**
   * Notify that this element visibility has changed.
   */
  private async onHiddenChanged_(hidden: boolean) {
    if (!hidden) {
      document.title = this.i18n('wallpaperLabel');
    }
    afterNextRender(this, () => this.$.grid.fire('iron-resize'));
  }

  /**
   * Called when the list of wallpaper collections changes. Collections are not
   * actually displayed until they have completed loading, which is handled by
   * `onCollectionLoaded_`.
   */
  private onCollectionsChanged_(collections: WallpaperCollection[]|null) {
    if (!isNonEmptyArray(collections)) {
      this.splitCollections_ = null;
      return;
    }

    const timeOfDay = collections.find(isTimeOfDay) ?? null;
    if (!timeOfDay && isTimeOfDayWallpaperEnabled()) {
      console.error('missing time of day wallpaper from collections');
      this.tiles_ = this.tiles_.filter(tile => !isTimeOfDay(tile));
    }

    // Delay assigning `this.splitCollections_` until the correct number of
    // tiles are assigned.
    const splitCollections = {
      regular: collections.filter(collection => !isTimeOfDay(collection)),
      timeOfDay,
    };

    // This is the index of the first tile after the "special" tiles like time
    // of day, local images, and google photos.
    const firstBackdropIndex = this.getFirstRegularBackdropTileIndex();
    const desiredNumTiles =
        splitCollections.regular.length + firstBackdropIndex;

    // Adjust the number of loading tiles to match the collections that just
    // came in.  There may be more (or fewer) loading tiles than necessary to
    // display all collections. Match the number of tiles to the correct length.
    if (this.tiles_.length < desiredNumTiles) {
      this.push(
          'tiles_',
          ...Array.from(
              {length: desiredNumTiles - this.tiles_.length},
              (_, i): LoadingTile => {
                return {
                  type: TileType.LOADING,
                  id: getTemporaryBackdropCollectionId(
                      i - firstBackdropIndex + this.tiles_.length),
                };
              }));
    }
    if (this.tiles_.length > desiredNumTiles) {
      this.splice('tiles_', desiredNumTiles);
    }

    // Assign `this.splitCollections_` now that
    // `tiles_.length === desiredNumTiles`.
    this.splitCollections_ = splitCollections;
  }

  /**
   * Calculate count of image units in each collection when a new collection is
   * fetched. D/L variants of the same image represent a count of 1.
   */
  private computeImageCounts_(
      images: Record<string, WallpaperImage[]|null>,
      imagesLoading: Record<string, boolean>): Record<string, number|null> {
    if (!images || !imagesLoading) {
      return {};
    }
    return Object.entries(images)
        .filter(([collectionId]) => {
          return imagesLoading[collectionId] === false;
        })
        .map(([key, value]) => {
          // Collection has completed loading. If no images were
          // retrieved, set count value to null to indicate
          // failure.
          if (Array.isArray(value)) {
            const unitIds = new Set();
            value.forEach(image => {
              unitIds.add(image.unitId);
            });
            return [key, unitIds.size] as [string, number];
          } else {
            return [key, null] as [string, null];
          }
        })
        .reduce((result, [key, value]) => {
          result[key] = value;
          return result;
        }, {} as Record<string, number|null>);
  }

  private getLoadingPlaceholderAnimationDelay_(index: number): string {
    return getLoadingPlaceholderAnimationDelay(index);
  }

  /**
   * Called each time a new collection finishes loading. |imageCounts| contains
   * a mapping of collection id to the number of images in that collection.
   * A value of null indicates that the given collection id has failed to load.
   */
  private onCollectionLoaded_(
      splitCollections: SplitCollections|null,
      imageCounts: Record<string, number|null>) {
    if (!splitCollections || !isNonEmptyArray(splitCollections.regular) ||
        !imageCounts) {
      return;
    }

    const firstBackdropIndex = this.getFirstRegularBackdropTileIndex();

    splitCollections.regular.forEach((collection, i) => {
      assert(
          isNonEmptyArray(collection.previews),
          `preview images required for collection ${collection.id}`);

      const index = i + firstBackdropIndex;
      const tile = this.tiles_[index];

      if (imageCounts[collection.id] === undefined) {
        // Collection is still loading, skip.
        return;
      }
      const newTile = getOnlineTile(collection, imageCounts[collection.id]);
      if (tile.type !== newTile.type || tile.id !== newTile.id ||
          tile.count !== newTile.count) {
        this.set(`tiles_.${index}`, newTile);
      }
    });

    if (splitCollections.timeOfDay &&
        imageCounts[splitCollections.timeOfDay.id] !== undefined) {
      const tileIndex = this.tiles_.findIndex(isTimeOfDay);
      if (tileIndex < 0) {
        console.warn('received time of day collection when not supported');
        return;
      }
      const tile = this.tiles_[tileIndex];
      const newTile = getOnlineTile(
          splitCollections.timeOfDay,
          imageCounts[splitCollections.timeOfDay.id]);
      if (tile.type !== newTile.type || tile.count !== newTile.count) {
        this.set(`tiles_.${tileIndex}`, newTile);
      }
    }
  }

  /** Invoked on changes to |googlePhotosEnabled_|. */
  private onGooglePhotosEnabledChanged_(googlePhotosEnabled:
                                            GooglePhotosEnablementState|
                                        undefined) {
    if (googlePhotosEnabled !== undefined) {
      assert(
          isGooglePhotosIntegrationEnabled(),
          'google photos integration must be enabled');
      const tile = getGooglePhotosTile(googlePhotosEnabled);
      const index =
          this.tiles_.findIndex(tile => tile.id === kGooglePhotosCollectionId);
      assert(index >= 0, 'could not find google photos tile');
      this.set(`tiles_.${index}`, tile);
    }
  }

  private setSeaPenTile_() {
    if (!isSeaPenEnabled()) {
      return;
    }
    const tile = getSeaPenTile();
    const index = this.tiles_.findIndex(tile => tile.id === kSeaPenId);
    assert(index >= 0, `${kSeaPenId} not found`);
    this.set(`tiles_.${index}`, tile);
  }

  /**
   * Called with updated local image list or local image thumbnail data when
   * either of those properties changes.
   */
  private onLocalImagesChanged_(
      localImages: Array<FilePath|DefaultImageSymbol>|null,
      localImagesLoading: boolean,
      localImageData: Record<FilePath['path']|DefaultImageSymbol, Url>) {
    const tile = getLocalTile(localImages, localImagesLoading, localImageData);
    const index = this.tiles_.findIndex(tile => tile.id === kLocalCollectionId);
    assert(index >= 0, 'could not find local tile');
    this.set(`tiles_.${index}`, tile);
  }

  /** Navigate to the correct route based on user selection. */
  private onCollectionSelected_(e: OnCollectionSelectedEvent) {
    const tile = e.model.item;
    assert(!!tile, 'tile must be set to select');
    if (!this.isSelectableTile_(tile)) {
      // Ignore all events from disabled/loading tiles.
      return;
    }
    if (!(e instanceof WallpaperGridItemSelectedEvent) &&
        !isSelectionEvent(e)) {
      // While refactoring is in progress, may receive either a
      // `WallpaperGridItemSelectedEvent` or a mouse/keyboard selection event.
      // If not one of those two event types, ignore it and let it propagate.
      return;
    }
    switch (tile.id) {
      case kGooglePhotosCollectionId:
        PersonalizationRouterElement.instance().goToRoute(
            Paths.GOOGLE_PHOTOS_COLLECTION);
        return;
      case kLocalCollectionId:
        PersonalizationRouterElement.instance().goToRoute(
            Paths.LOCAL_COLLECTION);
        return;
      case kSeaPenId:
        PersonalizationRouterElement.instance().goToRoute(
            Paths.SEA_PEN_COLLECTION);
        return;
      default:
        assert(
            isNonEmptyArray(this.collections_), 'collections array required');
        const collection =
            this.collections_.find(collection => collection.id === tile.id);
        assert(collection, 'collection with matching id required');
        PersonalizationRouterElement.instance().selectCollection(collection);
        return;
    }
  }

  private isLoadingTile_(item: Tile|null): item is LoadingTile {
    return !!item && item.type === TileType.LOADING;
  }

  private isLocalTile_(item: Tile|null): item is LocalTile {
    return !!item && item.type === TileType.IMAGE_LOCAL;
  }

  private isSeaPenTile_(item: Tile|null): item is SeaPenTile {
    return !!item && item.type === TileType.SEA_PEN;
  }

  private isOnlineTile_(item: Tile|null): item is OnlineTile {
    return !!item && item.type === TileType.IMAGE_ONLINE;
  }

  private isGooglePhotosTile_(item: Tile|null): item is GooglePhotosTile {
    return !!item && item.type === TileType.IMAGE_GOOGLE_PHOTOS;
  }

  private isSelectableTile_(item: Tile|null): item is GooglePhotosTile|LocalTile
      |OnlineTile {
    return !!item && !this.isLoadingTile_(item) && !item.disabled;
  }

  private isLocalNoImagesTile_(item: Tile): boolean {
    return !!item && this.isLocalTile_(item) && item.count === getCountText(0);
  }

  private isTimeOfDayCollection_(item: Tile|null): boolean {
    return this.isOnlineTile_(item) && isTimeOfDay(item);
  }

  private getAriaIndex_(index: number): number {
    return index + 1;
  }

  private getOnlineTileSecondaryText_(item: Tile): string {
    assert(this.isOnlineTile_(item), 'item must be online tile');
    if (this.isTimeOfDayCollection_(item)) {
      return loadTimeData.getString('timeOfDayWallpaperCollectionSublabel');
    }
    return item.count;
  }
}

customElements.define(
    WallpaperCollectionsElement.is, WallpaperCollectionsElement);
