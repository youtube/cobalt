/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A dialog for editing/creating a link.
 */

goog.provide('goog.ui.editor.LinkDialog');
goog.provide('goog.ui.editor.LinkDialog.BeforeTestLinkEvent');
goog.provide('goog.ui.editor.LinkDialog.EventType');
goog.provide('goog.ui.editor.LinkDialog.OkEvent');

goog.require('goog.a11y.aria');
goog.require('goog.a11y.aria.State');
goog.require('goog.dom');
goog.require('goog.dom.InputType');
goog.require('goog.dom.TagName');
goog.require('goog.dom.safe');
goog.require('goog.editor.BrowserFeature');
goog.require('goog.editor.Link');
goog.require('goog.editor.focus');
goog.require('goog.editor.node');
goog.require('goog.events.Event');
goog.require('goog.events.EventHandler');
goog.require('goog.events.InputHandler');
goog.require('goog.html.SafeHtml');
goog.require('goog.html.SafeHtmlFormatter');
goog.require('goog.string');
goog.require('goog.string.Unicode');
goog.require('goog.style');
goog.require('goog.ui.Button');
goog.require('goog.ui.Component');
goog.require('goog.ui.LinkButtonRenderer');
goog.require('goog.ui.editor.AbstractDialog');
goog.require('goog.ui.editor.TabPane');
goog.require('goog.ui.editor.messages');
goog.require('goog.userAgent');
goog.require('goog.window');
goog.requireType('goog.ui.Tab');



/**
 * A type of goog.ui.editor.AbstractDialog for editing/creating a link.
 * @param {goog.dom.DomHelper} domHelper DomHelper to be used to create the
 *     dialog's dom structure.
 * @param {goog.editor.Link} link The target link.
 * @constructor
 * @extends {goog.ui.editor.AbstractDialog}
 * @final
 */
goog.ui.editor.LinkDialog = function(domHelper, link) {
  'use strict';
  goog.ui.editor.LinkDialog.base(this, 'constructor', domHelper);

  /**
   * The link being modified by this dialog.
   * @type {goog.editor.Link}
   * @private
   */
  this.targetLink_ = link;

  /**
   * The event handler for this dialog.
   * @type {goog.events.EventHandler<!goog.ui.editor.LinkDialog>}
   * @private
   */
  this.eventHandler_ = new goog.events.EventHandler(this);
  this.registerDisposable(this.eventHandler_);

  /**
   * Optional warning to show about email addresses.
   * @type {?goog.html.SafeHtml}
   * @private
   */
  this.emailWarning_ = null;

  /**
   * Whether to show a checkbox where the user can choose to have the link open
   * in a new window.
   * @type {boolean}
   * @private
   */
  this.showOpenLinkInNewWindow_ = false;

  /**
   * Whether to focus the text to display input instead of the url input if the
   * text to display input is empty when the dialog opens.
   * @type {boolean}
   * @private
   */
  this.focusTextToDisplayOnOpenIfEmpty_ = false;

  /**
   * Whether the "open link in new window" checkbox should be checked when the
   * dialog is shown, and also whether it was checked last time the dialog was
   * closed.
   * @type {boolean}
   * @private
   */
  this.isOpenLinkInNewWindowChecked_ = false;

  /**
   * Whether to show a checkbox where the user can choose to have 'rel=nofollow'
   * attribute added to the link.
   * @type {boolean}
   * @private
   */
  this.showRelNoFollow_ = false;

  /**
   * InputHandler object to listen for changes in the url input field.
   * @type {?goog.events.InputHandler}
   * @private
   */
  this.urlInputHandler_ = null;

  /**
   * InputHandler object to listen for changes in the email input field.
   * @type {?goog.events.InputHandler}
   * @private
   */
  this.emailInputHandler_ = null;

  /**
   * InputHandler object to listen for changes in the text to display input
   * field.
   * @type {?goog.events.InputHandler}
   * @private
   */
  this.textInputHandler_ = null;

  /**
   * The tab bar where the url and email tabs are.
   * @type {?goog.ui.editor.TabPane}
   * @private
   */
  this.tabPane_ = null;

  /**
   * The div element holding the link's display text input.
   * @type {?HTMLDivElement}
   * @private
   */
  this.textToDisplayDiv_ = null;

  /**
   * The input element holding the link's display text.
   * @type {?HTMLInputElement}
   * @private
   */
  this.textToDisplayInput_ = null;

  /**
   * Whether or not the feature of automatically generating the display text is
   * enabled.
   * @type {boolean}
   * @private
   */
  this.autogenFeatureEnabled_ = true;

  /**
   * Whether or not we should automatically generate the display text.
   * @type {boolean}
   * @private
   */
  this.autogenerateTextToDisplay_ = false;

  /**
   * Whether or not automatic generation of the display text is disabled.
   * @type {boolean}
   * @private
   */
  this.disableAutogen_ = false;

  /**
   * The input element (checkbox) to indicate that the link should open in a new
   * window.
   * @type {?HTMLInputElement}
   * @private
   */
  this.openInNewWindowCheckbox_ = null;

  /**
   * The input element (checkbox) to indicate that the link should have
   * 'rel=nofollow' attribute.
   * @type {?HTMLInputElement}
   * @private
   */
  this.relNoFollowCheckbox_ = null;

  /**
   * Whether to stop leaking the page's url via the referrer header when the
   * "test this link" link is clicked.
   * @private {boolean}
   */
  this.stopReferrerLeaks_ = false;

  /**
   * Whether to remove access to the current window object in the newly created
   * window when the "test this link" is clicked, since it can be used to launch
   * a reverse tabnabbing attack.
   * @private {boolean}
   */
  this.stopTabNabbing_ = false;
};
goog.inherits(goog.ui.editor.LinkDialog, goog.ui.editor.AbstractDialog);


/**
 * Events specific to the link dialog.
 * @enum {string}
 */
goog.ui.editor.LinkDialog.EventType = {
  BEFORE_TEST_LINK: 'beforetestlink'
};



/**
 * OK event object for the link dialog.
 * @param {string} linkText Text the user chose to display for the link.
 * @param {string} linkUrl Url the user chose for the link to point to.
 * @param {boolean} openInNewWindow Whether the link should open in a new window
 *     when clicked.
 * @param {boolean} noFollow Whether the link should have 'rel=nofollow'
 *     attribute.
 * @constructor
 * @extends {goog.events.Event}
 * @final
 */
goog.ui.editor.LinkDialog.OkEvent = function(
    linkText, linkUrl, openInNewWindow, noFollow) {
  'use strict';
  goog.ui.editor.LinkDialog.OkEvent.base(
      this, 'constructor', goog.ui.editor.AbstractDialog.EventType.OK);

  /**
   * The text of the link edited in the dialog.
   * @type {string}
   */
  this.linkText = linkText;

  /**
   * The url of the link edited in the dialog.
   * @type {string}
   */
  this.linkUrl = linkUrl;

  /**
   * Whether the link should open in a new window when clicked.
   * @type {boolean}
   */
  this.openInNewWindow = openInNewWindow;

  /**
   * Whether the link should have 'rel=nofollow' attribute.
   * @type {boolean}
   */
  this.noFollow = noFollow;
};
goog.inherits(goog.ui.editor.LinkDialog.OkEvent, goog.events.Event);



/**
 * Event fired before testing a link by opening it in another window.
 * Calling preventDefault will stop the link from being opened.
 * @param {string} url Url of the link being tested.
 * @constructor
 * @extends {goog.events.Event}
 * @final
 */
goog.ui.editor.LinkDialog.BeforeTestLinkEvent = function(url) {
  'use strict';
  goog.ui.editor.LinkDialog.BeforeTestLinkEvent.base(
      this, 'constructor',
      goog.ui.editor.LinkDialog.EventType.BEFORE_TEST_LINK);

  /**
   * The url of the link being tested.
   * @type {string}
   */
  this.url = url;
};
goog.inherits(goog.ui.editor.LinkDialog.BeforeTestLinkEvent, goog.events.Event);


/**
 * Sets the warning message to show to users about including email addresses on
 * public web pages.
 * @param {!goog.html.SafeHtml} emailWarning Warning message to show users about
 *     including email addresses on the web.
 */
goog.ui.editor.LinkDialog.prototype.setEmailWarning = function(emailWarning) {
  'use strict';
  this.emailWarning_ = emailWarning;
};


/**
 * Tells the dialog to show a checkbox where the user can choose to have the
 * link open in a new window.
 * @param {boolean} startChecked Whether to check the checkbox the first
 *     time the dialog is shown. Subesquent times the checkbox will remember its
 *     previous state.
 */
goog.ui.editor.LinkDialog.prototype.showOpenLinkInNewWindow = function(
    startChecked) {
  'use strict';
  this.showOpenLinkInNewWindow_ = true;
  this.isOpenLinkInNewWindowChecked_ = startChecked;
};


/**
 * Tells the dialog to focus the text to display input instead of the url field
 * if the text to display input is empty when the dialog is opened.
 */
goog.ui.editor.LinkDialog.prototype.focusTextToDisplayOnOpenIfEmpty =
    function() {
  'use strict';
  this.focusTextToDisplayOnOpenIfEmpty_ = true;
};


/**
 * Tells the dialog to show a checkbox where the user can choose to add
 * 'rel=nofollow' attribute to the link.
 */
goog.ui.editor.LinkDialog.prototype.showRelNoFollow = function() {
  'use strict';
  this.showRelNoFollow_ = true;
};


/** @override */
goog.ui.editor.LinkDialog.prototype.show = function() {
  'use strict';
  goog.ui.editor.LinkDialog.base(this, 'show');


  this.selectAppropriateTab_(
      this.textToDisplayInput_.value, this.getTargetUrl_());

  if (this.focusTextToDisplayOnOpenIfEmpty_ &&
      !this.targetLink_.getCurrentText()) {
    goog.editor.focus.focusInputField(this.textToDisplayInput_);
  }

  this.syncOkButton_();

  if (this.showOpenLinkInNewWindow_) {
    if (!this.targetLink_.isNew()) {
      // If link is not new, checkbox should reflect current target.
      this.isOpenLinkInNewWindowChecked_ =
          this.targetLink_.getAnchor().target == '_blank';
    }
    this.openInNewWindowCheckbox_.checked = this.isOpenLinkInNewWindowChecked_;
  }

  if (this.showRelNoFollow_) {
    this.relNoFollowCheckbox_.checked =
        goog.ui.editor.LinkDialog.hasNoFollow(this.targetLink_.getAnchor().rel);
  }
};


/** @override */
goog.ui.editor.LinkDialog.prototype.hide = function() {
  'use strict';
  this.disableAutogenFlag_(false);
  goog.ui.editor.LinkDialog.base(this, 'hide');
};


/**
 * Tells the dialog whether to show the 'text to display' div.
 * When the target element of the dialog is an image, there is no link text
 * to modify. This function can be used for this kind of situations.
 * @param {boolean} visible Whether to make 'text to display' div visible.
 */
goog.ui.editor.LinkDialog.prototype.setTextToDisplayVisible = function(
    visible) {
  'use strict';
  if (this.textToDisplayDiv_) {
    goog.style.setStyle(
        this.textToDisplayDiv_, 'display', visible ? 'block' : 'none');
  }
};


/**
 * Tells the plugin whether to stop leaking the page's url via the referrer
 * header when the "test this link" link is clicked.
 * @param {boolean} stop Whether to stop leaking the referrer.
 */
goog.ui.editor.LinkDialog.prototype.setStopReferrerLeaks = function(stop) {
  'use strict';
  this.stopReferrerLeaks_ = stop;
};


/**
 * Tells the plugin whether to remove access to the current window object in the
 * newly created window when the "test this link" is clicked, since it can be
 * used to launch a reverse tabnabbing attack.
 * @param {boolean} stop Whether to remove the reference to the current window
 *     in the new window.
 */
goog.ui.editor.LinkDialog.prototype.setStopTabNabbing = function(stop) {
  'use strict';
  this.stopTabNabbing_ = stop;
};


/**
 * Tells the dialog whether the autogeneration of text to display is to be
 * enabled.
 * @param {boolean} enable Whether to enable the feature.
 */
goog.ui.editor.LinkDialog.prototype.setAutogenFeatureEnabled = function(
    enable) {
  'use strict';
  this.autogenFeatureEnabled_ = enable;
};


/**
 * Checks if `str` contains {@code "nofollow"} as a separate word.
 * @param {string} str String to be tested.  This is usually `rel`
 *     attribute of an `HTMLAnchorElement` object.
 * @return {boolean} `true` if `str` contains `nofollow`.
 */
goog.ui.editor.LinkDialog.hasNoFollow = function(str) {
  'use strict';
  return goog.ui.editor.LinkDialog.NO_FOLLOW_REGEX_.test(str);
};


/**
 * Removes {@code "nofollow"} from `rel` if it's present as a separate
 * word.
 * @param {string} rel Input string.  This is usually `rel` attribute of
 *     an `HTMLAnchorElement` object.
 * @return {string} `rel` with any {@code "nofollow"} removed.
 */
goog.ui.editor.LinkDialog.removeNoFollow = function(rel) {
  'use strict';
  return rel.replace(goog.ui.editor.LinkDialog.NO_FOLLOW_REGEX_, '');
};


// *** Protected interface ************************************************** //


/** @override */
goog.ui.editor.LinkDialog.prototype.createDialogControl = function() {
  'use strict';
  const builder = new goog.ui.editor.AbstractDialog.Builder(this);
  builder.setTitle(goog.ui.editor.messages.MSG_EDIT_LINK)
      .setContent(this.createDialogContent_());
  return builder.build();
};


/**
 * Creates and returns the event object to be used when dispatching the OK
 * event to listeners based on which tab is currently selected and the contents
 * of the input fields of that tab.
 * @return {!goog.ui.editor.LinkDialog.OkEvent} The event object to be used when
 *     dispatching the OK event to listeners.
 * @protected
 * @override
 */
goog.ui.editor.LinkDialog.prototype.createOkEvent = function() {
  'use strict';
  if (this.tabPane_.getCurrentTabId() ==
      goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_TAB) {
    return this.createOkEventFromEmailTab_();
  } else {
    return this.createOkEventFromWebTab_();
  }
};


// *** Private implementation *********************************************** //


/**
 * Regular expression that matches `nofollow` value in an
 * {@code * HTMLAnchorElement}'s `rel` element.
 * @type {RegExp}
 * @private
 */
goog.ui.editor.LinkDialog.NO_FOLLOW_REGEX_ = /\bnofollow\b/i;


/**
 * Creates contents of this dialog.
 * @return {!Element} Contents of the dialog as a DOM element.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.createDialogContent_ = function() {
  'use strict';
  this.textToDisplayDiv_ =
      /** @type {!HTMLDivElement} */ (this.buildTextToDisplayDiv_());
  const content =
      this.dom.createDom(goog.dom.TagName.DIV, null, this.textToDisplayDiv_);

  this.tabPane_ =
      new goog.ui.editor.TabPane(this.dom, goog.ui.editor.messages.MSG_LINK_TO);
  this.registerDisposable(this.tabPane_);
  this.tabPane_.addTab(
      goog.ui.editor.LinkDialog.Id_.ON_WEB_TAB,
      goog.ui.editor.messages.MSG_ON_THE_WEB,
      goog.ui.editor.messages.MSG_ON_THE_WEB_TIP,
      goog.ui.editor.LinkDialog.BUTTON_GROUP_, this.buildTabOnTheWeb_());
  this.tabPane_.addTab(
      goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_TAB,
      goog.ui.editor.messages.MSG_EMAIL_ADDRESS,
      goog.ui.editor.messages.MSG_EMAIL_ADDRESS_TIP,
      goog.ui.editor.LinkDialog.BUTTON_GROUP_, this.buildTabEmailAddress_());
  this.tabPane_.render(content);

  this.eventHandler_.listen(
      this.tabPane_, goog.ui.Component.EventType.SELECT, this.onChangeTab_);

  if (this.showOpenLinkInNewWindow_) {
    content.appendChild(this.buildOpenInNewWindowDiv_());
  }
  if (this.showRelNoFollow_) {
    content.appendChild(this.buildRelNoFollowDiv_());
  }

  return content;
};


/**
 * Builds and returns the text to display section of the edit link dialog.
 * @return {!Element} A div element to be appended into the dialog div.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.editor.LinkDialog.prototype.buildTextToDisplayDiv_ = function() {
  'use strict';
  const table = this.dom.createTable(1, 2);
  table.cellSpacing = '0';
  table.cellPadding = '0';
  table.style.fontSize = '10pt';
  // Build the text to display input.
  const textToDisplayDiv = this.dom.createDom(goog.dom.TagName.DIV);
  const html = goog.html.SafeHtml.create(
      'span', {
        'style': {
          'position': 'relative',
          'bottom': '2px',
          'padding-right': '1px',
          'white-space': 'nowrap'
        },
        id: goog.ui.editor.LinkDialog.Id_.TEXT_TO_DISPLAY_LABEL
      },
      [goog.ui.editor.messages.MSG_TEXT_TO_DISPLAY, goog.string.Unicode.NBSP]);
  goog.dom.safe.setInnerHtml(table.rows[0].cells[0], html);
  this.textToDisplayInput_ = this.dom.createDom(
      goog.dom.TagName.INPUT,
      {id: goog.ui.editor.LinkDialog.Id_.TEXT_TO_DISPLAY});
  const textInput = this.textToDisplayInput_;
  // 98% prevents scroll bars in standards mode.
  // TODO(robbyw): Is this necessary for quirks mode?
  goog.style.setStyle(textInput, 'width', '98%');
  goog.style.setStyle(table.rows[0].cells[1], 'width', '100%');
  goog.dom.appendChild(table.rows[0].cells[1], textInput);

  goog.a11y.aria.setState(
      /** @type {!Element} */ (textInput), goog.a11y.aria.State.LABELLEDBY,
      goog.ui.editor.LinkDialog.Id_.TEXT_TO_DISPLAY_LABEL);
  textInput.value = this.targetLink_.getCurrentText();

  this.textInputHandler_ = new goog.events.InputHandler(textInput);
  this.registerDisposable(this.textInputHandler_);
  this.eventHandler_.listen(
      this.textInputHandler_, goog.events.InputHandler.EventType.INPUT,
      this.onTextToDisplayEdit_);

  goog.dom.appendChild(textToDisplayDiv, table);
  return textToDisplayDiv;
};


/**
 * Builds and returns the "checkbox to open the link in a new window" section of
 * the edit link dialog.
 * @return {!Element} A div element to be appended into the dialog div.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.buildOpenInNewWindowDiv_ = function() {
  'use strict';
  this.openInNewWindowCheckbox_ = this.dom.createDom(
      goog.dom.TagName.INPUT, {'type': goog.dom.InputType.CHECKBOX});
  return this.dom.createDom(
      goog.dom.TagName.DIV, null,
      this.dom.createDom(
          goog.dom.TagName.LABEL, null, this.openInNewWindowCheckbox_,
          goog.ui.editor.messages.MSG_OPEN_IN_NEW_WINDOW));
};


/**
 * Creates a DIV with a checkbox for {@code rel=nofollow} option.
 * @return {!Element} Newly created DIV element.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.buildRelNoFollowDiv_ = function() {
  'use strict';
  const formatter = new goog.html.SafeHtmlFormatter();
  /** @desc Checkbox text for adding 'rel=nofollow' attribute to a link. */
  const MSG_ADD_REL_NOFOLLOW_ATTR = goog.getMsg(
      'Add \'{$relNoFollow}\' attribute ({$linkStart}Learn more{$linkEnd})', {
        'relNoFollow': 'rel=nofollow',
        'linkStart': formatter.startTag('a', {
          'href': 'http://support.google.com/webmasters/bin/' +
              'answer.py?hl=en&answer=96569',
          'target': '_blank'
        }),
        'linkEnd': formatter.endTag('a')
      });

  this.relNoFollowCheckbox_ = this.dom.createDom(
      goog.dom.TagName.INPUT, {'type': goog.dom.InputType.CHECKBOX});
  return this.dom.createDom(
      goog.dom.TagName.DIV, null,
      this.dom.createDom(
          goog.dom.TagName.LABEL, null, this.relNoFollowCheckbox_,
          goog.dom.safeHtmlToNode(
              formatter.format(MSG_ADD_REL_NOFOLLOW_ATTR))));
};


/**
* Builds and returns the div containing the tab "On the web".
* @return {!Element} The div element containing the tab.
* @private
*/
goog.ui.editor.LinkDialog.prototype.buildTabOnTheWeb_ = function() {
  'use strict';
  const onTheWebDiv = this.dom.createElement(goog.dom.TagName.DIV);

  const headingDiv = this.dom.createDom(
      goog.dom.TagName.DIV, {},
      this.dom.createDom(
          goog.dom.TagName.B, {}, goog.ui.editor.messages.MSG_WHAT_URL));
  const urlInput = this.dom.createDom(goog.dom.TagName.INPUT, {
    id: goog.ui.editor.LinkDialog.Id_.ON_WEB_INPUT,
    className: goog.ui.editor.LinkDialog.TARGET_INPUT_CLASSNAME_
  });
  goog.a11y.aria.setState(
      urlInput, goog.a11y.aria.State.LABELLEDBY,
      goog.ui.editor.LinkDialog.Id_.ON_WEB_TAB);
  // IE throws on unknown values for type, but IE10+ supports type=url
  if (!goog.userAgent.IE || goog.userAgent.isVersionOrHigher('10')) {
    // On browsers that support Web Forms 2.0, allow autocompletion of URLs.
    urlInput.type = goog.dom.InputType.URL;
  }

  if (goog.editor.BrowserFeature.NEEDS_99_WIDTH_IN_STANDARDS_MODE &&
      goog.editor.node.isStandardsMode(urlInput)) {
    urlInput.style.width = '99%';
  }

  const inputDiv = this.dom.createDom(goog.dom.TagName.DIV, null, urlInput);

  this.urlInputHandler_ = new goog.events.InputHandler(urlInput);
  this.registerDisposable(this.urlInputHandler_);
  this.eventHandler_.listen(
      this.urlInputHandler_, goog.events.InputHandler.EventType.INPUT,
      this.onUrlOrEmailInputChange_);

  const testLink = new goog.ui.Button(
      goog.ui.editor.messages.MSG_TEST_THIS_LINK,
      goog.ui.LinkButtonRenderer.getInstance(), this.dom);
  testLink.render(inputDiv);
  testLink.getElement().style.marginTop = '1em';
  this.eventHandler_.listen(
      testLink, goog.ui.Component.EventType.ACTION, this.onWebTestLink_);

  // Build the "On the web" explanation text div.
  const explanationDiv = this.dom.createDom(
      goog.dom.TagName.DIV,
      goog.ui.editor.LinkDialog.EXPLANATION_TEXT_CLASSNAME_);
  goog.dom.safe.setInnerHtml(
      explanationDiv, goog.ui.editor.messages.getTrLinkExplanationSafeHtml());
  onTheWebDiv.appendChild(headingDiv);
  onTheWebDiv.appendChild(inputDiv);
  onTheWebDiv.appendChild(explanationDiv);

  return onTheWebDiv;
};


/**
 * Builds and returns the div containing the tab "Email address".
 * @return {!Element} the div element containing the tab.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.buildTabEmailAddress_ = function() {
  'use strict';
  const emailTab = this.dom.createDom(goog.dom.TagName.DIV);

  const headingDiv = this.dom.createDom(
      goog.dom.TagName.DIV, {},
      this.dom.createDom(
          goog.dom.TagName.B, {}, goog.ui.editor.messages.MSG_WHAT_EMAIL));
  goog.dom.appendChild(emailTab, headingDiv);
  const emailInput = this.dom.createDom(goog.dom.TagName.INPUT, {
    id: goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_INPUT,
    className: goog.ui.editor.LinkDialog.TARGET_INPUT_CLASSNAME_
  });
  goog.a11y.aria.setState(
      emailInput, goog.a11y.aria.State.LABELLEDBY,
      goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_TAB);

  if (goog.editor.BrowserFeature.NEEDS_99_WIDTH_IN_STANDARDS_MODE &&
      goog.editor.node.isStandardsMode(emailInput)) {
    // Standards mode sizes this too large.
    emailInput.style.width = '99%';
  }

  goog.dom.appendChild(emailTab, emailInput);

  this.emailInputHandler_ = new goog.events.InputHandler(emailInput);
  this.registerDisposable(this.emailInputHandler_);
  this.eventHandler_.listen(
      this.emailInputHandler_, goog.events.InputHandler.EventType.INPUT,
      this.onUrlOrEmailInputChange_);

  goog.dom.appendChild(
      emailTab,
      this.dom.createDom(
          goog.dom.TagName.DIV, {
            id: goog.ui.editor.LinkDialog.Id_.EMAIL_WARNING,
            className: goog.ui.editor.LinkDialog.EMAIL_WARNING_CLASSNAME_,
            style: 'visibility:hidden'
          },
          goog.ui.editor.messages.MSG_INVALID_EMAIL));

  if (this.emailWarning_) {
    const explanationDiv = this.dom.createDom(
        goog.dom.TagName.DIV,
        goog.ui.editor.LinkDialog.EXPLANATION_TEXT_CLASSNAME_);
    goog.dom.safe.setInnerHtml(explanationDiv, this.emailWarning_);
    goog.dom.appendChild(emailTab, explanationDiv);
  }
  return emailTab;
};


/**
 * Returns the url that the target points to.
 * @return {string} The url that the target points to.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.getTargetUrl_ = function() {
  'use strict';
  // Get the href-attribute through getAttribute() rather than the href property
  // because Google-Toolbar on Firefox with "Send with Gmail" turned on
  // modifies the href-property of 'mailto:' links but leaves the attribute
  // untouched.
  return this.targetLink_.getAnchor().getAttribute('href') || '';
};


/**
 * Selects the correct tab based on the URL, and fills in its inputs.
 * For new links, it suggests a url based on the link text.
 * @param {string} text The inner text of the link.
 * @param {string} url The href for the link.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.editor.LinkDialog.prototype.selectAppropriateTab_ = function(
    text, url) {
  'use strict';
  if (this.isNewLink_()) {
    // Newly created non-empty link: try to infer URL from the link text.
    this.guessUrlAndSelectTab_(text);
  } else if (goog.editor.Link.isMailto(url)) {
    // The link is for an email.
    this.tabPane_.setSelectedTabId(
        goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_TAB);
    this.dom.getElement(goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_INPUT)
        .value = url.substring(url.indexOf(':') + 1);
    this.setAutogenFlagFromCurInput_();
  } else {
    // No specific tab was appropriate, default to on the web tab.
    this.tabPane_.setSelectedTabId(goog.ui.editor.LinkDialog.Id_.ON_WEB_TAB);
    this.dom.getElement(goog.ui.editor.LinkDialog.Id_.ON_WEB_INPUT).value =
        this.isNewLink_() ? 'http://' : url;
    this.setAutogenFlagFromCurInput_();
  }
};


/**
 * Select a url/tab based on the link's text. This function is simply
 * the isNewLink_() == true case of selectAppropriateTab_().
 * @param {string} text The inner text of the link.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.editor.LinkDialog.prototype.guessUrlAndSelectTab_ = function(text) {
  'use strict';
  if (goog.editor.Link.isLikelyEmailAddress(text)) {
    // The text is for an email address.
    this.tabPane_.setSelectedTabId(
        goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_TAB);
    this.dom.getElement(goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_INPUT)
        .value = text;
    this.setAutogenFlag_(true);
    // TODO(user): Why disable right after enabling? What bug are we
    // working around?
    this.disableAutogenFlag_(true);
  } else if (goog.editor.Link.isLikelyUrl(text)) {
    // The text is for a web URL.
    this.tabPane_.setSelectedTabId(goog.ui.editor.LinkDialog.Id_.ON_WEB_TAB);
    this.dom.getElement(goog.ui.editor.LinkDialog.Id_.ON_WEB_INPUT).value =
        text;
    this.setAutogenFlag_(true);
    this.disableAutogenFlag_(true);
  } else {
    // No meaning could be deduced from text, choose a default tab.
    if (!this.targetLink_.getCurrentText()) {
      this.setAutogenFlag_(true);
    }
    this.tabPane_.setSelectedTabId(goog.ui.editor.LinkDialog.Id_.ON_WEB_TAB);
  }
};


/**
 * Called on a change to the url or email input. If either one of those tabs
 * is active, sets the OK button to enabled/disabled accordingly.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.editor.LinkDialog.prototype.syncOkButton_ = function() {
  'use strict';
  let inputValue;
  if (this.tabPane_.getCurrentTabId() ==
      goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_TAB) {
    inputValue =
        this.dom.getElement(goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_INPUT)
            .value;
    this.toggleInvalidEmailWarning_(
        inputValue != '' && !goog.editor.Link.isLikelyEmailAddress(inputValue));
  } else if (
      this.tabPane_.getCurrentTabId() ==
      goog.ui.editor.LinkDialog.Id_.ON_WEB_TAB) {
    inputValue =
        this.dom.getElement(goog.ui.editor.LinkDialog.Id_.ON_WEB_INPUT).value;
  } else {
    return;
  }
  this.getOkButtonElement().disabled =
      goog.string.isEmptyOrWhitespace(inputValue);
};


/**
 * Show/hide the Invalid Email Address warning.
 * @param {boolean} on Whether to show the warning.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.toggleInvalidEmailWarning_ = function(on) {
  'use strict';
  this.dom.getElement(goog.ui.editor.LinkDialog.Id_.EMAIL_WARNING)
      .style.visibility = (on ? 'visible' : 'hidden');
};


/**
 * Changes the autogenerateTextToDisplay flag so that text to
 * display stops autogenerating.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.onTextToDisplayEdit_ = function() {
  'use strict';
  const inputEmpty = this.textToDisplayInput_.value == '';
  if (inputEmpty) {
    this.setAutogenFlag_(true);
  } else {
    this.setAutogenFlagFromCurInput_();
  }
};


/**
 * The function called when hitting OK with the "On the web" tab current.
 * @return {!goog.ui.editor.LinkDialog.OkEvent} The event object to be used when
 *     dispatching the OK event to listeners.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.createOkEventFromWebTab_ = function() {
  'use strict';
  const input = /** @type {HTMLInputElement} */ (
      this.dom.getElement(goog.ui.editor.LinkDialog.Id_.ON_WEB_INPUT));
  let linkURL = input.value;
  if (goog.editor.Link.isLikelyEmailAddress(linkURL)) {
    // Make sure that if user types in an e-mail address, it becomes "mailto:".
    return this.createOkEventFromEmailTab_(
        goog.ui.editor.LinkDialog.Id_.ON_WEB_INPUT);
  } else {
    if (linkURL.search(/:/) < 0) {
      linkURL = 'http://' + goog.string.trimLeft(linkURL);
    }
    return this.createOkEventFromUrl_(linkURL);
  }
};


/**
 * The function called when hitting OK with the "email address" tab current.
 * @param {string=} opt_inputId Id of an alternate input to check.
 * @return {!goog.ui.editor.LinkDialog.OkEvent} The event object to be used when
 *     dispatching the OK event to listeners.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.editor.LinkDialog.prototype.createOkEventFromEmailTab_ = function(
    opt_inputId) {
  'use strict';
  let linkURL =
      this.dom
          .getElement(
              opt_inputId || goog.ui.editor.LinkDialog.Id_.EMAIL_ADDRESS_INPUT)
          .value;
  linkURL = 'mailto:' + linkURL;
  return this.createOkEventFromUrl_(linkURL);
};


/**
 * Function to test a link from the on the web tab.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.onWebTestLink_ = function() {
  'use strict';
  const input = /** @type {HTMLInputElement} */ (
      this.dom.getElement(goog.ui.editor.LinkDialog.Id_.ON_WEB_INPUT));
  let url = input.value;
  if (url.search(/:/) < 0) {
    url = 'http://' + goog.string.trimLeft(url);
  }
  if (this.dispatchEvent(
          new goog.ui.editor.LinkDialog.BeforeTestLinkEvent(url))) {
    const win = this.dom.getWindow();
    const size = goog.dom.getViewportSize(win);
    const openOptions = {
      target: '_blank',
      width: Math.max(size.width - 50, 50),
      height: Math.max(size.height - 50, 50),
      toolbar: true,
      scrollbars: true,
      location: true,
      statusbar: false,
      menubar: true,
      resizable: true,
      noreferrer: this.stopReferrerLeaks_,
      noopener: this.stopTabNabbing_
    };
    goog.window.open(url, openOptions, win);
  }
};


/**
 * Called whenever the url or email input is edited. If the text to display
 * matches the text to display, turn on auto. Otherwise if auto is on, update
 * the text to display based on the url.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.onUrlOrEmailInputChange_ = function() {
  'use strict';
  if (this.autogenerateTextToDisplay_) {
    this.setTextToDisplayFromAuto_();
  } else if (this.textToDisplayInput_.value == '') {
    this.setAutogenFlagFromCurInput_();
  }
  this.syncOkButton_();
};


/**
 * Called when the currently selected tab changes.
 * @param {goog.events.Event} e The tab change event.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.onChangeTab_ = function(e) {
  'use strict';
  const tab = /** @type {goog.ui.Tab} */ (e.target);

  // Focus on the input field in the selected tab.
  const input = /** @type {!HTMLElement} */ (this.dom.getElement(
      tab.getId() + goog.ui.editor.LinkDialog.Id_.TAB_INPUT_SUFFIX));
  goog.editor.focus.focusInputField(input);

  // For some reason, IE does not fire onpropertychange events when the width
  // is specified as a percentage, which breaks the InputHandlers.
  input.style.width = input.offsetWidth + 'px';

  this.syncOkButton_();
  this.setTextToDisplayFromAuto_();
};


/**
 * If autogen is turned on, set the value of text to display based on the
 * current selection or url.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.setTextToDisplayFromAuto_ = function() {
  'use strict';
  if (this.autogenFeatureEnabled_ && this.autogenerateTextToDisplay_) {
    const inputId = this.tabPane_.getCurrentTabId() +
        goog.ui.editor.LinkDialog.Id_.TAB_INPUT_SUFFIX;
    this.textToDisplayInput_.value =
        /** @type {HTMLInputElement} */ (this.dom.getElement(inputId)).value;
  }
};


/**
 * Turn on the autogenerate text to display flag, and set some sort of indicator
 * that autogen is on.
 * @param {boolean} val Boolean value to set autogenerate to.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.setAutogenFlag_ = function(val) {
  'use strict';
  // TODO(user): This whole autogen thing is very confusing. It needs
  // to be refactored and/or explained.
  this.autogenerateTextToDisplay_ = val;
};


/**
 * Disables autogen so that onUrlOrEmailInputChange_ doesn't act in cases
 * that are undesirable.
 * @param {boolean} autogen Boolean value to set disableAutogen to.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.disableAutogenFlag_ = function(autogen) {
  'use strict';
  this.setAutogenFlag_(!autogen);
  this.disableAutogen_ = autogen;
};


/**
 * Creates an OK event from the text to display input and the specified link.
 * If text to display input is empty, then generate the auto value for it.
 * @return {!goog.ui.editor.LinkDialog.OkEvent} The event object to be used when
 *     dispatching the OK event to listeners.
 * @param {string} url Url the target element should point to.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.createOkEventFromUrl_ = function(url) {
  'use strict';
  // Fill in the text to display input in case it is empty.
  this.setTextToDisplayFromAuto_();
  if (this.showOpenLinkInNewWindow_) {
    // Save checkbox state for next time.
    this.isOpenLinkInNewWindowChecked_ = this.openInNewWindowCheckbox_.checked;
  }
  return new goog.ui.editor.LinkDialog.OkEvent(
      this.textToDisplayInput_.value, url,
      this.showOpenLinkInNewWindow_ && this.isOpenLinkInNewWindowChecked_,
      this.showRelNoFollow_ && this.relNoFollowCheckbox_.checked);
};


/**
 * If an email or url is being edited, set autogenerate to on if the text to
 * display matches the url.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.ui.editor.LinkDialog.prototype.setAutogenFlagFromCurInput_ = function() {
  'use strict';
  let autogen = false;
  if (!this.disableAutogen_) {
    const tabInput = this.dom.getElement(
        this.tabPane_.getCurrentTabId() +
        goog.ui.editor.LinkDialog.Id_.TAB_INPUT_SUFFIX);
    autogen = (tabInput.value == this.textToDisplayInput_.value);
  }
  this.setAutogenFlag_(autogen);
};


/**
 * @return {boolean} Whether the link is new.
 * @private
 */
goog.ui.editor.LinkDialog.prototype.isNewLink_ = function() {
  'use strict';
  return this.targetLink_.isNew();
};


/**
 * IDs for relevant DOM elements.
 * @enum {string}
 * @private
 */
goog.ui.editor.LinkDialog.Id_ = {
  TEXT_TO_DISPLAY: 'linkdialog-text',
  TEXT_TO_DISPLAY_LABEL: 'linkdialog-text-label',
  ON_WEB_TAB: 'linkdialog-onweb',
  ON_WEB_INPUT: 'linkdialog-onweb-tab-input',
  EMAIL_ADDRESS_TAB: 'linkdialog-email',
  EMAIL_ADDRESS_INPUT: 'linkdialog-email-tab-input',
  EMAIL_WARNING: 'linkdialog-email-warning',
  TAB_INPUT_SUFFIX: '-tab-input'
};


/**
 * Base name for the radio buttons group.
 * @type {string}
 * @private
 */
goog.ui.editor.LinkDialog.BUTTON_GROUP_ = 'linkdialog-buttons';


/**
 * Class name for the url and email input elements.
 * @type {string}
 * @private
 */
goog.ui.editor.LinkDialog.TARGET_INPUT_CLASSNAME_ =
    goog.getCssName('tr-link-dialog-target-input');


/**
 * Class name for the email address warning element.
 * @type {string}
 * @private
 */
goog.ui.editor.LinkDialog.EMAIL_WARNING_CLASSNAME_ =
    goog.getCssName('tr-link-dialog-email-warning');


/**
 * Class name for the explanation text elements.
 * @type {string}
 * @private
 */
goog.ui.editor.LinkDialog.EXPLANATION_TEXT_CLASSNAME_ =
    goog.getCssName('tr-link-dialog-explanation-text');
