// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The ChromeVox panel and menus.
 */
import {BrowserUtil} from '../../common/browser_util.js';
import {constants} from '../../common/constants.js';
import {LocalStorage} from '../../common/local_storage.js';
import {StringUtil} from '../../common/string_util.js';
import {BackgroundBridge} from '../common/background_bridge.js';
import {BridgeConstants} from '../common/bridge_constants.js';
import {BridgeHelper} from '../common/bridge_helper.js';
import {Command} from '../common/command.js';
import {CommandStore} from '../common/command_store.js';
import {EventSourceType} from '../common/event_source_type.js';
import {GestureCommandData} from '../common/gesture_command_data.js';
import {LocaleOutputHelper} from '../common/locale_output_helper.js';
import {Msgs} from '../common/msgs.js';
import {PanelCommand, PanelCommandType} from '../common/panel_command.js';
import {ALL_PANEL_MENU_NODE_DATA} from '../common/panel_menu_data.js';
import {SettingsManager} from '../common/settings_manager.js';
import {QueueMode} from '../common/tts_types.js';

import {ISearchUI} from './i_search_ui.js';
import {MenuManager} from './menu_manager.js';
import {PanelInterface} from './panel_interface.js';
import {PanelMode, PanelModeInfo} from './panel_mode.js';

const $ = (id) => document.getElementById(id);

/** Class to manage the panel. */
export class Panel extends PanelInterface {
  /**
   * @private
   */
  constructor() {
    super();
    /** @private {!PanelMode} */
    this.mode_ = PanelMode.COLLAPSED;

    /** @private {!MenuManager} */
    this.menuManager_ = new MenuManager();

    /** @private {boolean} */
    this.originalStickyState_ = false;

    /** @private {Window} */
    this.ownerWindow_ = window;

    /** @private {?(function(): !Promise)} */
    this.pendingCallback_ = null;

    /** @private {string} */
    this.sessionState_ = '';

    /** @private {Object} */
    this.tutorial_ = null;

    /** @private {Element} */
    this.brailleContainer_ = $('braille-container');
    /** @private {Element} */
    this.brailleTableElement_ = $('braille-table');
    /** @private {Element} */
    this.brailleTableElement2_ = $('braille-table2');
    /** @private {Element} */
    this.searchContainer_ = $('search-container');
    /** @private {!Element} */
    this.searchInput_ = /** @type {!Element} */ ($('search'));
    /** @private {Element} */
    this.speechContainer_ = $('speech-container');
    /** @private {Element} */
    this.speechElement_ = $('speech');

    /** @private {boolean} */
    this.tutorialReadyForTesting_ = false;

    this.initListeners_();
  }

  /** @private */
  initListeners_() {
    chrome.loginState.getSessionState(state => this.updateSessionState_(state));
    chrome.loginState.onSessionStateChanged.addListener(
        state => this.updateSessionState_(state));
    $('braille-pan-left')
        .addEventListener('click', () => this.onPanLeft_(), false);
    $('braille-pan-right')
        .addEventListener('click', () => this.onPanRight_(), false);
    $('menus_button')
        .addEventListener(
            'mousedown', event => this.onOpenMenus_(event), false);
    $('options').addEventListener('click', () => this.onOptions_(), false);
    $('close').addEventListener('click', () => this.onClose_(), false);

    document.addEventListener(
        'keydown', event => this.onKeyDown_(event), false);
    document.addEventListener(
        'mouseup', event => this.onMouseUp_(event), false);
    window.addEventListener(
        'storage', event => this.onStorageChanged_(event), false);
    window.addEventListener(
        'message', message => this.onMessage_(message), false);
    window.addEventListener('blur', event => this.onBlur_(event), false);
    window.addEventListener('hashchange', () => this.onHashChange_(), false);

    BridgeHelper.registerHandler(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ADD_MENU_ITEM,
        itemData => this.menuManager_.addNodeMenuItem(itemData));
    BridgeHelper.registerHandler(
        BridgeConstants.Panel.TARGET,
        BridgeConstants.Panel.Action.ON_CURRENT_RANGE_CHANGED,
        () => this.onCurrentRangeChanged_());
    this.updateFromPrefs_();
  }

  /** Initialize the panel. */
  static async init() {
    if (Panel.instance) {
      throw new Error('Cannot call Panel.init() more than once');
    }

    await LocalStorage.init();
    await SettingsManager.init();
    LocaleOutputHelper.init();

    Panel.instance = new Panel();
    PanelInterface.instance = Panel.instance;

    Msgs.addTranslatedMessagesToDom(document);

    if (location.search.slice(1) === 'tutorial') {
      Panel.instance.onTutorial_();
    }
  }

  /** @override */
  setPendingCallback(callback) {
    this.pendingCallback_ = callback;
  }

  /** Adds BackgroundBridge to the global object so that tests can mock it. */
  static exportBackgroundBridgeForTesting() {
    window.BackgroundBridge = BackgroundBridge;
  }

  /**
   * Update the display based on prefs.
   * @private
   */
  updateFromPrefs_() {
    if (this.mode_ === PanelMode.SEARCH) {
      this.speechContainer_.hidden = true;
      this.brailleContainer_.hidden = true;
      this.searchContainer_.hidden = false;
      return;
    }

    this.speechContainer_.hidden = false;
    this.brailleContainer_.hidden = false;
    this.searchContainer_.hidden = true;

    if (LocalStorage.get('brailleCaptions')) {
      this.speechContainer_.style.visibility = 'hidden';
      this.brailleContainer_.style.visibility = 'visible';
    } else {
      this.speechContainer_.style.visibility = 'visible';
      this.brailleContainer_.style.visibility = 'hidden';
    }
  }

  /**
   * Execute a command to update the panel.
   * @param {PanelCommand} command The command to execute.
   * @private
   */
  exec_(command) {
    /**
     * Escape text so it can be safely added to HTML.
     * @param {*} str Text to be added to HTML, will be cast to string.
     * @return {string} The escaped string.
     */
    function escapeForHtml(str) {
      return String(str)
          .replace(/&/g, '&amp;')
          .replace(/</g, '&lt;')
          .replace(/\>/g, '&gt;')
          .replace(/"/g, '&quot;')
          .replace(/'/g, '&#039;')
          .replace(/\//g, '&#x2F;');
    }

    switch (command.type) {
      case PanelCommandType.CLEAR_SPEECH:
        this.speechElement_.innerHTML = '';
        break;
      case PanelCommandType.ADD_NORMAL_SPEECH:
        if (this.speechElement_.innerHTML !== '') {
          this.speechElement_.innerHTML += '&nbsp;&nbsp;';
        }
        this.speechElement_.innerHTML +=
            '<span class="usertext">' + escapeForHtml(command.data) + '</span>';
        break;
      case PanelCommandType.ADD_ANNOTATION_SPEECH:
        if (this.speechElement_.innerHTML !== '') {
          this.speechElement_.innerHTML += '&nbsp;&nbsp;';
        }
        this.speechElement_.innerHTML += escapeForHtml(command.data);
        break;
      case PanelCommandType.UPDATE_BRAILLE:
        this.onUpdateBraille_(command.data);
        break;
      case PanelCommandType.OPEN_MENUS:
        this.onOpenMenus_(undefined, String(command.data));
        break;
      case PanelCommandType.OPEN_MENUS_MOST_RECENT:
        this.onOpenMenus_(undefined, this.menuManager_.lastMenu);
        break;
      case PanelCommandType.SEARCH:
        this.onSearch_();
        break;
      case PanelCommandType.TUTORIAL:
        this.onTutorial_();
        break;
      case PanelCommandType.CLOSE_CHROMEVOX:
        this.onClose_();
      case PanelCommandType.ENABLE_TEST_HOOKS:
        window.MenuManager = MenuManager;
        window.Msgs = Msgs;
        window.Panel = Panel;
        break;
    }
  }

  /**
   * Sets the mode, which determines the size of the panel and what objects
   *     are shown or hidden.
   * @param {PanelMode} mode The new mode.
   * @private
   */
  setMode_(mode) {
    if (this.mode_ === mode) {
      return;
    }

    // Change the title of ChromeVox menu based on menu's state.
    $('menus_title')
        .setAttribute(
            'msgid',
            mode === PanelMode.FULLSCREEN_MENUS ? 'menus_collapse_title' :
                                                  'menus_title');
    Msgs.addTranslatedMessagesToDom(document);

    this.mode_ = mode;

    document.title = Msgs.getMsg(PanelModeInfo[this.mode_].title);

    // Fully qualify the path here because this function might be called with a
    // window object belonging to the background page.
    this.ownerWindow_.location =
        chrome.extension.getURL('chromevox/panel/panel.html') +
        PanelModeInfo[this.mode_].location;

    $('main').hidden = (this.mode_ === PanelMode.FULLSCREEN_TUTORIAL);
    $('menus_background').hidden = (this.mode_ !== PanelMode.FULLSCREEN_MENUS);
    // Interactive tutorial elements may not have been loaded yet.
    const iTutorialContainer = $('chromevox-tutorial-container');
    if (iTutorialContainer) {
      iTutorialContainer.hidden =
          (this.mode_ !== PanelMode.FULLSCREEN_TUTORIAL);
    }

    this.updateFromPrefs_();

    // Change the orientation of the triangle next to the menus button to
    // indicate whether the menu is open or closed.
    if (mode === PanelMode.FULLSCREEN_MENUS) {
      $('triangle').style.transform = 'rotate(180deg)';
    } else if (mode === PanelMode.COLLAPSED) {
      $('triangle').style.transform = '';
    }
  }

  /**
   * Open / show the ChromeVox Menus.
   * @param {Event=} opt_event An optional event that triggered this.
   * @param {string=} opt_activateMenuTitle Title msg id of menu to open.
   * @private
   */
  async onOpenMenus_(opt_event, opt_activateMenuTitle) {
    // If the menu was already open, close it now and exit early.
    if (this.mode_ !== PanelMode.COLLAPSED) {
      this.setMode_(PanelMode.COLLAPSED);
      return;
    }

    // Eat the event so that a mousedown isn't turned into a drag, allowing
    // users to click-drag-release to select a menu item.
    if (opt_event) {
      opt_event.stopPropagation();
      opt_event.preventDefault();
    }

    await BackgroundBridge.PanelBackground.saveCurrentNode();
    this.setMode_(PanelMode.FULLSCREEN_MENUS);

    const onFocusDo = async () => {
      window.removeEventListener('focus', onFocusDo);
      // Clear any existing menus and clear the callback.
      this.menuManager_.clearMenus();
      this.pendingCallback_ = null;

      const eventSource = await BackgroundBridge.EventSource.get();
      const touchScreen = (eventSource === EventSourceType.TOUCH_GESTURE);

      // Build the top-level menus.
      const searchMenu = this.menuManager_.addSearchMenu('panel_search_menu');
      const jumpMenu = this.menuManager_.addMenu('panel_menu_jump');
      const speechMenu = this.menuManager_.addMenu('panel_menu_speech');
      const touchMenu = touchScreen ?
          this.menuManager_.addMenu('panel_menu_touchgestures') :
          null;
      const chromevoxMenu = this.menuManager_.addMenu('panel_menu_chromevox');
      const actionsMenu = this.menuManager_.addMenu('panel_menu_actions');

      // Add a menu item that opens the full list of ChromeBook keyboard
      // shortcuts. We want this to be at the top of the ChromeVox menu.
      await this.menuManager_.addOSKeyboardShortcutsMenuItem(chromevoxMenu);

      // Create a mapping between categories from CommandStore, and our
      // top-level menus. Some categories aren't mapped to any menu.
      const categoryToMenu = this.menuManager_.makeCategoryMapping(
          actionsMenu, chromevoxMenu, jumpMenu, speechMenu);

      // Make a copy of the key bindings, get the localized title of each
      // command, and then sort them.
      const sortedBindings = await this.menuManager_.getSortedKeyBindings();

      // Insert items from the bindings into the menus.
      const bindingMap = this.menuManager_.makeBindingMap(sortedBindings);
      for (const binding of bindingMap.values()) {
        const category = CommandStore.categoryForCommand(binding.command);
        const menu = category ? categoryToMenu[category] : null;
        this.menuManager_.addMenuItemFromKeyBinding(binding, menu, touchScreen);
      }

      // Add Touch Gestures menu items.
      if (touchMenu) {
        this.menuManager_.addTouchGestureMenuItems(touchMenu);
      }

      if (this.sessionState_ !== 'IN_SESSION') {
        this.menuManager_.denySignedOut();
      }

      // Add a menu item that disables / closes ChromeVox.
      chromevoxMenu.addMenuItem(
          Msgs.getMsg('disable_chromevox'), 'Ctrl+Alt+Z', '', '',
          async () => this.onClose_());

      for (const menuData of ALL_PANEL_MENU_NODE_DATA) {
        this.menuManager_.addNodeMenu(menuData);
      }
      await BackgroundBridge.PanelBackground.createAllNodeMenuBackgrounds(
          opt_activateMenuTitle);

      const actions =
          await BackgroundBridge.PanelBackground.getActionsForCurrentNode();
      for (const standardAction of actions.standardActions) {
        const actionMsg = Panel.ACTION_TO_MSG_ID[standardAction];
        if (!actionMsg) {
          continue;
        }
        const commandName = CommandStore.commandForMessage(actionMsg);
        let shortcutName = '';
        if (commandName) {
          const commandBinding = bindingMap.get(commandName);
          shortcutName = commandBinding ? commandBinding.keySeq : '';
        }
        const actionDesc = Msgs.getMsg(actionMsg);
        actionsMenu.addMenuItem(
            actionDesc, shortcutName, '' /* menuItemBraille */,
            '' /* gesture */,
            () => BackgroundBridge.PanelBackground
                      .performStandardActionOnCurrentNode(standardAction));
      }

      for (const customAction of actions.customActions) {
        actionsMenu.addMenuItem(
            customAction.description, '' /* menuItemShortcut */,
            '' /* menuItemBraille */, '' /* gesture */,
            () => BackgroundBridge.PanelBackground
                      .performCustomActionOnCurrentNode(customAction.id));
      }

      // Activate either the specified menu or the search menu.
      const selectedMenu =
          this.menuManager_.getSelectedMenu(opt_activateMenuTitle);

      const activateFirstItem = (selectedMenu !== this.menuManager_.searchMenu);
      this.menuManager_.activateMenu(selectedMenu, activateFirstItem);
    };

    // The panel does not get focus immediately when we request to be full
    // screen (handled in ChromeVoxPanel natively on hash changed). Wait, if
    // needed, for focus to begin initialization.
    if (document.hasFocus()) {
      onFocusDo();
    } else {
      window.addEventListener('focus', onFocusDo);
    }
  }

  /**
   * Open incremental search.
   * @private
   */
  async onSearch_() {
    this.setMode_(PanelMode.SEARCH);
    this.menuManager_.clearMenus();
    this.pendingCallback_ = null;
    this.updateFromPrefs_();
    await ISearchUI.init(this.searchInput_);
  }

  /**
   * Updates the content shown on the virtual braille display.
   * @param {*=} data The data sent through the PanelCommand.
   * @private
   */
  onUpdateBraille_(data) {
    const groups = data.groups;
    const cols = data.cols;
    const rows = data.rows;
    const sideBySide = SettingsManager.get('brailleSideBySide');

    const addBorders = event => {
      const cell = event.target;
      if (cell.tagName === 'TD') {
        cell.className = 'highlighted-cell';
        const companionIDs = cell.getAttribute('data-companionIDs');
        companionIDs.split(' ').forEach(
            companionID => $(companionID).className = 'highlighted-cell');
      }
    };

    const removeBorders = event => {
      const cell = event.target;
      if (cell.tagName === 'TD') {
        cell.className = 'unhighlighted-cell';
        const companionIDs = cell.getAttribute('data-companionIDs');
        companionIDs.split(' ').forEach(
            companionID => $(companionID).className = 'unhighlighted-cell');
      }
    };

    const routeCursor = event => {
      const cell = event.target;
      if (cell.tagName === 'TD') {
        const displayPosition = parseInt(cell.id.split('-')[0], 10);
        if (Number.isNaN(displayPosition)) {
          throw new Error(
              'The display position is calculated assuming that the cell ID ' +
              'is formatted like int-string. For example, 0-brailleCell is a ' +
              'valid cell ID.');
        }
        chrome.extension.getBackgroundPage()['ChromeVox'].braille.route(
            displayPosition);
      }
    };

    this.brailleContainer_.addEventListener('mouseover', addBorders);
    this.brailleContainer_.addEventListener('mouseout', removeBorders);
    this.brailleContainer_.addEventListener('click', routeCursor);

    // Clear the tables.
    let rowCount = this.brailleTableElement_.rows.length;
    for (let i = 0; i < rowCount; i++) {
      this.brailleTableElement_.deleteRow(0);
    }
    rowCount = this.brailleTableElement2_.rows.length;
    for (let i = 0; i < rowCount; i++) {
      this.brailleTableElement2_.deleteRow(0);
    }

    let row1;
    let row2;
    // Number of rows already written.
    rowCount = 0;
    // Number of cells already written in this row.
    let cellCount = cols;
    for (let i = 0; i < groups.length; i++) {
      if (cellCount === cols) {
        cellCount = 0;
        // Check if we reached the limit on the number of rows we can have.
        if (rowCount === rows) {
          break;
        }
        rowCount++;
        row1 = this.brailleTableElement_.insertRow(-1);
        if (sideBySide) {
          // Side by side.
          row2 = this.brailleTableElement2_.insertRow(-1);
        } else {
          // Interleaved.
          row2 = this.brailleTableElement_.insertRow(-1);
        }
      }

      const topCell = row1.insertCell(-1);
      topCell.innerHTML = groups[i][0];
      topCell.id = i + '-textCell';
      topCell.setAttribute('data-companionIDs', i + '-brailleCell');
      topCell.className = 'unhighlighted-cell';

      let bottomCell = row2.insertCell(-1);
      bottomCell.id = i + '-brailleCell';
      bottomCell.setAttribute('data-companionIDs', i + '-textCell');
      bottomCell.className = 'unhighlighted-cell';
      if (cellCount + groups[i][1].length > cols) {
        let brailleText = groups[i][1];
        while (cellCount + brailleText.length > cols) {
          // At this point we already have a bottomCell to fill, so fill it.
          bottomCell.innerHTML = brailleText.substring(0, cols - cellCount);
          // Update to see what we still have to fill.
          brailleText = brailleText.substring(cols - cellCount);
          // Make new row.
          if (rowCount === rows) {
            break;
          }
          rowCount++;
          row1 = this.brailleTableElement_.insertRow(-1);
          if (sideBySide) {
            // Side by side.
            row2 = this.brailleTableElement2_.insertRow(-1);
          } else {
            // Interleaved.
            row2 = this.brailleTableElement_.insertRow(-1);
          }
          const bottomCell2 = row2.insertCell(-1);
          bottomCell2.id = i + '-brailleCell2';
          bottomCell2.setAttribute(
              'data-companionIDs', i + '-textCell ' + i + '-brailleCell');
          bottomCell.setAttribute(
              'data-companionIDs',
              bottomCell.getAttribute('data-companionIDs') + ' ' + i +
                  '-brailleCell2');
          topCell.setAttribute(
              'data-companionID2',
              bottomCell.getAttribute('data-companionIDs') + ' ' + i +
                  '-brailleCell2');

          bottomCell2.className = 'unhighlighted-cell';
          bottomCell = bottomCell2;
          cellCount = 0;
        }
        // Fill the rest.
        bottomCell.innerHTML = brailleText;
        cellCount = brailleText.length;
      } else {
        bottomCell.innerHTML = groups[i][1];
        cellCount += groups[i][1].length;
      }
    }
  }

  /**
   * Sets the index of the current active menu to be 0.
   * @private
   */
  scrollToTop_() {
    this.menuManager_.activeMenu.scrollToTop();
  }

  /**
   * Sets the index of the current active menu to be the last index.
   * @private
   */
  scrollToBottom_() {
    this.menuManager_.activeMenu.scrollToBottom();
  }

  /**
   * Advance the index of the current active menu item by |delta|.
   * @param {number} delta The number to add to the active menu item index.
   * @private
   */
  advanceItemBy_(delta) {
    if (this.menuManager_.activeMenu) {
      this.menuManager_.activeMenu.advanceItemBy(delta);
    }
  }

  /**
   * Called when the user releases the mouse button. If it's anywhere other
   * than on the menus button, close the menus and return focus to the page,
   * and if the mouse was released over a menu item, execute that item's
   * callback.
   * @param {Event} event The mouse event.
   * @private
   */
  onMouseUp_(event) {
    if (!this.menuManager_.activeMenu) {
      return;
    }

    let target = event.target;
    while (target && !target.classList.contains('menu-item')) {
      // Allow the user to click and release on the menu button and leave
      // the menu button.
      if (target.id === 'menus_button') {
        return;
      }

      target = target.parentElement;
    }

    if (target && this.menuManager_.activeMenu) {
      this.pendingCallback_ =
          this.menuManager_.activeMenu.getCallbackForElement(target);
    }
    this.closeMenusAndRestoreFocus();
  }

  /**
   * Called when a key is pressed. Handle arrow keys to navigate the menus,
   * Esc to close, and Enter/Space to activate an item.
   * @param {Event} event The key event.
   * @private
   */
  onKeyDown_(event) {
    if (event.key === 'Escape' &&
        this.mode_ === PanelMode.FULLSCREEN_TUTORIAL) {
      this.setMode_(PanelMode.COLLAPSED);
      return;
    }

    if (!this.menuManager_.activeMenu) {
      return;
    }

    if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) {
      return;
    }

    // We need special logic for navigating the search bar.
    // If left/right arrow are pressed, we should adjust the search bar's
    // cursor. We only want to advance the active menu if we are at the
    // beginning/end of the search bar's contents.
    if (this.menuManager_.searchMenu &&
        event.target === this.menuManager_.searchMenu.searchBar) {
      switch (event.key) {
        case 'ArrowLeft':
        case 'ArrowRight':
          if (event.target.value) {
            const cursorIndex = event.target.selectionStart +
                (event.key === 'ArrowRight' ? 1 : -1);
            const queryLength = event.target.value.length;
            if (cursorIndex >= 0 && cursorIndex <= queryLength) {
              return;
            }
          }
          break;
        case ' ':
          return;
      }
    }

    switch (event.key) {
      case 'ArrowLeft':
        this.menuManager_.advanceActiveMenuBy(-1);
        break;
      case 'ArrowRight':
        this.menuManager_.advanceActiveMenuBy(1);
        break;
      case 'ArrowUp':
        this.advanceItemBy_(-1);
        break;
      case 'ArrowDown':
        this.advanceItemBy_(1);
        break;
      case 'Escape':
        this.closeMenusAndRestoreFocus();
        break;
      case 'PageUp':
        this.advanceItemBy_(10);
        break;
      case 'PageDown':
        this.advanceItemBy_(-10);
        break;
      case 'Home':
        this.scrollToTop_();
        break;
      case 'End':
        this.scrollToBottom_();
        break;
      case 'Enter':
      case ' ':
        this.pendingCallback_ = this.getCallbackForCurrentItem_();
        this.closeMenusAndRestoreFocus();
        break;
      default:
        // Don't mark this event as handled.
        return;
    }

    event.preventDefault();
    event.stopPropagation();
  }

  /**
   * Open the ChromeVox Options.
   * @private
   */
  onOptions_() {
    BackgroundBridge.CommandHandler.onCommand(Command.SHOW_OPTIONS_PAGE);
    this.setMode_(PanelMode.COLLAPSED);
  }

  /**
   * Exit ChromeVox.
   * @private
   */
  onClose_() {
    // Change the url fragment to 'close', which signals the native code
    // to exit ChromeVox.
    this.ownerWindow_.location =
        chrome.extension.getURL('chromevox/panel/panel.html') + '#close';
  }

  /**
   * Get the callback for whatever item is currently selected.
   * @return {?Function} The callback for the current item.
   * @private
   */
  getCallbackForCurrentItem_() {
    if (this.menuManager_.activeMenu) {
      return this.menuManager_.activeMenu.getCallbackForCurrentItem();
    }
    return null;
  }

  /** @override */
  async closeMenusAndRestoreFocus() {
    const pendingCallback = this.pendingCallback_;
    this.pendingCallback_ = null;

    // Prepare the watcher before close the panel so that the watcher won't miss
    // panel collapse signal.
    await BackgroundBridge.PanelBackground.setPanelCollapseWatcher();

    // Make sure all menus are cleared to avoid bogus output when we re-open.
    this.menuManager_.clearMenus();

    // Make sure we're not in full-screen mode.
    this.setMode_(PanelMode.COLLAPSED);

    this.menuManager_.activeMenu = null;

    await BackgroundBridge.PanelBackground.waitForPanelCollapse();

    if (pendingCallback) {
      await pendingCallback();
    }
    BackgroundBridge.PanelBackground.clearSavedNode();
  }

  /**
   * Open the tutorial.
   * @private
   */
  onTutorial_() {
    chrome.chromeosInfoPrivate.isTabletModeEnabled(enabled => {
      // Use tablet mode to decide the medium for the tutorial.
      const medium = enabled ? constants.InteractionMedium.TOUCH :
                               constants.InteractionMedium.KEYBOARD;
      if (!$('chromevox-tutorial')) {
        let curriculum = null;
        if (this.sessionState_ ===
            chrome.loginState.SessionState.IN_OOBE_SCREEN) {
          // We currently support two mediums: keyboard and touch, which is why
          // we can decide the curriculum using a ternary statement.
          curriculum = medium === constants.InteractionMedium.KEYBOARD ?
              'quick_orientation' :
              'touch_orientation';
        }
        this.createITutorial_(curriculum, medium);
      }

      this.setMode_(PanelMode.FULLSCREEN_TUTORIAL);
      if (this.tutorial_ && this.tutorial_.show) {
        this.tutorial_.medium = medium;
        this.tutorial_.show();
      }
    });
  }

  /**
   * Creates a <chromevox-tutorial> element and adds it to the dom.
   * @param {(string|null)} curriculum
   * @param {constants.InteractionMedium} medium
   * @private
   */
  createITutorial_(curriculum, medium) {
    const tutorialScript = document.createElement('script');
    tutorialScript.src =
        '../../common/tutorial/components/chromevox_tutorial.js';
    tutorialScript.setAttribute('type', 'module');
    document.body.appendChild(tutorialScript);

    // Create tutorial container and element.
    const tutorialContainer = document.createElement('div');
    tutorialContainer.setAttribute('id', 'chromevox-tutorial-container');
    tutorialContainer.hidden = true;
    const tutorialElement = document.createElement('chromevox-tutorial');
    tutorialElement.setAttribute('id', 'chromevox-tutorial');
    if (curriculum) {
      tutorialElement.curriculum = curriculum;
    }
    tutorialElement.medium = medium;
    tutorialContainer.appendChild(tutorialElement);
    document.body.appendChild(tutorialContainer);
    this.tutorial_ = tutorialElement;

    // Add listeners. These are custom events fired from custom components.
    const backgroundPage = chrome.extension.getBackgroundPage();

    $('chromevox-tutorial').addEventListener('closetutorial', async evt => {
      // Ensure UserActionMonitor is destroyed before closing tutorial.
      await BackgroundBridge.UserActionMonitor.destroy();
      this.onCloseTutorial_();
    });
    $('chromevox-tutorial')
        .addEventListener('startinteractivemode', async evt => {
          const actions = evt.detail.actions;
          await BackgroundBridge.UserActionMonitor.create(actions);
          await BackgroundBridge.UserActionMonitor.destroy();
          if (this.tutorial_ && this.tutorial_.showNextLesson) {
            this.tutorial_.showNextLesson();
          }
        });
    $('chromevox-tutorial')
        .addEventListener('stopinteractivemode', async evt => {
          await BackgroundBridge.UserActionMonitor.destroy();
        });
    $('chromevox-tutorial').addEventListener('requestfullydescribe', evt => {
      BackgroundBridge.CommandHandler.onCommand(Command.FULLY_DESCRIBE);
    });
    $('chromevox-tutorial').addEventListener('requestearcon', evt => {
      evt = /** @type {{detail: {earconId: string}}} */ (evt);
      const earconId = evt.detail.earconId;
      backgroundPage['ChromeVox']['earcons']['playEarcon'](earconId);
    });
    $('chromevox-tutorial').addEventListener('cancelearcon', evt => {
      evt = /** @type {{detail: {earconId: string}}} */ (evt);
      const earconId = evt.detail.earconId;
      backgroundPage['ChromeVox']['earcons']['cancelEarcon'](earconId);
    });
    $('chromevox-tutorial').addEventListener('readyfortesting', () => {
      this.tutorialReadyForTesting_ = true;
    });
    $('chromevox-tutorial').addEventListener('openUrl', async evt => {
      const url = evt.detail.url;
      // Ensure UserActionMonitor is destroyed before closing tutorial.
      await BackgroundBridge.UserActionMonitor.destroy();
      this.onCloseTutorial_();
      BrowserUtil.openBrowserUrl(url);
    });
  }

  /**
   * Close the tutorial.
   * @private
   */
  onCloseTutorial_() {
    this.setMode_(PanelMode.COLLAPSED);
  }

  /** @private */
  onCurrentRangeChanged_() {
    if (this.mode_ === PanelMode.FULLSCREEN_TUTORIAL) {
      if (this.tutorial_ && this.tutorial_.restartNudges) {
        this.tutorial_.restartNudges();
      }
    }
  }

  /** @private */
  onBlur_(event) {
    if (event.target !== window || document.activeElement === document.body) {
      return;
    }

    this.closeMenusAndRestoreFocus();
  }

  /** @private */
  async onHashChange_() {
    // Save the sticky state when a user first focuses the panel.
    if (location.hash === '#fullscreen' || location.hash === '#focus') {
      this.originalStickyState_ =
          await BackgroundBridge.ChromeVoxPrefs.getStickyPref();
    }

    // If the original sticky state was on when we first entered the panel,
    // toggle it in in every case. (fullscreen/focus turns the state off,
    // collapse turns it back on).
    if (this.originalStickyState_) {
      BackgroundBridge.CommandHandler.onCommand(Command.TOGGLE_STICKY_MODE);
    }
  }

  /** @private */
  onMessage_(message) {
    const command = JSON.parse(message.data);
    this.exec_(/** @type {PanelCommand} */ (command));
  }

  /** @private */
  async onPanLeft_() {
    await BackgroundBridge.Braille.panLeft();
  }

  /** @private */
  async onPanRight_() {
    await BackgroundBridge.Braille.panRight();
  }

  /** @private */
  onStorageChanged_(event) {
    if (event.key === 'brailleCaptions') {
      this.updateFromPrefs_();
    }
  }

  /** @private */
  updateSessionState_(sessionState) {
    this.sessionState_ = sessionState;
    $('options').disabled = sessionState !== 'IN_SESSION';
  }
}

Panel.ACTION_TO_MSG_ID = {
  decrement: 'action_decrement_description',
  doDefault: 'perform_default_action',
  increment: 'action_increment_description',
  scrollBackward: 'action_scroll_backward_description',
  scrollForward: 'action_scroll_forward_description',
  showContextMenu: 'show_context_menu',
  longClick: 'force_long_click_on_current_item',
};

window.addEventListener('load', async () => await Panel.init(), false);

/** @type {Panel} */
Panel.instance;
