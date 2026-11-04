FROM gcr.io/skia-public/skottie-base@sha256:2027949950ed3f1dd3e4f7a081162727813b4667cd6b67a8d4333ec61078b488

COPY canvaskit.js /usr/local/share/skottie/canvaskit.js
COPY canvaskit.wasm /usr/local/share/skottie/canvaskit.wasm
COPY version.js /usr/local/share/skottie/version.js

CMD ["/usr/local/bin/skottie", "--help"]