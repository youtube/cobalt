// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../strings.m.js';
import './commerce/shopping_list.js';
import './icons.html.js';
import './power_bookmarks_context_menu.js';
import './power_bookmarks_labels.js';
import './power_bookmark_row.js';
import './power_bookmarks_context_menu.js';
import './power_bookmarks_edit_dialog.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_footer.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_heading.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_icons.html.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_list_item_badge.js';
import '//bookmarks-side-panel.top-chrome/shared/sp_shared_style.css.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import '//resources/cr_elements/cr_toolbar/cr_toolbar_selection_overlay.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';

import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from '//bookmarks-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {BookmarkProductInfo} from '//bookmarks-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {SpEmptyStateElement} from '//bookmarks-side-panel.top-chrome/shared/sp_empty_state.js';
import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {CrToolbarSearchFieldElement} from '//resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PluralStringProxyImpl} from '//resources/js/plural_string_proxy.js';
import {listenOnce} from '//resources/js/util.js';
import {IronListElement} from '//resources/polymer/v3_0/iron-list/iron-list.js';
import {afterNextRender, DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionSource, SortOrder, ViewType} from './bookmarks.mojom-webui.js';
import {BookmarksApiProxy, BookmarksApiProxyImpl} from './bookmarks_api_proxy.js';
import {PowerBookmarksContextMenuElement} from './power_bookmarks_context_menu.js';
import {PowerBookmarksDragManager} from './power_bookmarks_drag_manager.js';
import {PowerBookmarksEditDialogElement} from './power_bookmarks_edit_dialog.js';
import {PowerBookmarksLabelsElement} from './power_bookmarks_labels.js';
import {getTemplate} from './power_bookmarks_list.html.js';
import {editingDisabledByPolicy, Label, PowerBookmarksService} from './power_bookmarks_service.js';

const ADD_FOLDER_ACTION_UMA = 'Bookmarks.FolderAddedFromSidePanel';
const ADD_URL_ACTION_UMA = 'Bookmarks.AddedFromSidePanel';

function getBookmarkName(bookmark: chrome.bookmarks.BookmarkTreeNode): string {
  return bookmark.title || bookmark.url || '';
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with
// BookmarksSidePanelSearchCTREvent in tools/metrics/histograms/enums.xml.
export enum SearchAction {
  SHOWN = 0,
  SEARCHED = 1,

  // Must be last.
  COUNT = 2,
}

export interface SortOption {
  sortOrder: SortOrder;
  label: string;
  lowerLabel: string;
}

export interface PowerBookmarksListElement {
  $: {
    bookmarks: HTMLElement,
    contextMenu: PowerBookmarksContextMenuElement,
    deletionToast: CrLazyRenderElement<CrToastElement>,
    powerBookmarksContainer: HTMLElement,
    searchField: CrToolbarSearchFieldElement,
    sortMenu: CrActionMenuElement,
    editDialog: PowerBookmarksEditDialogElement,
    disabledFeatureDialog: CrDialogElement,
    topLevelEmptyState: SpEmptyStateElement,
    folderEmptyState: SpEmptyStateElement,
    heading: HTMLElement,
    footer: HTMLElement,
    labels: PowerBookmarksLabelsElement,
  };
}

interface SectionVisibility {
  search?: boolean;
  labels?: boolean;
  heading?: boolean;
  filterHeadings?: boolean;
  folderEmptyState?: boolean;
  newFolderButton?: boolean;
  bookmarksList?: boolean;
  topLevelEmptyState?: boolean;
  footer?: boolean;
}

export class PowerBookmarksListElement extends PolymerElement {
  static get is() {
    return 'power-bookmarks-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      displayLists_: {
        type: Array,
        value: () => [],
      },

      compact_: {
        type: Boolean,
        value: () => loadTimeData.getInteger('viewType') === 0,
        observer: 'updateListScrollOffset_',
      },

      activeFolderPath_: {
        type: Array,
        value: () => [],
      },

      labels_: {
        type: Array,
        value: () => [],
      },

      activeSortIndex_: {
        type: Number,
        value: () => loadTimeData.getInteger('sortOrder'),
      },

      sortTypes_: {
        type: Array,
        value: () =>
            [{
              sortOrder: SortOrder.kNewest,
              label: loadTimeData.getString('sortNewest'),
              lowerLabel: loadTimeData.getString('sortNewestLower'),
            },
             {
               sortOrder: SortOrder.kOldest,
               label: loadTimeData.getString('sortOldest'),
               lowerLabel: loadTimeData.getString('sortOldestLower'),
             },
             {
               sortOrder: SortOrder.kLastOpened,
               label: loadTimeData.getString('sortLastOpened'),
               lowerLabel: loadTimeData.getString('sortLastOpenedLower'),
             },
             {
               sortOrder: SortOrder.kAlphabetical,
               label: loadTimeData.getString('sortAlphabetically'),
               lowerLabel: loadTimeData.getString('sortAlphabetically'),
             },
             {
               sortOrder: SortOrder.kReverseAlphabetical,
               label: loadTimeData.getString('sortReverseAlphabetically'),
               lowerLabel: loadTimeData.getString('sortReverseAlphabetically'),
             }],
      },

      editing_: {
        type: Boolean,
        value: false,
      },

      selectedBookmarks_: {
        type: Object,
        value: {},
      },

      guestMode_: {
        type: Boolean,
        value: loadTimeData.getBoolean('guestMode'),
        reflectToAttribute: true,
      },

      renamingId_: {
        type: String,
        value: '',
      },

      deletionDescription_: {
        type: String,
        value: '',
      },

      /* If container containing shown bookmarks has scrollbars. */
      hasScrollbars_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      hasLoadedData_: {
        type: Boolean,
        value: false,
      },

      hasSomeActiveFilter_: {
        type: Boolean,
        value: false,
        computed: 'computeHasSomeActiveFilter_(searchQuery_, labels_.*)',
      },

      hasShownBookmarks_: {
        type: Boolean,
        value: false,
        computed: 'computeHasShownBookmarks_(displayLists_.*)',
      },

      canDrag_: {
        type: Boolean,
        value: true,
        computed:
            'computeCanDrag_(editing_, renamingId_, hasSomeActiveFilter_)',
        observer: 'onCanDragChange_',
      },

      sectionVisibility_: {
        type: Object,
        computed: 'computeSectionVisibility_(hasLoadedData_,' +
            'activeFolderPath_.length, hasShownBookmarks_,' +
            'labels_.length, hasSomeActiveFilter_)',
      },
    };
  }

  static get observers() {
    return [
      'updateDisplayLists_(activeFolderPath_.*, labels_.*, ' +
          'activeSortIndex_, searchQuery_)',
    ];
  }

  private bookmarksApi_: BookmarksApiProxy =
      BookmarksApiProxyImpl.getInstance();
  private shoppingListApi_: ShoppingListApiProxy =
      ShoppingListApiProxyImpl.getInstance();
  private shoppingListenerIds_: number[] = [];
  private displayLists_: chrome.bookmarks.BookmarkTreeNode[][];
  private trackedProductInfos_ = new Map<string, BookmarkProductInfo>();
  private availableProductInfos_ = new Map<string, BookmarkProductInfo>();
  private bookmarksService_: PowerBookmarksService =
      new PowerBookmarksService(this);
  private bookmarksDragManager_: PowerBookmarksDragManager =
      new PowerBookmarksDragManager(this);
  private focusOutlineManager_: FocusOutlineManager;
  private compact_: boolean;
  private activeFolderPath_: chrome.bookmarks.BookmarkTreeNode[];
  private labels_: Label[];
  private imageUrls_ = new Map<string, string>();
  private activeSortIndex_: number;
  private sortTypes_: SortOption[];
  private searchQuery_: string|undefined;
  private currentUrl_: string|undefined;
  private editing_: boolean;
  private selectedBookmarks_: {[key: string]: boolean};
  private guestMode_: boolean;
  private renamingId_: string;
  private deletionDescription_: string;
  private shownBookmarksResizeObserver_?: ResizeObserver;
  private hasScrollbars_: boolean;
  private contextMenuBookmark_: chrome.bookmarks.BookmarkTreeNode|undefined;
  private hasLoadedData_: boolean;
  private canDrag_: boolean;
  private hasSomeActiveFilter_: boolean;
  private hasShownBookmarks_: boolean;
  private sectionVisibility_: SectionVisibility = {};
  private shoppingCollectionFolderId_: string;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setAttribute('role', 'application');
    listenOnce(this.$.powerBookmarksContainer, 'dom-change', () => {
      setTimeout(() => this.bookmarksApi_.showUi(), 0);
    });
    this.focusOutlineManager_ = FocusOutlineManager.forDocument(document);
    this.bookmarksService_.startListening();
    this.shoppingListApi_.getAllPriceTrackedBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product => this.set(
              `trackedProductInfos_.${product.bookmarkId.toString()}`,
              product));
    });
    this.shoppingListApi_.getAllShoppingBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product => this.setAvailableProductInfo_(product));
    });
    this.updateShoppingCollectionFolderId_();
    const callbackRouter = this.shoppingListApi_.getCallbackRouter();
    this.shoppingListenerIds_.push(
        callbackRouter.priceTrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceTracked_(product)),
        callbackRouter.priceUntrackedForBookmark.addListener(
            (product: BookmarkProductInfo) =>
                this.onBookmarkPriceUntracked_(product)),
    );

    if (document.documentElement.hasAttribute('chrome-refresh-2023')) {
      this.shownBookmarksResizeObserver_ =
          new ResizeObserver(this.onShownBookmarksResize_.bind(this));
      this.shownBookmarksResizeObserver_.observe(this.$.bookmarks);
    }

    this.updateListScrollOffset_();

    this.bookmarksDragManager_.startObserving();
    this.recordMetricsOnConnected_();
  }

  override disconnectedCallback() {
    this.bookmarksService_.stopListening();
    this.shoppingListenerIds_.forEach(
        id => this.shoppingListApi_.getCallbackRouter().removeListener(id));

    if (this.shownBookmarksResizeObserver_) {
      this.shownBookmarksResizeObserver_.disconnect();
      this.shownBookmarksResizeObserver_ = undefined;
    }

    this.bookmarksDragManager_.stopObserving();
  }

  setCurrentUrl(url: string) {
    this.currentUrl_ = url;
  }

  setImageUrl(bookmark: chrome.bookmarks.BookmarkTreeNode, url: string) {
    this.set(`imageUrls_.${bookmark.id.toString()}`, url);
  }

  onBookmarksLoaded() {
    this.updateDisplayLists_();
    this.hasLoadedData_ = true;
  }

  onBookmarkChanged(id: string, changedInfo: chrome.bookmarks.ChangeInfo) {
    const bookmark = this.bookmarksService_.findBookmarkWithId(id)!;
    if (this.hasSomeActiveFilter_ &&
        (this.bookmarkShouldShow_(bookmark) ||
         this.bookmarkIsShowing_(bookmark))) {
      this.updateDisplayLists_();
    }
    Object.keys(changedInfo).forEach(key => {
      this.notifyPathIfVisible_(id, key);
    });
    this.updateShoppingData_();
  }

  onBookmarkCreated(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      parent: chrome.bookmarks.BookmarkTreeNode) {
    if (this.bookmarkShouldShow_(bookmark)) {
      this.updateShoppingCollectionFolderId_();

      const scrollTop = this.$.bookmarks.scrollTop;
      this.updateDisplayLists_();
      if (bookmark.url) {
        getAnnouncerInstance().announce(loadTimeData.getStringF(
            'bookmarkCreated', getBookmarkName(bookmark)));
      } else {
        getAnnouncerInstance().announce(loadTimeData.getStringF(
            'bookmarkFolderCreated', getBookmarkName(bookmark)));
      }
      for (let i = 0; i < this.displayLists_.length; i++) {
        const indexInList = this.displayLists_[i].indexOf(bookmark);
        if (indexInList > -1) {
          const listElement = this.getDisplayListElement_(i);
          if (listElement &&
              (indexInList < listElement.firstVisibleIndex ||
               indexInList > listElement.lastVisibleIndex)) {
            listElement.scrollToIndex(indexInList);
          } else {
            afterNextRender(this, () => {
              this.$.bookmarks.scrollTop = scrollTop;
            });
          }
          break;
        }
      }
    }
    this.updateShoppingData_();
    this.notifyPathIfVisible_(parent.id, 'children');
  }

  onBookmarkMoved(
      bookmark: chrome.bookmarks.BookmarkTreeNode,
      oldParent: chrome.bookmarks.BookmarkTreeNode,
      newParent: chrome.bookmarks.BookmarkTreeNode) {
    const shouldShow = this.bookmarkShouldShow_(bookmark);
    const isShowing = this.bookmarkIsShowing_(bookmark);
    if (oldParent === newParent && shouldShow) {
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkReordered', getBookmarkName(bookmark)));
    } else if (
        (shouldShow !== isShowing) ||
        (shouldShow && this.hasSomeActiveFilter_)) {
      const scrollTop = this.$.bookmarks.scrollTop;
      this.updateDisplayLists_();
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkMoved', getBookmarkName(bookmark),
          getBookmarkName(newParent)));
      afterNextRender(this, () => {
        this.$.bookmarks.scrollTop = scrollTop;
      });
    }
    // If the new parent folder is visible, notify to ensure its displayed
    // child count is updated.
    this.notifyPathIfVisible_(newParent.id, 'children');
  }

  onBookmarkRemoved(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    const scrollTop = this.$.bookmarks.scrollTop;
    const isShown = this.bookmarkIsShowing_(bookmark);
    if (isShown) {
      this.removeNodeFromDisplayLists_(bookmark.id);
      getAnnouncerInstance().announce(loadTimeData.getStringF(
          'bookmarkDeleted', getBookmarkName(bookmark)));
      afterNextRender(this, () => {
        this.$.bookmarks.scrollTop = scrollTop;
      });
    }

    if (this.shoppingCollectionFolderId_ === bookmark.id) {
      this.shoppingCollectionFolderId_ = '';
    }

    this.set(`trackedProductInfos_.${bookmark.id}`, null);
    this.availableProductInfos_.delete(bookmark.id);

    // If the parent folder is visible, notify to ensure its displayed
    // child count is updated.
    this.notifyPathIfVisible_(bookmark.parentId!, 'children');
  }

  isPriceTracked(bookmark: chrome.bookmarks.BookmarkTreeNode): boolean {
    return !!this.get(`trackedProductInfos_.${bookmark.id}`);
  }

  getProductImageUrl(bookmark: chrome.bookmarks.BookmarkTreeNode): string {
    const bookmarkProductInfo = this.availableProductInfos_.get(bookmark.id);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.imageUrl.url;
    } else {
      return '';
    }
  }

  /** PowerBookmarksDragDelegate */
  getFallbackBookmark(): chrome.bookmarks.BookmarkTreeNode {
    return this.getParentFolder_();
  }

  /** PowerBookmarksDragDelegate */
  getFallbackDropTargetElement(): HTMLElement {
    return this;
  }

  /** PowerBookmarksDragDelegate */
  onFinishDrop(dropTarget: chrome.bookmarks.BookmarkTreeNode): void {
    this.focusBookmark_(dropTarget.id);

    // Show the focus state immediately after dropping a bookmark to indicate
    // where the bookmark was moved to, and remove the state immediately after
    // the next mouse event.
    this.focusOutlineManager_.visible = true;
    document.addEventListener('mousedown', () => {
      this.focusOutlineManager_.visible = false;
    }, {once: true});
  }

  getBookmarkDescriptionForTests(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    return this.getBookmarkDescription_(bookmark);
  }

  clickBookmarkRowForTests(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    const event = new CustomEvent('row-clicked', {
      bubbles: true,
      composed: true,
      detail: {
        bookmark: bookmark,
        event: new MouseEvent('row-clicked'),
      },
    });
    this.onRowClicked_(event);
  }

  setRenamingIdForTests(id: string) {
    const event = new CustomEvent('rename', {
      bubbles: true,
      composed: true,
      detail: {
        id: id,
      },
    });
    this.setRenamingId_(event);
  }

  private notifyPathIfVisible_(id: string, key: string) {
    for (let i = 0; i < this.displayLists_.length; i++) {
      const listIndex = this.displayLists_[i].findIndex(b => b.id === id);
      if (listIndex > -1) {
        this.notifyPath(`displayLists_.${i}.${listIndex}.${key}`);
        return;
      }
    }
  }

  private computeCanDrag_(): boolean {
    return !this.editing_ && !this.renamingId_ && !this.hasSomeActiveFilter_;
  }

  private focusBookmark_(id: string) {
    const bookmarkElement =
        this.shadowRoot!.querySelector(`#bookmark-${id}`) as HTMLElement;
    if (bookmarkElement) {
      bookmarkElement.focus();
    }
  }

  private isPriceTrackingEligible_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return !!this.availableProductInfos_.get(bookmark.id);
  }

  private onBookmarkPriceTracked_(product: BookmarkProductInfo) {
    this.set(`trackedProductInfos_.${product.bookmarkId.toString()}`, product);
  }

  private onBookmarkPriceUntracked_(product: BookmarkProductInfo) {
    this.set(`trackedProductInfos_.${product.bookmarkId.toString()}`, null);
  }

  private bookmarkIsShowing_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return this.displayLists_.some(list => list.includes(bookmark));
  }

  private removeNodeFromDisplayLists_(nodeId: string) {
    for (let listIndex = 0; listIndex < this.displayLists_.length;
         listIndex++) {
      const itemIndex =
          this.displayLists_[listIndex].findIndex(b => b.id === nodeId);
      if (itemIndex > -1) {
        this.splice(`displayLists_.${listIndex}`, itemIndex, 1);
      }
    }
  }

  /**
   * Returns true if the given node is either the current active folder or a
   * root folder that isn't shown itself while the all bookmarks list is shown.
   */
  private visibleParent_(parent: chrome.bookmarks.BookmarkTreeNode): boolean {
    const activeFolder = this.getActiveFolder_();
    return (!activeFolder && parent.parentId === '0' &&
            !this.bookmarkIsShowing_(parent)) ||
        parent === activeFolder;
  }

  private bookmarkShouldShow_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    if (this.hasSomeActiveFilter_) {
      return this.bookmarksService_.bookmarkMatchesSearchQueryAndLabels(
          bookmark, this.labels_, this.searchQuery_);
    }
    return this.visibleParent_(
        this.bookmarksService_.findBookmarkWithId(bookmark.parentId)!);
  }

  private getActiveFolder_(): chrome.bookmarks.BookmarkTreeNode|undefined {
    if (this.activeFolderPath_.length) {
      return this.activeFolderPath_[this.activeFolderPath_.length - 1];
    }
    return undefined;
  }

  private getBackButtonLabel_(): string {
    const activeFolder = this.getActiveFolder_();
    const parentFolder = this.bookmarksService_.findBookmarkWithId(
        activeFolder ? activeFolder.parentId : undefined);
    return loadTimeData.getStringF(
        'backButtonLabel', this.getFolderLabel_(parentFolder));
  }

  private getBookmarksListRole_(): string {
    return this.editing_ ? 'listbox' : 'list';
  }

  private getBookmarkDescription_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string|undefined {
    if (this.compact_) {
      if (bookmark.url) {
        return undefined;
      }
      const count = bookmark.children ? bookmark.children.length : 0;
      return loadTimeData.getStringF('bookmarkFolderChildCount', count);
    } else {
      let urlString;
      if (bookmark.url) {
        const url = new URL(bookmark.url);
        // Show chrome:// if it's a chrome internal url
        if (url.protocol === 'chrome:') {
          urlString = 'chrome://' + url.hostname;
        }
        urlString = url.hostname;
      }
      if (urlString && this.searchQuery_ && bookmark.parentId) {
        const parentFolder =
            this.bookmarksService_.findBookmarkWithId(bookmark.parentId);
        const folderLabel = this.getFolderLabel_(parentFolder);
        return loadTimeData.getStringF(
            'urlFolderDescription', urlString, folderLabel);
      }
      return urlString;
    }
  }

  private getBookmarkDescriptionMeta_(bookmark:
                                          chrome.bookmarks.BookmarkTreeNode) {
    // If there is a price available for the product and it isn't being
    // tracked, return the current price which will be added to the description
    // meta section.
    const productInfo = this.availableProductInfos_.get(bookmark.id);
    if (productInfo && productInfo.info.currentPrice &&
        !this.isPriceTracked(bookmark)) {
      return productInfo.info.currentPrice;
    }

    return '';
  }

  private getViewButtonIcon_() {
    return this.compact_ ? 'bookmarks:compact-view' : 'bookmarks:visual-view';
  }

  private getViewButtonTooltip_() {
    return this.compact_ ? loadTimeData.getString('compactView') :
                           loadTimeData.getString('visualView');
  }

  private getBookmarkMenuA11yLabel_(url: string, title: string): string {
    if (url) {
      return loadTimeData.getStringF('bookmarkMenuLabel', title);
    } else {
      return loadTimeData.getStringF('folderMenuLabel', title);
    }
  }

  private getBookmarkA11yLabel_(id: string, url: string, title: string):
      string {
    if (this.editing_) {
      if (this.get(`selectedBookmarks_.${id}`)) {
        if (url) {
          return loadTimeData.getStringF('deselectBookmarkLabel', title);
        }
        return loadTimeData.getStringF('deselectFolderLabel', title);
      } else {
        if (url) {
          return loadTimeData.getStringF('selectBookmarkLabel', title);
        }
        return loadTimeData.getStringF('selectFolderLabel', title);
      }
    }
    if (url) {
      return loadTimeData.getStringF('openBookmarkLabel', title);
    }
    return loadTimeData.getStringF('openFolderLabel', title);
  }

  private getBookmarkA11yDescription_(
      bookmark: chrome.bookmarks.BookmarkTreeNode): string {
    let description = '';
    if (this.isPriceTracked(bookmark)) {
      description += loadTimeData.getStringF(
          'a11yDescriptionPriceTracking', this.getCurrentPrice_(bookmark));
      const previousPrice = this.getPreviousPrice_(bookmark);
      if (previousPrice) {
        description += loadTimeData.getStringF(
            'a11yDescriptionPriceChange', previousPrice);
      }
    }
    return description;
  }

  private updateShoppingCollectionFolderId_(): void {
    this.shoppingListApi_.getShoppingCollectionBookmarkFolderId().then(res => {
      this.shoppingCollectionFolderId_ = res.collectionId.toString();
    });
  }

  private isShoppingCollection_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return bookmark.id === this.shoppingCollectionFolderId_;
  }

  private getBookmarkImageUrls_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string[] {
    const imageUrls: string[] = [];
    if (bookmark.url) {
      const imageUrl = this.get(`imageUrls_.${bookmark.id.toString()}`);
      if (imageUrl) {
        imageUrls.push(imageUrl);
      }
    } else if (
        this.canEdit_(bookmark) && bookmark.children &&
        !this.isShoppingCollection_(bookmark)) {
      bookmark.children.forEach((child) => {
        const childImageUrl: string =
            this.get(`imageUrls_.${child.id.toString()}`);
        if (childImageUrl) {
          imageUrls.push(childImageUrl);
        }
      });
    }
    return imageUrls;
  }

  private getBookmarkForceHover_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return bookmark === this.contextMenuBookmark_;
  }

  private getActiveFolderLabel_(): string {
    return this.getFolderLabel_(this.getActiveFolder_());
  }

  private getFolderLabel_(folder: chrome.bookmarks.BookmarkTreeNode|
                          undefined): string {
    if (folder && folder.id !== loadTimeData.getString('otherBookmarksId') &&
        folder.id !== loadTimeData.getString('mobileBookmarksId')) {
      return folder!.title;
    } else {
      return loadTimeData.getString('allBookmarks');
    }
  }

  private getSortLabel_(): string {
    return this.sortTypes_[this.activeSortIndex_]!.label;
  }

  private renamingItem_(id: string) {
    return id === this.renamingId_;
  }

  private updateShoppingData_() {
    this.availableProductInfos_.clear();
    this.shoppingListApi_.getAllShoppingBookmarkProductInfo().then(res => {
      res.productInfos.forEach(
          product => this.setAvailableProductInfo_(product));
    });
  }

  private setAvailableProductInfo_(productInfo: BookmarkProductInfo) {
    const bookmarkId = productInfo.bookmarkId.toString();
    this.availableProductInfos_.set(bookmarkId, productInfo);
    if (productInfo.info.imageUrl.url !== '') {
      const bookmark = this.bookmarksService_.findBookmarkWithId(bookmarkId)!;
      this.setImageUrl(bookmark, productInfo.info.imageUrl.url);
    }
  }

  /**
   * Update the lists of bookmarks and folders displayed to the user.
   */
  private updateDisplayLists_() {
    const activeFolder = this.getActiveFolder_();
    const primaryList = this.bookmarksService_.filterBookmarks(
        activeFolder, this.activeSortIndex_, this.searchQuery_, this.labels_);
    this.displayLists_ = [primaryList];
    if (this.hasSomeActiveFilter_ && !!activeFolder) {
      const secondaryList = this.bookmarksService_.filterBookmarks(
          undefined, this.activeSortIndex_, this.searchQuery_, this.labels_,
          activeFolder);
      this.displayLists_.push(secondaryList);
    }
    this.displayLists_.forEach(
        list => this.bookmarksService_.refreshDataForBookmarks(list));
    this.updateListScrollOffset_();
  }

  private updateListScrollOffset_() {
    // Set scrollOffset so the iron-list scrolling accounts for the space the
    // other scrolling UI elements take.
    afterNextRender(this, () => {
      const primaryList = this.getDisplayListElement_(0);
      const secondaryList = this.getDisplayListElement_(1);
      const bookmarksOffsetTop = this.$.bookmarks.offsetTop;
      if (primaryList) {
        primaryList.scrollOffset = primaryList.offsetTop - bookmarksOffsetTop;
      }
      if (secondaryList) {
        secondaryList.scrollOffset =
            secondaryList.offsetTop - bookmarksOffsetTop;
      }
    });
  }

  private onCanDragChange_() {
    if (this.canDrag_) {
      this.bookmarksDragManager_.startObserving();
    } else {
      this.bookmarksDragManager_.stopObserving();
    }
  }

  private recordMetricsOnConnected_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.SortTypeShown',
        this.sortTypes_[this.activeSortIndex_].sortOrder, SortOrder.kCount);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.ViewTypeShown',
        this.compact_ ? ViewType.kCompact : ViewType.kExpanded,
        ViewType.kCount);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.Search.CTR', SearchAction.SHOWN,
        SearchAction.COUNT);
  }

  private canAddCurrentUrl_(): boolean {
    return this.bookmarksService_.canAddUrl(
        this.currentUrl_, this.getActiveFolder_());
  }

  private canEdit_(bookmark: chrome.bookmarks.BookmarkTreeNode): boolean {
    return bookmark.id !== loadTimeData.getString('bookmarksBarId') &&
        bookmark.id !== loadTimeData.getString('managedBookmarksFolderId');
  }

  private getSortMenuItemLabel_(sortType: SortOption): string {
    return loadTimeData.getStringF('sortByType', sortType.label);
  }

  private getSortMenuItemLowerLabel_(sortType: SortOption): string {
    return loadTimeData.getStringF('sortByType', sortType.lowerLabel);
  }

  private sortMenuItemIsSelected_(sortType: SortOption): boolean {
    return this.sortTypes_[this.activeSortIndex_].sortOrder ===
        sortType.sortOrder;
  }

  private bookmarkIsSelected_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    return this.get(`selectedBookmarks_.${bookmark.id.toString()}`);
  }

  /**
   * Invoked when the user clicks a power bookmarks row. This will either
   * display children in the case of a folder row, or open the URL in the case
   * of a bookmark row.
   */
  private onRowClicked_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (!this.editing_) {
      if (event.detail.bookmark.children) {
        this.push('activeFolderPath_', event.detail.bookmark);
        // Cancel search when changing active folder.
        this.$.searchField.setValue('');
        afterNextRender(this, () => {
          for (let i = 0; i < this.displayLists_.length; i++) {
            if (this.displayLists_[i].length > 0) {
              this.getDisplayListElement_(i)!.focusItem(0);
              break;
            }
          }
        });
      } else {
        this.bookmarksApi_.openBookmark(
            event.detail.bookmark.id, this.activeFolderPath_.length, {
              middleButton: event.detail.event.button === 1,
              altKey: event.detail.event.altKey,
              ctrlKey: event.detail.event.ctrlKey,
              metaKey: event.detail.event.metaKey,
              shiftKey: event.detail.event.shiftKey,
            },
            ActionSource.kBookmark);
      }
    }
    // Workaround for this issue, causing unexpected list scrolling when
    // refocusing the list after changing tabs:
    // https://github.com/PolymerElements/iron-list/issues/270
    if (event.target) {
      (event.target as HTMLElement).blur();
    }
  }

  private onRowSelectedChange_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, checked: boolean}>) {
    event.preventDefault();
    event.stopPropagation();
    const isSelected = this.bookmarkIsSelected_(event.detail.bookmark);
    if (event.detail.checked && !isSelected) {
      this.set(
          `selectedBookmarks_.${event.detail.bookmark.id.toString()}`, true);
    } else if (!event.detail.checked && isSelected) {
      this.set(
          `selectedBookmarks_.${event.detail.bookmark.id.toString()}`, false);
    }
  }

  private async onBookmarksEdited_(event: CustomEvent<{
    bookmarks: chrome.bookmarks.BookmarkTreeNode[],
    name: string|undefined,
    url: string|undefined,
    folderId: string,
    newFolders: chrome.bookmarks.BookmarkTreeNode[],
  }>) {
    event.preventDefault();
    event.stopPropagation();
    let parentId = event.detail.folderId;
    for (const folder of event.detail.newFolders) {
      chrome.metricsPrivate.recordUserAction(ADD_FOLDER_ACTION_UMA);
      const newFolder =
          await this.bookmarksApi_.createFolder(folder.parentId!, folder.title);
      folder.children!.forEach(child => child.parentId = newFolder.id);
      if (folder.id === parentId) {
        parentId = newFolder.id;
      }
    }
    this.bookmarksApi_.editBookmarks(
        event.detail.bookmarks.map(bookmark => bookmark.id), event.detail.name,
        event.detail.url, parentId);
    this.selectedBookmarks_ = {};
    this.editing_ = false;
  }

  private setRenamingId_(event: CustomEvent<{id: string}>) {
    this.renamingId_ = event.detail.id;
  }

  private onRename_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, value: string|null}>) {
    const newName = event.detail.value;
    if (newName != null) {
      this.bookmarksApi_.renameBookmark(event.detail.bookmark.id, newName);
    }
    this.renamingId_ = '';
  }

  private getDisplayListElement_(index: number): IronListElement|null {
    return this.shadowRoot!.querySelector<IronListElement>(
        `#shownBookmarksIronList${index}`);
  }

  private notifyBookmarksListResize_() {
    for (let i = 0; i < this.displayLists_.length; i++) {
      if (this.displayLists_[i].length > 0) {
        this.getDisplayListElement_(i)!.notifyResize();
      }
    }
  }

  private getFilterHeading_(index: number) {
    if (index === 0) {
      return loadTimeData.getStringF(
          'primaryFilterHeading', this.getActiveFolderLabel_());
    }
    return loadTimeData.getString('secondaryFilterHeading');
  }

  private getSelectedDescription_() {
    return loadTimeData.getStringF(
        'selectedBookmarkCount', this.getSelectedBookmarksLength_());
  }

  private getSelectedBookmarksList_(): chrome.bookmarks.BookmarkTreeNode[] {
    const selectedEntries = Object.entries(this.selectedBookmarks_)
                                .filter(([_id, selected]) => selected);
    const selectedIds = selectedEntries.map(([id, _selected]) => id);
    return selectedIds.map(
        (id) => this.bookmarksService_.findBookmarkWithId(id)!);
  }

  private getSelectedBookmarksLength_(): number {
    return Object.values(this.selectedBookmarks_)
        .filter((selected) => selected)
        .length;
  }

  /**
   * Toggles the given label between active and inactive.
   */
  private onLabelsChanged_() {
    this.labels_ = [...this.$.labels.labels];
  }

  /**
   * Moves the displayed folders up one level when the back button is clicked.
   */
  private onBackClicked_() {
    this.pop('activeFolderPath_');
  }

  private onSearchChanged_(e: CustomEvent<string>) {
    this.searchQuery_ = e.detail.toLocaleLowerCase();
  }

  private onSearchBlurred_() {
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.Search.CTR', SearchAction.SEARCHED,
        SearchAction.COUNT);
  }

  private onContextMenuShown_(bookmark: chrome.bookmarks.BookmarkTreeNode) {
    this.contextMenuBookmark_ = bookmark;
  }

  private onShowContextMenuClicked_(
      event: CustomEvent<
          {bookmark: chrome.bookmarks.BookmarkTreeNode, event: MouseEvent}>) {
    event.preventDefault();
    event.stopPropagation();
    if (!event.detail.bookmark) {
      return;
    }
    const priceTracked = this.isPriceTracked(event.detail.bookmark);
    const priceTrackingEligible =
        this.isPriceTrackingEligible_(event.detail.bookmark);
    const bookmark = event.detail.bookmark;
    if (event.detail.event.button === 0) {
      this.$.contextMenu.showAt(
          event.detail.event, [bookmark], priceTracked, priceTrackingEligible,
          this.onContextMenuShown_.bind(this, bookmark));
    } else {
      this.$.contextMenu.showAtPosition(
          event.detail.event, [bookmark], priceTracked, priceTrackingEligible,
          this.onContextMenuShown_.bind(this, bookmark));
    }
  }

  private getParentFolder_(): chrome.bookmarks.BookmarkTreeNode {
    return this.getActiveFolder_() ||
        this.bookmarksService_.findBookmarkWithId(
            loadTimeData.getString('otherBookmarksId'))!;
  }

  private onShowSortMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.showAt(event.target as HTMLElement);
  }

  private onAddNewFolderClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const newParent = this.getParentFolder_();
    if (editingDisabledByPolicy([newParent])) {
      this.showDisabledFeatureDialog_();
      return;
    }
    chrome.metricsPrivate.recordUserAction(ADD_FOLDER_ACTION_UMA);
    this.bookmarksApi_
        .createFolder(newParent.id, loadTimeData.getString('newFolderTitle'))
        .then((newFolder) => {
          this.renamingId_ = newFolder.id;
        });
  }

  private onBulkEditClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.editing_ = !this.editing_;
    if (!this.editing_) {
      this.selectedBookmarks_ = {};
    }
  }

  private onDeleteClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const selectedBookmarksList = this.getSelectedBookmarksList_();
    if (editingDisabledByPolicy(selectedBookmarksList)) {
      this.showDisabledFeatureDialog_();
      return;
    }
    this.bookmarksApi_
        .deleteBookmarks(selectedBookmarksList.map((bookmark) => bookmark.id))
        .then(() => {
          this.showDeletionToastWithCount_(selectedBookmarksList.length);
          this.selectedBookmarks_ = {};
          this.editing_ = false;
        });
  }

  private onContextMenuEditClicked_(
      event: CustomEvent<{bookmarks: chrome.bookmarks.BookmarkTreeNode[]}>) {
    event.preventDefault();
    event.stopPropagation();
    if (editingDisabledByPolicy(event.detail.bookmarks)) {
      this.showDisabledFeatureDialog_();
      return;
    }
    this.showEditDialog_(
        event.detail.bookmarks, event.detail.bookmarks.length > 1);
  }

  private onContextMenuDeleteClicked_(
      event: CustomEvent<{bookmarks: chrome.bookmarks.BookmarkTreeNode[]}>) {
    event.preventDefault();
    event.stopPropagation();
    this.showDeletionToastWithCount_(event.detail.bookmarks.length);
    this.selectedBookmarks_ = {};
    this.editing_ = false;
  }

  private onContextMenuClosed_() {
    // This check is needed to avoid the case where the context menu is closed
    // via right-click a new row, and is already re-opened by the time this
    // executes.
    if (!this.$.contextMenu.isOpen()) {
      this.contextMenuBookmark_ = undefined;
    }
  }

  private showDeletionToastWithCount_(deletionCount: number) {
    PluralStringProxyImpl.getInstance()
        .getPluralString('bookmarkDeletionCount', deletionCount)
        .then(pluralString => {
          this.deletionDescription_ = pluralString;
          this.$.deletionToast.get().show();
        });
  }

  private showDisabledFeatureDialog_() {
    this.$.disabledFeatureDialog.showModal();
  }

  private closeDisabledFeatureDialog_() {
    this.$.disabledFeatureDialog.close();
  }

  private onUndoClicked_() {
    this.bookmarksApi_.undo();
    this.$.deletionToast.get().hide();
  }

  private onMoveClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    const selectedBookmarksList = this.getSelectedBookmarksList_();
    if (editingDisabledByPolicy(selectedBookmarksList)) {
      this.showDisabledFeatureDialog_();
      return;
    }
    this.showEditDialog_(selectedBookmarksList, true);
  }

  private showEditDialog_(
      bookmarks: chrome.bookmarks.BookmarkTreeNode[], moveOnly: boolean) {
    this.$.editDialog.showDialog(
        this.activeFolderPath_, this.bookmarksService_.getTopLevelBookmarks(),
        bookmarks, moveOnly);
  }

  private onBulkEditMenuClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.$.contextMenu.showAt(
        event, this.getSelectedBookmarksList_(), false, false);
  }

  private onSortTypeClicked_(event: DomRepeatEvent<SortOption>) {
    event.preventDefault();
    event.stopPropagation();
    this.$.sortMenu.close();
    this.activeSortIndex_ = event.model.index;
    this.bookmarksApi_.setSortOrder(event.model.item.sortOrder);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.SortTypeShown', event.model.item.sortOrder,
        SortOrder.kCount);
  }

  private onViewToggleClicked_(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();
    this.compact_ = !this.compact_;
    this.notifyBookmarksListResize_();
    const viewType = this.compact_ ? ViewType.kCompact : ViewType.kExpanded;
    this.bookmarksApi_.setViewType(viewType);
    chrome.metricsPrivate.recordEnumerationValue(
        'PowerBookmarks.SidePanel.ViewTypeShown', viewType, ViewType.kCount);
  }

  private onAddTabClicked_() {
    const newParent = this.getParentFolder_();
    if (editingDisabledByPolicy([newParent])) {
      this.showDisabledFeatureDialog_();
      return;
    }
    chrome.metricsPrivate.recordUserAction(ADD_URL_ACTION_UMA);
    this.bookmarksApi_.bookmarkCurrentTabInFolder(newParent.id);
  }

  private hideAddTabButton_() {
    return this.editing_ || this.guestMode_;
  }

  private disableBackButton_(): boolean {
    return !this.activeFolderPath_.length || this.editing_;
  }

  private getEmptyTitle_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyTitleGuest');
    } else if (this.hasSomeActiveFilter_) {
      return loadTimeData.getString('emptyTitleSearch');
    } else {
      return loadTimeData.getString('emptyTitle');
    }
  }

  private getEmptyBody_(): string {
    if (this.guestMode_) {
      return loadTimeData.getString('emptyBodyGuest');
    } else if (this.hasSomeActiveFilter_) {
      return loadTimeData.getString('emptyBodySearch');
    } else {
      return loadTimeData.getString('emptyBody');
    }
  }

  private getEmptyImagePath_(): string {
    return this.hasSomeActiveFilter_ ? '' : './images/bookmarks_empty.svg';
  }

  private getEmptyImagePathDark_(): string {
    return this.hasSomeActiveFilter_ ? '' : './images/bookmarks_empty_dark.svg';
  }

  private computeHasSomeActiveFilter_(): boolean {
    return !!this.searchQuery_ || this.labels_.some(label => label.active);
  }

  private computeHasShownBookmarks_(): boolean {
    return this.displayLists_.some((list) => list.length > 0);
  }

  private computeSectionVisibility_(): SectionVisibility {
    if (this.guestMode_) {
      return {topLevelEmptyState: true};
    }

    if (!this.hasLoadedData_) {
      return {search: true, footer: true};
    }

    const hasActiveFolder = this.activeFolderPath_.length > 0;
    const hasShownBookmarks = this.hasShownBookmarks_;
    const hasSomeActiveFilter = this.hasSomeActiveFilter_;

    return {
      search: true,
      labels: this.labels_.length > 0,
      heading: !hasSomeActiveFilter && (hasActiveFolder || hasShownBookmarks),
      filterHeadings: hasSomeActiveFilter,
      folderEmptyState:
          !hasShownBookmarks && !hasSomeActiveFilter && hasActiveFolder,
      newFolderButton: !hasSomeActiveFilter,
      bookmarksList: hasShownBookmarks,
      topLevelEmptyState:
          !hasShownBookmarks && (hasSomeActiveFilter || !hasActiveFolder),
      footer: !hasSomeActiveFilter,
    };
  }

  /**
   * Whether the given price-tracked bookmark should display as if discounted.
   */
  private showDiscountedPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      boolean {
    const bookmarkProductInfo = this.get(`trackedProductInfos_.${bookmark.id}`);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice.length > 0;
    }
    return false;
  }

  private getCurrentPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.get(`trackedProductInfos_.${bookmark.id}`);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.currentPrice;
    } else {
      return '';
    }
  }

  private getPreviousPrice_(bookmark: chrome.bookmarks.BookmarkTreeNode):
      string {
    const bookmarkProductInfo = this.get(`trackedProductInfos_.${bookmark.id}`);
    if (bookmarkProductInfo) {
      return bookmarkProductInfo.info.previousPrice;
    } else {
      return '';
    }
  }

  private onShownBookmarksResize_() {
    // The iron-lists of `displayLists_` are in a dynamically sized card.
    // Any time the size changes, let iron-list know so that iron-list can
    // properly adjust to its possibly new height.
    this.notifyBookmarksListResize_();

    this.hasScrollbars_ =
        this.$.bookmarks.scrollHeight > this.$.bookmarks.offsetHeight;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-list': PowerBookmarksListElement;
  }
}

customElements.define(PowerBookmarksListElement.is, PowerBookmarksListElement);
