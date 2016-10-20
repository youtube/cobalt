/**
 * Copyright 2016 Google Inc. All Rights Reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/** This function handles the following tasks:
 *  1. Call the jekyll-table-of-contents script to generate the TOC
 *  2. Highlight the current document in the left nav
 *  3. Fix the left and right nav so they remain visible while scrolling
 *  4. Highlight current section in TOC with ScrollSpy
 */
$(document).ready(function() {

  /** Generated table of contents */
  $.getScript("/cobalt/assets/js/jekyll-table-of-contents.js", function() {
    $('#toc').toc({ listType: 'ul' });
    $('#standard-menu-links>ul:first-child').
        addClass('nav navbar-nav navbar-right');

    /** Highlight active section in right nav */
    var $body = $(document.body);
    var navHeight = $('.navbar').outerHeight(true) + 100;

    $body.scrollspy({
      target: '#rightCol',
      offset: navHeight
    });
  });

  /**
   * When linking to anchor on page, allow for fixed header so that linked
   * element does not appear behind anchor. This function handles scrolling
   * when the page loads if the target URL includes a hash (#) that points
   * to a defined element. It also redefines the onclick behavior for links
   * (<a> elements) that contain a hash in the target URL (href attribute
   * value).
   */

  // Set offset height (height of fixed header plus padding) and
  // scroll speed (number of milliseconds to scroll to target element).
  // Default OFFSET_HEIGHT is 65px. Default OFFSET_SCROLL_SPEED is 400ms.
  // The OFFSET_SCROLL_SPEED can be set in the config file like this:
  //    anchors:
  //      offset-scroll-speed: 1000
  // The value is specified in milliseconds.
  var OFFSET_HEIGHT = 65;

  var scroll_speed_from_config;
  scroll_speed_from_config = "";
  var OFFSET_SCROLL_SPEED;
  if (scroll_speed_from_config &&
     (scroll_speed_from_config == 'fast' ||
      scroll_speed_from_config == 'slow')) {
    OFFSET_SCROLL_SPEED = scroll_speed_from_config;
  } else {
    OFFSET_SCROLL_SPEED = scroll_speed_from_config ?
        parseInt(scroll_speed_from_config, 10) : 400;
  }
 
  /**
   * Scroll to target element at desired scroll speed. Adjust for offset
   *    height.
   * @param {Object} target The element being linked to.
   * @param {string} [location_hash=null] - The hash to append to the URL in
   *     the navigation bar.
   * @return {boolean} Return false to prevent default browser behavior.
   */
  function scrollToTarget(target, location_hash=null) {
    $('html,body').stop().animate({
      scrollTop: target.offset().top - OFFSET_HEIGHT
    }, OFFSET_SCROLL_SPEED);
    if (location_hash) {
      // Set hash manually since "return false" prevents default behavior.
      window.location.hash = location_hash;
    }
    return false;
  }

  function checkLinkBeforeScroll(link) {
    // If an element matches the target link hash and the target host and
    // page match the current host and page, scroll to the element.
    if ($(link.hash) &&
        location.hostname == link.hostname &&
        link.pathname.replace(/^\//, '') ==
            location.pathname.replace(/^\//, '')
       ) {
      // Specify hash as parameter to modify navigation bar.
      scrollToTarget($(link.hash), link.hash);
    }
  }
  
  // Scroll to anchor tag on page load.
  if (location.hash && $(location.hash)) {
    scrollToTarget($(location.hash));
  }

  $('#toc').on('click', 'a[href*=#]:not([href=#])', function() {
    checkLinkBeforeScroll(this);
  });
  
  /**
   * Apply click functions to all anchors that do not link to '#'
   */
  $('a[href*=#]:not([href=#])').click(function() {
    // If an element matches the target link hash and the target host and
    // page match the current host and page, scroll to the element.
    if (!$(this).hasClass('mdl-tabs__tab')) {
      console.log(this);
      checkLinkBeforeScroll(this);
    }
  });

  $('#sidebar li').
      children('b').
      parent().
          addClass('expand-container');
  $('#sidebar li:has(> p > b)').
      addClass('expand-container');

  var url = window.location.origin + window.location.pathname;
  /** Highlight the active document in left nav
   * closest expandable to active item, contains header + item list -- e.g.:
   * <li class="expand-container">
   *   <strong class="expanded|collapsed">Section header</strong>
   *   <ul> [containing list with active item </ul>
   *
   * This function runs on links that are direct children of the <li>
   * element and of links that are children of a <p> element inside the <li>.
   */
  function highlightActive() {
    // Add active-item class to <li>
    this.closest('li').addClass('active-item');

    // Display all lists that contain the active item among their descendants.
    $('#sidebar li.expand-container ul:has(li.active-item)').
        css('display', 'block');

    // Fix collapsed/expanded class on <strong> tags in active-item's path.
    $('#sidebar li.active-item').
        parents('li.expand-container').
        each(function() {
          $(this).children('b').
              removeClass('collapsed').
              addClass('expanded');
          $(this).children('p').
              children('b').
              removeClass('collapsed').
              addClass('expanded');
        });
  }

  highlightActive.call($('#sidebar a').filter(function() {
    if (this.href + 'index.md' == url) {
      return this.href + 'index.md' == url;
    }
    return this.href == url;
  }));

  /** Collapsible menus in left nav */
  $.getScript("/cobalt/assets/js/simple-expand.js", function() {
    $('#sidebar li ul li b').simpleexpand({'defaultTarget': 'ul'});
  });

  /** Highlight active document in left nav */
  /*
  var url = window.location.href.split('#')[0];
  $('a').filter(function() {
    if (((this.href + 'index.html') == url) ||
         (this.href == (url + 'index.html'))) {
      return true;
    }
    return this.href == url;
  }).closest('li').addClass('active-item'); // parent should be <li> or <h4>
  */

  /** Collapsible menus in left nav */
  /*
  $.getScript("/assets/js/simple-expand.js", function() {
    $('.submenu').simpleexpand();
    $('.active-item').closest('ul.content').css('display', 'block')
      .siblings('h4').removeClass('collapsed').addClass('expanded');
  });
  */

  /** Affix side menus */
  $('#sidebar').affix({
    offset: {
      top: 110
    }
  });
  $('#tocSidebar').affix({
    offset: {
      top: 110
    }
  });

  /** Style outbound links */
  /**
   * This function is commented out by default. When it does run, it adds
   * the 'external' CSS class to every outbound link on the site.
   */
  //$("a").filter(function() {
  //  return (this.hostname && this.hostname !== location.hostname);
  //}).addClass("external");

  /** Prettyprint code blocks */
  $('pre').addClass("prettyprint");
});


function getButtonDetails(service, siteUrl, pageUrl, pageTitle) {
  pageUrl = siteUrl + pageUrl;
  redirectUrl = siteUrl + "/redirects/site-toolkit-close-redirect.html";
  var escapedUrl = encodeURIComponent(pageUrl);
  var escapedRedirectUrl = encodeURIComponent(redirectUrl);
  var escapedTitle = encodeURIComponent(pageTitle);

  var facebookAppId = "";
  /*
   * The shell script that creates the site replaces the commented line
   * starting with "INSERT FACEBOOK_APP_ID" with the
   * sharing.facebook.facebook-app-id value from the site config file.
   * The JS then checks to make sure a value is actually set before trying
   * to show a Facebook button since an app ID is a prerequisite to the
   * button working correctly.
   */
  facebookAppId = "";

  var details = {};

  switch(service) {
    // Seems to contain values specific to video sharing
    case 'ameba':
      details = {'name': 'Ameba',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://blog.ameba.jp/ucs/entry/srventryinsertinput.do?entry_text=%(escaped_iframe_embedded_player_html)s&editor_flg=1'};
      break;
    // TUUID and MID appear company-specific?
    case 'bebo':
      details = {'name': 'Bebo',
                 'popupHeight': 436,
                 'popupWidth': 626,
                 'url': 'http://www.bebo.com/c/share?Url=' + escapedUrl +
                        '&Title=' + escapedTitle +
                        '&TUUID=c583051f-6b2d-41ec-8dd0-a3a0ee1656c1' +
                        '&MID=8348657161'};
      break;
    // source parameter and some video-specific stuff
    case 'blogger':
      details = {'name': 'Blogger',
                 'popupHeight': 468,
                 'popupWidth': 768,
                 'url': 'http://www.blogger.com/blog-this.g?n=' + escapedTitle + '&source=youtube&b=%(escaped_iframe_embedded_player_html)s&eurl=%(escaped_image)s'};
      break;
    case 'cyworld':
      details = {'name': 'Cyworld',
                 'popupHeight': 410,
                 'popupWidth': 450,
                 'url': 'http://api.cyworld.com/openscrap/video/v1/?vu=' +
                        escapedUrl};
      break;
    // company parameter specifies YT
    case 'delicious':
      details = {'name': 'Delicious',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://delicious.com/save?noui&v=5s&jump=close&url=' +
                        escapedUrl + '&title=' + escapedTitle +
                        '&company=youtube',
                 'rightNavIcon': 'delicious'};
      break;
    case 'digg':
      details = {'name': 'Digg',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://digg.com/submit?url=' + escapedUrl,
                 'rightNavIcon': 'digg'};
      break;
    case 'facebook':
      if (facebookAppId) {
        details = {'name': 'Facebook',
                   'popupHeight': 560,
                   'popupWidth': 530,
                   'url': 'https://www.facebook.com/dialog/share?app_id=' +
                          '&href=' + escapedUrl +
                          '&display=popup&redirect_uri=' + escapedRedirectUrl,
                   'rightNavIcon': 'facebook'};
      } else {
        details = {};
      }
      break;
    case 'fotka':
      details = {'name': 'Fotka',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.fotka.pl/out/opis_importer.php?url=' +
                        escapedUrl};
      break;
    // Might only be video sharing service?
    case 'goo':
      details = {'name': 'goo',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://blog.goo.ne.jp/portal_login/blogparts/?key=%(escaped_vid)s&title=%(escaped_long_title)s&type=youtube'};
      break;
    // TODO: Not sure what googleplus_url is or soc-app param, source param, escaped_hl
    case 'google_plus':
      details = {'name': 'Google+',
                 'popupHeight': 620,
                 'popupWidth': 620,
                 'url': 'https://plus.google.com/share?url=' + escapedUrl,
                 'rightNavIcon': 'google-plus'};
      break;
    // embeddable, simple params
    case 'hi5':
      details = {'name': 'hi5',
                 'popupHeight': 475,
                 'popupWidth': 800,
                 'url': 'http://www.hi5.com/friend/checkViewedVideo.do?t=' +
                        escapedTitle + '&url=' + escapedUrl +
                        '&embeddable=true&simple=true'};
      break;
    // html param, uargs?, video-specific param
    case 'hyves':
      details = {'name': 'Hyves',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.hyves-share.nl/button/gadget/',
                 'uargs': 'title=' + escapedTitle + '&html=%(escaped_iframe_embedded_player_html)s'};
      break;
    case 'kakao':
      details = {'name': '카카오스토리',
                 'popupHeight': 530,
                 'popupWidth': 480,
                 'url': 'https://story.kakao.com/share?url=' + escapedUrl};
      break;
    case 'linkedin':
      details = {'name': 'LinkedIn',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.linkedin.com/shareArticle?url=' + escapedUrl + '&title=' + escapedTitle,
                 'rightNavIcon': 'linkedin'};
      break;
    case 'livejournal':
      details = {'name': 'LiveJournal',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.livejournal.com/update.bml?url=' +
                 escapedUrl + '&subject=' + escapedTitle};
      break;
    // what is shortened_url
    case 'mail':
      details = {'name': 'Mail',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': '%(shortened_url)'};
      break;
    case 'meneame':
      details = {'name': 'menéame',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://meneame.net/submit.php?url=' + escapedUrl};
      break;
    // k parameter seems like it might required some kind of ID
    case 'mixi':
      details = {'name': 'mixi',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://mixi.jp/share.pl?u=' + escapedUrl +
                        '&k=0033257f963abb6e88b20b7671fb2fbfa5d31125'};
      break;
    // partner param in URL
    case 'mixx':
      details = {'name': 'Mixx',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.mixx.com/submit/video?partner=YT' +
                        '&page_url=' + escapedUrl};
      break;
    case 'myspace':
      details = {'name': 'Myspace',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.myspace.com/Modules/PostTo/Pages/?t=' +
                      escapedTitle + '&u=' + escapedUrl + '&l=1'};
      break;
    case 'naver':
      details = {'name': 'Naver',
                 'popupHeight': 500,
                 'popupWidth': 600,
                 'url': 'http://blog.naver.com/LinkShare.nhn?url=' +
                        escapedUrl};
      break;
    case 'nujij':
      details = {'name': 'NUjij',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://nujij.nl/jij.lynkx?t=' + escapedTitle +
                        '&u=' + escapedUrl};
      break;
    case 'odnoklassniki':
      details = {'name': 'Одноклассники',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.odnoklassniki.ru/dk?st.cmd=addShare' +
                        '&st.noresize=on&st._surl=' + escapedUrl,
                 'rightNavIcon': 'odnoklassniki'};
      break;
    // Need to modify so you're pinning a page, not an image/video
    case 'pinterest':
      details = {'name': 'Pinterest',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://pinterest.com/pin/create/button/?url=' +
                        escapedUrl + '&description=' + escapedTitle,
                 'rightNavIcon': 'pinterest'};
      break;
    // Everything in URL is about embedding a video
    case 'rakuten':
      details = {'name': '楽天ブログ',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://my.plaza.rakuten.co.jp/index.phtml?func=diary&act=write&video_type=youtube&video_code=%(escaped_vid)s'};
      break;
    case 'reddit':
      details = {'name': 'reddit',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://reddit.com/submit?url=' + escapedUrl +
                        '&title=' + escapedTitle,
                 'rightNavIcon': 'reddit'};
      break;
    // only for embedding video?
    case 'skyblog':
      details = {'name': 'Skyrock',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://skyrock.com/m/blog/share-widget.php?idp=10&idm=%(escaped_vid)s&title=' + escapedTitle};
      break;
    case 'sledzik':
      details = {'name': 'nk.pl',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://nasza-klasa.pl/sledzik?shout=' + escapedUrl};
      break;
    case 'stumbleupon':
      details = {'name': 'StumbleUpon',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.stumbleupon.com/submit?url=' + escapedUrl +
                        '&title=' + escapedTitle,
                 'rightNavIcon': 'stumbleupon'};
      break;
    case 'tuenti':
      details = {'name': 'Tuenti',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.tuenti.com/share?url=' + escapedUrl};
      break;
    // can you embed something other than video?
    case 'tumblr':
      details = {'name': 'Tumblr',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.tumblr.com/share/link?url=' + escapedUrl + '&name=' + escapedTitle,
                 'rightNavIcon': 'tumblr'};
      break;
    // escaped_shortened_url? via? related?
    case 'twitter':
      details = {'name': 'Twitter',
                 'popupHeight': 420,
                 'popupWidth': 550,
                 'url': 'http://twitter.com/intent/tweet?url=' + escapedUrl + '&text=' + escapedTitle,
                 'rightNavIcon': 'twitter'};// + '&via=%(escaped_twitter_via)s};
                 //'url': 'http://twitter.com/intent/tweet?url=%(escaped_shortened_url)s&text=' + escapedTitle + '&via=%(escaped_twitter_via)s&related=YouTube,YouTubeTrends,YTCreators'};
      break;
    case 'vkontakte':
      details = {'name': 'ВКонтакте',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://vkontakte.ru/share.php?url=' + escapedUrl,
                 'rightNavIcon': 'vk'};
      break;
    case 'weibo':
      details = {'name': 'Weibo',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://service.weibo.com/share/share.php?url=' +
                        escapedUrl + '&appkey=&title=' + escapedTitle,
                 'rightNavIcon': 'weibo'};
      break;
    // escaped description
    case 'wykop':
      details = {'name': 'Wykop',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://www.wykop.pl/dodaj?url=' + escapedUrl +
                        '&title=' + escapedTitle};
                 // + '&desc=%(escaped_description)s'};
      break;
    // type parameter needs to be changed, other values of unknown origin
    case 'yahoo':
      details = {'name': 'Yahoo! Japan',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://blogs.yahoo.co.jp/FRONT/blogthis.html?type=youtube&item=%(escaped_embed_url)s&itemwidth=%(escaped_embed_width)s&itemheight=%(escaped_embed_height)s&link=' + escapedUrl + '&linktitle=' + escapedTitle,
                 'rightNavIcon': 'yahoo'};
      break;
    case 'yigg':
      details = {'name': 'YiGG',
                 'popupHeight': null,
                 'popupWidth': null,
                 'url': 'http://yigg.de/neu?exturl=' + escapedUrl +
                        '&exttitle=' + escapedTitle};
      break;
  }
  return details;
}

// Create a social media service sharing button.
// NOTE: Facebook buttons only display if you have set a facebook-app-id
// in your config file.
function createShareButton(service, siteUrl, pageUrl, pageTitle,
                           buttonLocation) {
  var buttonDetails = getButtonDetails(service, siteUrl, pageUrl, pageTitle);

  // If there is no URL for the button, don't try to show it.
  if (!buttonDetails['url']) {
    return;
  }
  // buttonDetails['rightNavIcon'] is the property for a right-column share
  // button icon. If it doesn't exist, there's no icon and no button for
  // that service.
  if (buttonLocation == 'right' && !buttonDetails['rightNavIcon']) {
    return;
  }

  var buttonHeight = buttonDetails['popupHeight'] ?
      buttonDetails['popupHeight'] : 650;
  var buttonWidth = buttonDetails['popupWidth'] ?
      buttonDetails['popupWidth'] : 1024;

  // All share buttons are added to the footer's sharing-services element.
  var parentDiv = document.getElementById('sharing-services');
  if (buttonLocation == 'right') {
    parentDiv = document.getElementById('rightCol-share-buttons');
  }

  // Create button element for share button. The element defines what happens
  // when a user clicks on the button.
  var button;
  if (buttonLocation == 'footer') {
    button = document.createElement('button');
  } else if (buttonLocation == 'right') {
    button = document.createElement('i');
    button.className = 'fa fa-' + buttonDetails['rightNavIcon'];
  }
  var buttonTitle = ('Share to ' + buttonDetails['name'] + '. ' +
                     'Opens in a new window.');
  button.title = buttonTitle;
  button.setAttribute('data-tooltip-text', buttonTitle);
  button.setAttribute('onclick', 'window.open("' +
      buttonDetails['url'] + '", ' + '"", ' +
      '"height=' + buttonHeight + ', width=' + buttonWidth + ', ' +
      'scrollbars=true");');

  // Add the CSS class for footer buttons.
  if (buttonLocation == 'footer') {
    // Create div element that specifies CSS class for sharing service.
    // The CSS class identifies the background image for the div.
    var div = document.createElement('div');
    div.setAttribute('class', 'sharing-service-' + service);

    // Add div element to button
    button.appendChild(div);
  }

  // Add button to sharing-services div
  parentDiv.appendChild(button);
}
