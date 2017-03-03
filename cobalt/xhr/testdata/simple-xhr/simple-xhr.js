// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

var xhr = new XMLHttpRequest();
xhr.onreadystatechange = function() {
  var cobalt = document.createElement('span');
  if(xhr.readyState == 1) {
    cobalt.textContent = "...Opened";
  }
  else if (xhr.readyState == 2) {
    cobalt.textContent = "...HeadersReceived";
  }
  else if (xhr.readyState == 3) {
    cobalt.textContent = "...Loading";
  }
  else if (xhr.readyState == 4) {
    cobalt.textContent = "...Loaded.";
    var headers = document.createElement('span');
    headers.textContent = "Headers: " + xhr.getAllResponseHeaders();
    document.body.appendChild(headers);
  }
  document.body.appendChild(cobalt);
}

xhr.open("GET", "https://www.youtube.com/tv");
xhr.send();
