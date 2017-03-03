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

var cssFile = document.createElement('link');
cssFile.setAttribute('rel', 'stylesheet');
cssFile.setAttribute('type', 'text/css');
cssFile.setAttribute('href', 'layer_intro.css');
document.getElementsByTagName("head")[0].appendChild(cssFile)

function InitializeIntroLayer(layerElement) {
  var introContainer = document.createElement('div');
  introContainer.className = 'intro-container';

  var textElement = document.createElement('div');
  textElement.innerHTML = 'Cobalt CSS Animations Demo';
  textElement.className = 'intro-text';

  introContainer.appendChild(textElement);

  layerElement.appendChild(introContainer);
}
