/*
 * Copyright (c) 2013 Alex Ghiculescu
 *               2016 Google Inc.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

(function($){
  $.fn.toc = function(options) {
    var tocDepth = $('#toc-depth').attr('data-toc-depth') || ''
    var tocHeader = $('#toc-header').attr('data-toc-header') || 'Contents'
    var shareButtonElement = $('#rightCol-share-buttons') || ''
    var defaults = {
      noBackToTopLinks: false,
      title: '<b>' + tocHeader + '</b>',
      minimumHeaders: 3,
      headers: tocDepth || 'h1, h2, h3, h4, h5, h6',
      listType: 'ol', // values: [ol|ul]
      showEffect: 'show', // values: [show|slideDown|fadeIn|none]
      showSpeed: 'slow' // set to 0 to deactivate effect
    },
    settings = $.extend(defaults, options);

    function fixedEncodeURIComponent (str) {
      return encodeURIComponent(str).replace(/[!'()*]/g, function(c) {
        return '%' + c.charCodeAt(0).toString(16);
      });
    }

    var headers = $(settings.headers).filter(function() {
      // get all headers with an ID
      var previousSiblingName = $(this).prev().attr( 'name' );
      if (!this.id && previousSiblingName) {
        this.id = $(this).attr( 'id', previousSiblingName.replace(/\./g, '-') );
      }
      return this.id;
    }), output = $(this);
    if (!headers.length || headers.length < settings.minimumHeaders || !output.length) {
      if (shareButtonElement) {
        shareButtonElement.removeClass('share-buttons-bottom');
      }
      return;
    }

    if (0 === settings.showSpeed) {
      settings.showEffect = 'none';
    }

    var render = {
      show: function() { output.hide().html(html).show(settings.showSpeed); },
      slideDown: function() { output.hide().html(html).slideDown(settings.showSpeed); },
      fadeIn: function() { output.hide().html(html).fadeIn(settings.showSpeed); },
      none: function() { output.html(html); }
    };

    var get_level = function(ele) { return parseInt(ele.nodeName.replace('H', ''), 10); }
    var highest_level = headers.map(function(_, ele) { return get_level(ele); }).get().sort()[0];
    var return_to_top = '<i class="icon-arrow-up back-to-top"> </i>';

    var level = get_level(headers[0]),
      this_level,
      html = ('<a href="#top_of_page" id="toc-contents-header" data-title="' +
              settings.title + '">' + settings.title +
              '</a><'+settings.listType+'>');
    headers.on('click', function() {
      if (!settings.noBackToTopLinks) {
        window.location.hash = this.id;
      }
    })
    .addClass('clickable-header')
    .each(function(_, header) {
      this_level = get_level(header);
      if (!settings.noBackToTopLinks && this_level === highest_level) {
        $(header).addClass('top-level-header').after(return_to_top);
      }
      // same level as before; same indenting
      if (this_level === level)
        html += ("<li><a href='#" + fixedEncodeURIComponent(header.id) + "'>" +
                 header.innerHTML + "</a>");
      // higher level than before; end parent ol
      else if (this_level <= level){
        for(i = this_level; i < level; i++) {
          html += "</li></"+settings.listType+">"
        }
        html += ("<li><a href='#" + fixedEncodeURIComponent(header.id) + "'>" +
                 header.innerHTML + "</a>");
      }
      // lower level than before; expand the previous to contain a ol
      else if (this_level > level) {
        for(i = this_level; i > level; i--) {
          html += "<"+settings.listType+"><li>"
        }
        html += ("<a href='#" + fixedEncodeURIComponent(header.id) + "'>" +
                 header.innerHTML + "</a>");
      }
      // update to compare to next header
      level = this_level;
    });
    html += "</"+settings.listType+">";
    if (!settings.noBackToTopLinks) {
      $(document).on('click', '.back-to-top', function() {
        $(window).scrollTop(0);
        window.location.hash = '';
      });
    }

    render[settings.showEffect]();
  };

})(jQuery);
