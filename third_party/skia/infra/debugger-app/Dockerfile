FROM gcr.io/skia-public/debugger-app-base@sha256:047a12df8bbe98518c314c5014f56665e082e04f6ff5c27d0fa3ea3931c7b5ad

COPY canvaskit.js /usr/local/share/debugger-app/canvaskit.js
COPY canvaskit.wasm /usr/local/share/debugger-app/canvaskit.wasm
COPY version.js /usr/local/share/debugger-app/version.js
COPY index.d.ts /usr/local/share/debugger-app/index.d.ts

CMD ["/usr/local/bin/debugger", "--help"]