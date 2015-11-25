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
