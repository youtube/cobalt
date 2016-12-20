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

$(document).ready(function() {
  /**
   * Rewrite links to serve correctly on cobalt.foo.
   */
  var letsPretend = true;
  if (window.location.hostname.indexOf('cobalt.foo') !== -1 || letsPretend == true) { 
    $('a').each(function() {
      var href = this.href;
      console.log(this.href);
      var newHref = href.replace(/\/cobalt/, '');
      console.log('nh2: ' + newHref);
    });
  }
});
