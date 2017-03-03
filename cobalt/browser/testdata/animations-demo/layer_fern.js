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
cssFile.setAttribute('href', 'layer_fern.css');
document.getElementsByTagName("head")[0].appendChild(cssFile)

function CreateStem() {
  var fernStem = document.createElement('div');
  fernStem.className = 'fern-stem';
  return fernStem;
}

function CreateStick() {
  var fernStick = document.createElement('div');
  fernStick.className = 'fern-stick';
  return fernStick;
}

function CreateFernNode(className, scale) {
  var fernNode = document.createElement('div');
  fernNode.className = className;
  AppendFern(fernNode, scale);
  return fernNode;
}

function AppendFern(container, scale) {
  if (scale <= 0.04) {
    container.appendChild(CreateStick());
    return;
  }

  container.appendChild(
      CreateFernNode('fern-top-node', scale * 0.85));
  container.appendChild(
      CreateFernNode('fern-left-node', scale * 0.14));
  container.appendChild(
      CreateFernNode('fern-right-node', scale * 0.14));
  container.appendChild(CreateStem());
}

function InitializeFernLayer(layerElement) {
  var fernContainer = document.createElement('div');
  fernContainer.className = 'fern-container';

  AppendFern(fernContainer, 0.28);
  layerElement.appendChild(fernContainer);
}

