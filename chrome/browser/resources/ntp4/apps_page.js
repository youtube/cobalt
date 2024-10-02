// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {$, appendParam} from 'chrome://resources/js/util_ts.js';

import {AppInfo} from './app_info.js';
import {contextMenuHandler} from './context_menu_handler.js';
import {addSingletonGetter} from './cr_deprecated.js';
import {Menu} from './menu.js';
import {MenuItem} from './menu_item.js';
import {getAppsPageIndex, getCardSlider} from './new_tab.js';
import {getCurrentlyDraggingTile, setCurrentDropEffect, TilePage} from './tile_page.js';
import {decorate, toCssPx} from './ui.js';
import {findAncestorByClass} from './util.js';



export const APP_LAUNCH = {
  // The histogram buckets (keep in sync with extension_constants.h).
  NTP_APPS_MAXIMIZED: 0,
  NTP_APPS_COLLAPSED: 1,
  NTP_APPS_MENU: 2,
  NTP_MOST_VISITED: 3,
  NTP_APP_RE_ENABLE: 16,
  NTP_WEBSTORE_FOOTER: 18,
  NTP_WEBSTORE_PLUS_ICON: 19,
};

// Histogram buckets for UMA tracking of where a DnD drop came from.
const DRAG_SOURCE = {
  SAME_APPS_PANE: 0,
  OTHER_APPS_PANE: 1,
  MOST_VISITED_PANE: 2,  // Deprecated.
  BOOKMARKS_PANE: 3,     // Deprecated.
  OUTSIDE_NTP: 4,
};
const DRAG_SOURCE_LIMIT = DRAG_SOURCE.OUTSIDE_NTP + 1;

// Run on OS Login available modes
const RUN_ON_OS_LOGIN_MODE = {
  NOT_RUN: 'run_on_os_login_mode_not_run',
  WINDOWED: 'run_on_os_login_mode_windowed',
  MINIMIZED: 'run_on_os_login_mode_minimized',
};

// The fraction of the app tile size that the icon uses.
const APP_IMG_SIZE_FRACTION = 4 / 5;

/**
 * This policy maps a given string to a `TrustedHTML` object
 * without performing any validation. Callsites must ensure
 * that the resulting object will only be used in inert
 * documents.
 *
 * @type {!TrustedTypePolicy}
 */
const unsanitizedPolicy = trustedTypes.createPolicy(
    'apps-page-js', {createHTML: untrustedHTML => untrustedHTML});

/**
 * App context menu. The class is designed to be used as a singleton with
 * the app that is currently showing a context menu stored in this.app_.
 * @constructor
 */
function AppContextMenu() {
  this.__proto__ = AppContextMenu.prototype;
  this.initialize();
}
addSingletonGetter(AppContextMenu);

AppContextMenu.prototype = {
  initialize() {
    const menu = new Menu();
    decorate(menu, Menu);
    menu.classList.add('app-context-menu');
    this.menu = menu;

    this.launch_ = this.appendMenuItem_();
    this.launch_.addEventListener('activate', this.onActivate_.bind(this));

    menu.appendChild(MenuItem.createSeparator());
    this.launchRegularTab_ = this.appendMenuItem_('applaunchtyperegular');
    this.launchPinnedTab_ = this.appendMenuItem_('applaunchtypepinned');
    this.launchNewWindow_ = this.appendMenuItem_('applaunchtypewindow');
    this.launchFullscreen_ = this.appendMenuItem_('applaunchtypefullscreen');

    const self = this;
    this.forAllLaunchTypes_(function(launchTypeButton, id) {
      launchTypeButton.addEventListener(
          'activate', self.onLaunchTypeChanged_.bind(self));
    });

    this.runOnOsLogin_ = this.appendMenuItem_('runonoslogin');
    this.runOnOsLogin_.addEventListener(
        'activate', this.onRunOnOsLoginModeChanged_.bind(this));

    this.launchTypeMenuSeparator_ = MenuItem.createSeparator();
    menu.appendChild(this.launchTypeMenuSeparator_);
    this.options_ = this.appendMenuItem_('appoptions');
    this.uninstall_ = this.appendMenuItem_('appuninstall');

    this.appinfo_ = this.appendMenuItem_('appinfodialog');
    this.appinfo_.addEventListener('activate', this.onShowAppInfo_.bind(this));
    if (!loadTimeData.getBoolean('canShowAppInfoDialog')) {
      this.details_ = this.appendMenuItem_('appdetails');
      this.details_.addEventListener(
          'activate', this.onShowDetails_.bind(this));
    }

    this.options_.addEventListener('activate', this.onShowOptions_.bind(this));
    this.uninstall_.addEventListener('activate', this.onUninstall_.bind(this));

    this.createShortcutSeparator_ =
        menu.appendChild(MenuItem.createSeparator());
    this.createShortcut_ = this.appendMenuItem_('appcreateshortcut');
    this.createShortcut_.addEventListener(
        'activate', this.onCreateShortcut_.bind(this));

    this.installLocallySeparator_ =
        menu.appendChild(MenuItem.createSeparator());
    this.installLocally_ = this.appendMenuItem_('appinstalllocally');
    this.installLocally_.addEventListener(
        'activate', this.onInstallLocally_.bind(this));

    document.body.appendChild(menu);
  },

  /**
   * Appends a menu item to |this.menu|.
   * @param {string=} opt_textId If defined, the ID for the localized string
   *     that acts as the item's label.
   * @private
   */
  appendMenuItem_(opt_textId) {
    const button =
        /** @type {!HTMLButtonElement} */ (document.createElement('button'));
    this.menu.appendChild(button);
    decorate(button, MenuItem);
    if (opt_textId) {
      button.textContent = loadTimeData.getString(opt_textId);
    }
    return button;
  },

  /**
   * Iterates over all the launch type menu items.
   * @param {function(MenuItem, number)} f The function to call for each
   *     menu item. The parameters to the function include the menu item and
   *     the associated launch ID.
   * @private
   */
  forAllLaunchTypes_(f) {
    // Order matters: index matches launchType id.
    const launchTypes = [
      this.launchPinnedTab_,
      this.launchRegularTab_,
      this.launchFullscreen_,
      this.launchNewWindow_,
    ];

    for (let i = 0; i < launchTypes.length; ++i) {
      if (!launchTypes[i]) {
        continue;
      }

      f(launchTypes[i], i);
    }
  },

  /**
   * Does all the necessary setup to show the menu for the given app.
   * @param {App} app The App object that will be showing a context menu.
   */
  setupForApp(app) {
    this.app_ = app;

    this.launch_.textContent = app.appData.title;

    const launchTypeWindow = this.launchNewWindow_;
    let hasLaunchType = false;
    this.forAllLaunchTypes_(function(launchTypeButton, id) {
      launchTypeButton.disabled = false;
      launchTypeButton.checked = app.appData.launch_type === id;
      // There are three cases when a launch type is hidden:
      //  1. type is anything except |launchTypeWindow| or
      //  2. if the launch type can't be changed or
      //  3. if the launch type is hidden.
      launchTypeButton.hidden = launchTypeButton !== launchTypeWindow ||
          !app.appData.mayChangeLaunchType || app.appData.hideDisplayMode;

      if (!launchTypeButton.hidden) {
        hasLaunchType = true;
      }
    });

    this.launchTypeMenuSeparator_.hidden =
        !app.appData.mayChangeLaunchType || !hasLaunchType;

    this.options_.disabled = !app.appData.optionsUrl || !app.appData.enabled;
    this.options_.hidden = app.appData.optionsUrl === undefined;

    this.uninstall_.disabled = !app.appData.mayDisable;

    this.appinfo_.textContent = '';
    if (app.appData.settingsMenuItemOverrideText) {
      this.appinfo_.textContent = app.appData.settingsMenuItemOverrideText;
    } else if (
        loadTimeData.getBoolean('canShowAppInfoDialog') &&
        app.appData.isLocallyInstalled) {
      this.appinfo_.textContent = loadTimeData.getString('appinfodialog');
    }
    this.appinfo_.hidden = !this.appinfo_.textContent;
    if (this.details_) {
      this.details_.hidden = !this.appinfo_.hidden;
      this.details_.disabled = !app.appData.detailsUrl;
    }

    this.createShortcutSeparator_.hidden = this.createShortcut_.hidden =
        !app.appData.mayCreateShortcuts;

    this.installLocallySeparator_.hidden = this.installLocally_.hidden =
        app.appData.isLocallyInstalled;

    this.runOnOsLogin_.hidden = !app.appData.mayShowRunOnOsLoginMode;
    this.runOnOsLogin_.disabled = !app.appData.mayToggleRunOnOsLoginMode;
    this.runOnOsLogin_.checked =
        app.appData.runOnOsLoginMode !== RUN_ON_OS_LOGIN_MODE.NOT_RUN;
  },

  /** @private */
  onActivate_() {
    chrome.send('launchApp', [this.app_.appId, APP_LAUNCH.NTP_APPS_MENU]);
  },

  /**
   * @param {Event} e
   * @private
   */
  onLaunchTypeChanged_(e) {
    const pressed = e.currentTarget;
    const app = this.app_;
    let targetLaunchType = pressed;
    // When bookmark apps are enabled, hosted apps can only toggle between
    // open as window and open as tab.
    targetLaunchType = this.launchNewWindow_.checked ? this.launchRegularTab_ :
                                                       this.launchNewWindow_;
    this.forAllLaunchTypes_(function(launchTypeButton, id) {
      if (launchTypeButton === targetLaunchType) {
        chrome.send('setLaunchType', [app.appId, id]);
        // Manually update the launch type. We will only get
        // appsPrefChangeCallback calls after changes to other NTP instances.
        app.appData.launch_type = id;
      }
    });
  },

  /** @private */
  onShowOptions_() {
    window.location = this.app_.appData.optionsUrl;
  },

  /** @private */
  onShowDetails_() {
    let url = this.app_.appData.detailsUrl;
    url = appendParam(url, 'utm_source', 'chrome-ntp-launcher');
    window.location = url;
  },

  /** @private */
  onUninstall_() {
    chrome.send('uninstallApp', [this.app_.appData.id]);
  },

  /** @private */
  onCreateShortcut_() {
    chrome.send('createAppShortcut', [this.app_.appData.id]);
  },

  /** @private */
  onInstallLocally_() {
    chrome.send('installAppLocally', [this.app_.appData.id]);
  },

  /** @private */
  onShowAppInfo_() {
    chrome.send('showAppInfo', [this.app_.appData.id]);
  },

  /** @private */
  onRunOnOsLoginModeChanged_(e) {
    const pressed = e.currentTarget;
    const app = this.app_;
    let mode = RUN_ON_OS_LOGIN_MODE.NOT_RUN;

    if (pressed === this.runOnOsLogin_ && !pressed.checked) {
      mode = RUN_ON_OS_LOGIN_MODE.WINDOWED;
    }

    chrome.send('runOnOsLogin', [app.appData.id, mode]);
  },
};

/**
 * Creates a new App object.
 * @param {Object} appData The data object that describes the app.
 * @constructor
 * @extends {HTMLDivElement}
 */
export function App(appData) {
  const el = /** @type {!App} */ (document.createElement('div'));
  el.__proto__ = App.prototype;
  el.initialize(appData);

  return el;
}

App.prototype = {
  __proto__: HTMLDivElement.prototype,

  /**
   * Initialize the app object.
   * @param {Object} appData The data object that describes the app.
   *
   * TODO(crbug.com/425829): This function makes use of deprecated getter or
   * setter functions.
   * @suppress {deprecated}
   */
  initialize(appData) {
    this.appData = appData;
    assert(this.appData_.id, 'Got an app without an ID');
    this.id = this.appData_.id;
    this.setAttribute('role', 'menuitem');
    this.setAttribute('aria-label', this.appData_.full_name);

    this.className = 'app focusable';

    if (!this.appData_.icon_big_exists && this.appData_.icon_small_exists) {
      this.useSmallIcon_ = true;
    }

    this.appContents_ = (this.useSmallIcon_ ? $('app-small-icon-template') :
                                              $('app-large-icon-template'))
                            .cloneNode(true);
    this.appContents_.id = '';
    this.appendChild(this.appContents_);

    this.appImgContainer_ =
        /** @type {HTMLElement} */ (this.querySelector('.app-img-container'));
    this.appImg_ = this.appImgContainer_.querySelector('img');
    this.setIcon();

    if (this.useSmallIcon_) {
      this.imgDiv_ =
          /** @type {HTMLElement} */ (this.querySelector('.app-icon-div'));
      this.addLaunchClickTarget_(this.imgDiv_);
      this.imgDiv_.title = this.appData_.full_name;
      chrome.send('getAppIconDominantColor', [this.id]);
    } else {
      this.addLaunchClickTarget_(this.appImgContainer_);
      this.appImgContainer_.title = this.appData_.full_name;
    }

    // The app's full name is shown in the tooltip, whereas the short name
    // is used for the label.
    const appSpan =
        /** @type {HTMLElement} */ (this.appContents_.querySelector('.title'));
    appSpan.textContent = this.appData_.title;
    appSpan.title = this.appData_.full_name;
    this.addLaunchClickTarget_(
        /** @type {HTMLElement} */
        (this.querySelector('.app-title-container')));

    if (this.appData_.is_deprecated_app) {
      this.classList.add('deprecated');
    }

    this.addEventListener('keydown', contextMenuHandler);
    this.addEventListener('keyup', contextMenuHandler);

    // This hack is here so that appContents.contextMenu will be the same as
    // this.contextMenu.
    const self = this;

    // TODO(crbug.com/425829): Remove above suppression once we no longer use
    // deprecated function defineGetter.
    // eslint-disable-next-line no-restricted-properties
    this.appContents_.__defineGetter__('contextMenu', function() {
      return self.contextMenu;
    });

    if (!this.appData_.kioskMode) {
      this.appContents_.addEventListener('contextmenu', contextMenuHandler);
    }

    this.addEventListener('mousedown', this.onMousedown_, true);
    this.addEventListener('keydown', this.onKeydown_);
    this.addEventListener('blur', this.onBlur_);
  },

  /**
   * Sets the color of the favicon dominant color bar.
   * @param {string} color The css-parsable value for the color.
   */
  set stripeColor(color) {
    this.querySelector('.color-stripe').style.backgroundColor = color;
  },

  /**
   * Removes the app tile from the page. Should be called after the app has
   * been uninstalled.
   *
   * TODO(dbeam): this method now conflicts with HTMLElement#remove(), which
   * is why the param is optional. Rename.
   *
   * @param {boolean=} opt_animate Whether the removal should be animated.
   */
  remove(opt_animate) {
    // Unset the ID immediately, because the app is already gone. But leave
    // the tile on the page as it animates out.
    this.id = '';
    this.tile.doRemove(opt_animate);
  },

  /**
   * Set the URL of the icon from |appData_|. This won't actually show the
   * icon until loadIcon() is called (for performance reasons; we don't want
   * to load icons until we have to).
   */
  setIcon() {
    let src =
        this.useSmallIcon_ ? this.appData_.icon_small : this.appData_.icon_big;
    if (!this.appData_.enabled || !this.appData_.isLocallyInstalled ||
        (!this.appData_.offlineEnabled && !navigator.onLine)) {
      src += '?grayscale=true';
    }

    this.appImgSrc_ = src;
    this.classList.add('icon-loading');
  },

  /**
   * Shows the icon for the app. That is, it causes chrome to load the app
   * icon resource.
   */
  loadIcon() {
    if (this.appImgSrc_) {
      this.appImg_.src = this.appImgSrc_;
      this.appImg_.classList.remove('invisible');
      this.appImgSrc_ = null;
    }

    this.classList.remove('icon-loading');
  },

  /**
   * Set the size and position of the app tile.
   * @param {number} size The total size of |this|.
   * @param {number} x The x-position.
   * @param {number} y The y-position.
   *     animate.
   */
  setBounds(size, x, y) {
    const imgSize = size * APP_IMG_SIZE_FRACTION;
    this.appImgContainer_.style.width = this.appImgContainer_.style.height =
        toCssPx(this.useSmallIcon_ ? 16 : imgSize);
    if (this.useSmallIcon_) {
      // 3/4 is the ratio of 96px to 128px (the used height and full height
      // of icons in apps).
      const iconSize = imgSize * 3 / 4;
      // The -2 is for the div border to improve the visual alignment for the
      // icon div.
      this.imgDiv_.style.width = this.imgDiv_.style.height =
          toCssPx(iconSize - 2);
      // Margins set to get the icon placement right and the text to line up.
      this.imgDiv_.style.marginTop = this.imgDiv_.style.marginBottom =
          toCssPx((imgSize - iconSize) / 2);
    }

    this.style.width = this.style.height = toCssPx(size);
    this.style.left = toCssPx(x);
    this.style.right = toCssPx(x);
    this.style.top = toCssPx(y);
  },

  /** @private */
  onBlur_() {
    this.classList.remove('click-focus');
    this.appContents_.classList.remove('suppress-active');
  },

  /**
   * Invoked when an app is clicked.
   * @param {Event} e The click/auxclick event.
   * @private
   */
  onClick_(e) {
    if (/** @type {MouseEvent} */ (e).button > 1) {
      return;
    }

    chrome.send('launchApp', [
      this.appId,
      APP_LAUNCH.NTP_APPS_MAXIMIZED,
      'chrome-ntp-icon',
      e.button,
      e.altKey,
      e.ctrlKey,
      e.metaKey,
      e.shiftKey,
    ]);

    // Don't allow the click to trigger a link or anything
    e.preventDefault();
  },

  /**
   * Invoked when the user presses a key while the app is focused.
   * @param {Event} e The key event.
   * @private
   */
  onKeydown_(e) {
    if (e.key === 'F10' && e.shiftKey) {
      this.appContents_.dispatchEvent(new MouseEvent('contextmenu'));
      e.preventDefault();
      e.stopPropagation();
    } else if (e.key === 'Enter') {
      chrome.send('launchApp', [
        this.appId,
        APP_LAUNCH.NTP_APPS_MAXIMIZED,
        '',
        0,
        e.altKey,
        e.ctrlKey,
        e.metaKey,
        e.shiftKey,
      ]);
      e.preventDefault();
      e.stopPropagation();
    }
  },

  /**
   * Adds a node to the list of targets that will launch the app. This list
   * is also used in onMousedown to determine whether the app contents should
   * be shown as active (if we don't do this, then clicking anywhere in
   * appContents, even a part that is outside the ideally clickable region,
   * will cause the app icon to look active).
   * @param {HTMLElement} node The node that should be clickable.
   * @private
   */
  addLaunchClickTarget_(node) {
    node.classList.add('launch-click-target');
    node.addEventListener('click', this.onClick_.bind(this));
    node.addEventListener('auxclick', this.onClick_.bind(this));
  },

  /**
   * Handler for mousedown on the App. Adds a class that allows us to
   * not display as :active for right clicks (specifically, don't pulse on
   * these occasions). Also, we don't pulse for clicks that aren't within the
   * clickable regions.
   * @param {Event} e The mousedown event.
   * @private
   */
  onMousedown_(e) {
    // If the current platform uses middle click to autoscroll and this
    // mousedown isn't handled, onClick_() will never fire. crbug.com/142939
    if (e.button === 1) {
      e.preventDefault();
    }

    if (e.button === 2 ||
        !findAncestorByClass(
            /** @type {Element} */ (e.target), 'launch-click-target')) {
      this.appContents_.classList.add('suppress-active');
    } else {
      this.appContents_.classList.remove('suppress-active');
    }

    // This class is here so we don't show the focus state for apps that
    // gain keyboard focus via mouse clicking.
    this.classList.add('click-focus');
  },

  /**
   * Change the appData and update the appearance of the app.
   * @param {AppInfo} appData The new data object that describes the app.
   */
  replaceAppData(appData) {
    this.appData_ = appData;
    this.setIcon();
    this.loadIcon();
  },

  /**
   * The data and preferences for this app.
   * @type {Object}
   */
  set appData(data) {
    this.appData_ = data;
  },
  get appData() {
    return this.appData_;
  },

  /** @type {string} */
  get appId() {
    return this.appData_.id;
  },

  /**
   * Returns a pointer to the context menu for this app. All apps share the
   * singleton AppContextMenu. This function is called by the
   * ContextMenuHandler in response to the 'contextmenu' event.
   * @type {Menu}
   */
  get contextMenu() {
    const menu = AppContextMenu.getInstance();
    menu.setupForApp(this);
    return menu.menu;
  },

  /**
   * Returns whether this element can be 'removed' from chrome (i.e. whether
   * the user can drag it onto the trash and expect something to happen).
   * @return {boolean} True if the app can be uninstalled.
   */
  canBeRemoved() {
    return this.appData_.mayDisable;
  },

  /**
   * Uninstalls the app after it's been dropped on the trash.
   */
  removeFromChrome() {
    chrome.send('uninstallApp', [this.appData_.id, true]);
    this.tile.tilePage.removeTile(this.tile, true);
  },

  /**
   * Called when a drag is starting on the tile. Updates dataTransfer with
   * data for this tile.
   */
  setDragData(dataTransfer) {
    dataTransfer.setData('Text', this.appData_.title);
    dataTransfer.setData('URL', this.appData_.url);
  },
};

const appsPageGridValues = {
  // The fewest tiles we will show in a row.
  minColCount: 3,
  // The most tiles we will show in a row.
  maxColCount: 6,

  // The smallest a tile can be.
  minTileWidth: 64 / APP_IMG_SIZE_FRACTION,
  // The biggest a tile can be.
  maxTileWidth: 128 / APP_IMG_SIZE_FRACTION,

  // The padding between tiles, as a fraction of the tile width.
  tileSpacingFraction: 1 / 8,
};
TilePage.initGridValues(appsPageGridValues);

/**
 * Creates a new AppsPage object.
 * @constructor
 * @extends {TilePage}
 */
export function AppsPage() {
  const el = new TilePage(appsPageGridValues);
  el.__proto__ = AppsPage.prototype;
  el.initialize();

  return el;
}

AppsPage.prototype = {
  __proto__: TilePage.prototype,

  initialize() {
    this.classList.add('apps-page');

    this.addEventListener('cardselected', this.onCardSelected_);

    this.addEventListener('tilePage:tile_added', this.onTileAdded_);

    this.content_.addEventListener('scroll', this.onScroll.bind(this));
  },

  /**
   * Highlight a newly installed app as it's added to the NTP.
   * @param {AppInfo} appData The data object that describes the app.
   */
  insertAndHighlightApp(appData) {
    getCardSlider().selectCardByValue(this);
    this.content_.scrollTop = this.content_.scrollHeight;
    this.insertApp(appData, true);
  },

  /**
   * Similar to appendApp, but it respects the app_launch_ordinal field of
   * |appData|.
   * @param {Object} appData The data that describes the app.
   * @param {boolean} animate Whether to animate the insertion.
   */
  insertApp(appData, animate) {
    let index = this.tileElements_.length;
    for (let i = 0; i < this.tileElements_.length; i++) {
      if (appData.app_launch_ordinal <
          this.tileElements_[i].firstChild.appData.app_launch_ordinal) {
        index = i;
        break;
      }
    }

    this.addTileAt(new App(appData), index, animate);
  },

  /**
   * Handler for 'cardselected' event, fired when |this| is selected. The
   * first time this is called, we load all the app icons.
   * @private
   */
  onCardSelected_() {
    const apps = /** @type {NodeList<App>} */ (
        this.querySelectorAll('.app.icon-loading'));
    for (let i = 0; i < apps.length; i++) {
      apps[i].loadIcon();
    }
  },

  /**
   * Handler for tile additions to this page.
   * @param {Event} e The tilePage:tile_added event.
   * @private
   */
  onTileAdded_(e) {
    assert(e.currentTarget === this);
    assert(e.addedTile.firstChild instanceof App);
    if (this.classList.contains('selected-card')) {
      e.addedTile.firstChild.loadIcon();
    }
  },

  /**
   * A handler for when the apps page is scrolled (then we need to reposition
   * the bubbles.
   * @override
   */
  onScroll() {
    if (!this.selected) {
      return;
    }
    for (let i = 0; i < this.tileElements_.length; i++) {
      const app = this.tileElements_[i].firstChild;
      assert(app instanceof App);
    }
  },

  /** @override */
  doDragOver(e) {
    // Only animatedly re-arrange if the user is currently dragging an app.
    const tile = getCurrentlyDraggingTile();
    if (tile && tile.querySelector('.app')) {
      TilePage.prototype.doDragOver.call(this, e);
    } else {
      e.preventDefault();
      this.setDropEffect(e.dataTransfer);
    }
  },

  /** @override */
  shouldAcceptDrag(e) {
    if (getCurrentlyDraggingTile()) {
      return true;
    }
    if (!e.dataTransfer || !e.dataTransfer.types) {
      return false;
    }
    return Array.prototype.indexOf.call(
               e.dataTransfer.types, 'text/uri-list') !== -1;
  },

  /** @override */
  addDragData(dataTransfer, index) {
    let sourceId = -1;
    const currentlyDraggingTile = getCurrentlyDraggingTile();
    if (currentlyDraggingTile) {
      const tileContents = currentlyDraggingTile.firstChild;
      if (tileContents.classList.contains('app')) {
        const originalPage = currentlyDraggingTile.tilePage;
        const samePageDrag = originalPage === this;
        sourceId = samePageDrag ? DRAG_SOURCE.SAME_APPS_PANE :
                                  DRAG_SOURCE.OTHER_APPS_PANE;
        this.tileGrid_.insertBefore(
            currentlyDraggingTile, this.tileElements_[index]);
        this.tileMoved(currentlyDraggingTile);
        if (!samePageDrag) {
          originalPage.fireRemovedEvent(currentlyDraggingTile, index, true);
          this.fireAddedEvent(currentlyDraggingTile, index, true);
        }
      }
    } else {
      this.addOutsideData_(dataTransfer);
      sourceId = DRAG_SOURCE.OUTSIDE_NTP;
    }

    assert(sourceId !== -1);
    chrome.send(
        'metricsHandler:recordInHistogram',
        ['NewTabPage.AppsPageDragSource', sourceId, DRAG_SOURCE_LIMIT]);
  },

  /**
   * Adds drag data that has been dropped from a source that is not a tile.
   * @param {Object} dataTransfer The data transfer object that holds drop
   *     data.
   * @private
   */
  addOutsideData_(dataTransfer) {
    const url = dataTransfer.getData('url');
    assert(url);

    // If the dataTransfer has html data, use that html's text contents as the
    // title of the new link.
    const html = dataTransfer.getData('text/html');
    let title;
    if (html) {
      // It's important that we don't attach this node to the document
      // because it might contain scripts.
      const doc = document.implementation.createHTMLDocument();
      doc.body.innerHTML = unsanitizedPolicy.createHTML(html);
      title = doc.body.textContent;
    }

    // Make sure title is >=1 and <=45 characters for Chrome app limits.
    if (!title) {
      title = url;
    }
    if (title.length > 45) {
      title = title.substring(0, 45);
    }
    const data = {url: url, title: title};

    // Synthesize an app.
    this.generateAppForLink(data);
  },

  /**
   * Creates a new crx-less app manifest and installs it.
   * @param {Object} data The data object describing the link. Must have |url|
   *     and |title| members.
   */
  generateAppForLink(data) {
    assert(data.url !== undefined);
    assert(data.title !== undefined);
    const pageIndex = getAppsPageIndex(this);
    chrome.send('generateAppForLink', [data.url, data.title, pageIndex]);
  },

  /** @override */
  tileMoved(draggedTile) {
    if (!(draggedTile.firstChild instanceof App)) {
      return;
    }

    const pageIndex = getAppsPageIndex(this);
    chrome.send('setPageIndex', [draggedTile.firstChild.appId, pageIndex]);

    const appIds = [];
    for (let i = 0; i < this.tileElements_.length; i++) {
      const tileContents = this.tileElements_[i].firstChild;
      if (tileContents instanceof App) {
        appIds.push(tileContents.appId);
      }
    }

    chrome.send('reorderApps', [draggedTile.firstChild.appId, appIds]);
  },

  /** @override */
  setDropEffect(dataTransfer) {
    const tile = getCurrentlyDraggingTile();
    if (tile && tile.querySelector('.app')) {
      setCurrentDropEffect(dataTransfer, 'move');
    } else {
      setCurrentDropEffect(dataTransfer, 'copy');
    }
  },
};

/**
 * Launches the specified app using the APP_LAUNCH_NTP_APP_RE_ENABLE
 * histogram. This should only be invoked from the AppLauncherHandler.
 * @param {string} appId The ID of the app.
 */
function launchAppAfterEnable(appId) {
  chrome.send('launchApp', [appId, APP_LAUNCH.NTP_APP_RE_ENABLE]);
}
window['ntp'] = window['ntp'] || {};
window['ntp']['launchAppAfterEnable'] = launchAppAfterEnable;
