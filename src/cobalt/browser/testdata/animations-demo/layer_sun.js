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
cssFile.setAttribute('href', 'layer_sun.css');
document.getElementsByTagName("head")[0].appendChild(cssFile)

function CreateSunBlade(className, duration, delay) {
  var sunBladeElement = document.createElement('div');
  sunBladeElement.className = className;
  sunBladeElement.style.animationDuration =
      duration + 's, 0.4s';
  sunBladeElement.style.animationDelay = -delay + 's, ' + -duration + 's';
  return sunBladeElement;
}

function CreateSunElement(durationMultiplier) {
  var sunBladeScaleContainerElement = document.createElement('div');
  sunBladeScaleContainerElement.className = 'sun-blade-scale-container';

  var NUM_SUN_BLADE_ELEMENTS = 3;
  var rotationPeriodInSeconds = 4 * durationMultiplier;
  var sunBladeSpacing =
      (1.0 / NUM_SUN_BLADE_ELEMENTS) * rotationPeriodInSeconds;

  for (var i = 0; i < NUM_SUN_BLADE_ELEMENTS; ++i) {
    var sunBladeElement = CreateSunBlade(
        'sun-blade-dark',
        rotationPeriodInSeconds, i * sunBladeSpacing + 0.25 * sunBladeSpacing);
    sunBladeScaleContainerElement.appendChild(sunBladeElement);
  }

  for (var i = 0; i < NUM_SUN_BLADE_ELEMENTS; ++i) {
    var sunBladeElement = CreateSunBlade(
        'sun-blade-light',
        rotationPeriodInSeconds, i * sunBladeSpacing);

    sunBladeScaleContainerElement.appendChild(sunBladeElement);
  }

  return sunBladeScaleContainerElement;
}

function InitializeSunLayer(layerElement) {
  var sunBladeLayerContainerElement = document.createElement('div');
  sunBladeLayerContainerElement.className = 'sun-blade-layer-container';

  var parentElement = sunBladeLayerContainerElement;
  var durationMultiplier = 1.0;
  var NUM_SUNS = 7;
  for (var i = 0; i < NUM_SUNS; ++i) {
    sunElement = CreateSunElement(durationMultiplier);
    parentElement.appendChild(sunElement);
    parentElement = sunElement;
    durationMultiplier *= 0.8;
  }
  layerElement.appendChild(sunBladeLayerContainerElement);
}

