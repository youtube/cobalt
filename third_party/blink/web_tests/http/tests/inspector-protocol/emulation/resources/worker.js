onmessage = function(e) {
  postMessage({'language': navigator.language});
  close();
};
