// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 */
export default class View {
  /**
   * @return {string}
   */
  viewId() {
  }

  /**
   * @return {string}
   */
  title() {
  }

  /**
   * @return {boolean}
   */
  isCloseable() {
  }

  /**
   * @return {boolean}
   */
  isTransient() {
  }

  /**
   * @return {!Promise<!Array<!UI.ToolbarItem>>}
   */
  toolbarItems() {
  }

  /**
   * @return {!Promise<!UI.Widget>}
   */
  widget() {
  }

  /**
   * @return {!Promise|undefined}
   */
  disposeView() {}
}

export const _symbol = Symbol('view');
export const _widgetSymbol = Symbol('widget');

/**
 * @implements {View}
 * @unrestricted
 */
export class SimpleView extends UI.VBox {
  /**
   * @param {string} title
   * @param {boolean=} isWebComponent
   */
  constructor(title, isWebComponent) {
    super(isWebComponent);
    this._title = title;
    /** @type {!Array<!UI.ToolbarItem>} */
    this._toolbarItems = [];
    this[_symbol] = this;
  }

  /**
   * @override
   * @return {string}
   */
  viewId() {
    return this._title;
  }

  /**
   * @override
   * @return {string}
   */
  title() {
    return this._title;
  }

  /**
   * @override
   * @return {boolean}
   */
  isCloseable() {
    return false;
  }

  /**
   * @override
   * @return {boolean}
   */
  isTransient() {
    return false;
  }

  /**
   * @override
   * @return {!Promise<!Array<!UI.ToolbarItem>>}
   */
  toolbarItems() {
    return Promise.resolve(this.syncToolbarItems());
  }

  /**
   * @return {!Array<!UI.ToolbarItem>}
   */
  syncToolbarItems() {
    return this._toolbarItems;
  }

  /**
   * @override
   * @return {!Promise<!UI.Widget>}
   */
  widget() {
    return /** @type {!Promise<!UI.Widget>} */ (Promise.resolve(this));
  }

  /**
   * @param {!UI.ToolbarItem} item
   */
  addToolbarItem(item) {
    this._toolbarItems.push(item);
  }

  /**
   * @return {!Promise}
   */
  revealView() {
    return UI.viewManager.revealView(this);
  }

  /**
   * @override
   */
  disposeView() {
  }
}

/**
 * @implements {View}
 * @unrestricted
 */
export class ProvidedView {
  /**
   * @param {!Root.Runtime.Extension} extension
   */
  constructor(extension) {
    this._extension = extension;
  }

  /**
   * @override
   * @return {string}
   */
  viewId() {
    return this._extension.descriptor()['id'];
  }

  /**
   * @override
   * @return {string}
   */
  title() {
    return this._extension.title();
  }

  /**
   * @override
   * @return {boolean}
   */
  isCloseable() {
    return this._extension.descriptor()['persistence'] === 'closeable';
  }

  /**
   * @override
   * @return {boolean}
   */
  isTransient() {
    return this._extension.descriptor()['persistence'] === 'transient';
  }

  /**
   * @override
   * @return {!Promise<!Array<!UI.ToolbarItem>>}
   */
  toolbarItems() {
    const actionIds = this._extension.descriptor()['actionIds'];
    if (actionIds) {
      const result = actionIds.split(',').map(id => UI.Toolbar.createActionButtonForId(id.trim()));
      return Promise.resolve(result);
    }

    if (this._extension.descriptor()['hasToolbar']) {
      return this.widget().then(widget => /** @type {!UI.ToolbarItem.ItemsProvider} */ (widget).toolbarItems());
    }
    return Promise.resolve([]);
  }

  /**
   * @override
   * @return {!Promise<!UI.Widget>}
   */
  async widget() {
    this._widgetRequested = true;
    const widget = await this._extension.instance();
    if (!(widget instanceof UI.Widget)) {
      throw new Error('view className should point to a UI.Widget');
    }
    widget[_symbol] = this;
    return /** @type {!UI.Widget} */ (widget);
  }

  /**
   * @override
   */
  async disposeView() {
    if (!this._widgetRequested) {
      return;
    }
    const widget = await this.widget();
    widget.ownerViewDisposed();
  }
}

/**
 * @interface
 */
export class ViewLocation {
  /**
   * @param {string} locationName
   */
  appendApplicableItems(locationName) {
  }

  /**
   * @param {!View} view
   * @param {?View=} insertBefore
   */
  appendView(view, insertBefore) {
  }

  /**
   * @param {!View} view
   * @param {?View=} insertBefore
   * @param {boolean=} userGesture
   * @return {!Promise}
   */
  showView(view, insertBefore, userGesture) {
  }

  /**
<<<<<<< HEAD
   * @param {function()=} revealCallback
   * @param {string=} location
   * @return {!UI.ViewLocation}
   */
  createStackLocation(revealCallback, location) {
    return new UI.ViewManager._StackLocation(this, revealCallback, location);
  }

  /**
   * @param {string} location
   * @return {boolean}
   */
  hasViewsForLocation(location) {
    return !!this._viewsForLocation(location).length;
  }

  /**
   * @param {string} location
   * @return {!Array<!UI.View>}
   */
  _viewsForLocation(location) {
    const result = [];
    for (const id of this._views.keys()) {
      if (this._locationNameByViewId.get(id) === location)
        result.push(this._views.get(id));
    }
    return result;
  }
};


/**
 * @unrestricted
 */
UI.ViewManager._ContainerWidget = class extends UI.VBox {
  /**
   * @param {!UI.View} view
   */
  constructor(view) {
    super();
    this.element.classList.add('flex-auto', 'view-container', 'overflow-auto');
    this._view = view;
    this.element.tabIndex = -1;
    this.setDefaultFocusedElement(this.element);
  }

  /**
   * @return {!Promise}
   */
  _materialize() {
    if (this._materializePromise)
      return this._materializePromise;
    const promises = [];
    promises.push(this._view.toolbarItems().then(UI.ViewManager._populateToolbar.bind(UI.ViewManager, this.element)));
    promises.push(this._view.widget().then(widget => {
      // Move focus from |this| to loaded |widget| if any.
      const shouldFocus = this.element.hasFocus();
      this.setDefaultFocusedElement(null);
      this._view[UI.View._widgetSymbol] = widget;
      widget.show(this.element);
      if (shouldFocus)
        widget.focus();
    }));
    this._materializePromise = Promise.all(promises);
    return this._materializePromise;
  }

  /**
   * @override
   */
  wasShown() {
    this._materialize().then(() => {
      this._wasShownForTest();
    });
  }

  _wasShownForTest() {
    // This method is sniffed in tests.
  }
};

/**
 * @unrestricted
 */
UI.ViewManager._ExpandableContainerWidget = class extends UI.VBox {
  /**
   * @param {!UI.View} view
   */
  constructor(view) {
    super(true);
    this.element.classList.add('flex-none');
    this.registerRequiredCSS('ui/viewContainers.css');

    this._titleElement = createElementWithClass('div', 'expandable-view-title');
    UI.ARIAUtils.markAsLink(this._titleElement);
    this._titleExpandIcon = UI.Icon.create('smallicon-triangle-right', 'title-expand-icon');
    this._titleElement.appendChild(this._titleExpandIcon);
    this._titleElement.createTextChild(view.title());
    this._titleElement.tabIndex = 0;
    this._titleElement.addEventListener('click', this._toggleExpanded.bind(this), false);
    this._titleElement.addEventListener('keydown', this._onTitleKeyDown.bind(this), false);
    this.contentElement.insertBefore(this._titleElement, this.contentElement.firstChild);

    this.contentElement.createChild('slot');
    this._view = view;
    view[UI.ViewManager._ExpandableContainerWidget._symbol] = this;
  }

  /**
   * @return {!Promise}
=======
   * @param {!View} view
>>>>>>> bc9bfbcd01448b108b9f2f03cc440b2b92016b02
   */
  removeView(view) {
  }

  /**
   * @return {!UI.Widget}
   */
  widget() {
  }
}

/**
 * @interface
 */
export class TabbedViewLocation extends ViewLocation {
  /**
   * @return {!UI.TabbedPane}
   */
  tabbedPane() {
  }

  /**
   * @return {!UI.ToolbarMenuButton}
   */
  enableMoreTabsButton() {
  }
}

/**
 * @interface
 */
export class ViewLocationResolver {
  /**
   * @param {string} location
   * @return {?ViewLocation}
   */
  resolveLocation(location) {
  }
}

/* Legacy exported object*/
self.UI = self.UI || {};

/* Legacy exported object*/
UI = UI || {};

/** @interface */
UI.View = View;

/** @public */
UI.View.widgetSymbol = _widgetSymbol;

/** @constructor */
UI.SimpleView = SimpleView;

/** @constructor */
UI.ProvidedView = ProvidedView;

/** @interface */
UI.ViewLocation = ViewLocation;

/** @interface */
UI.TabbedViewLocation = TabbedViewLocation;

/** @interface */
UI.ViewLocationResolver = ViewLocationResolver;
