FROM gcr.io/skia-public/jsfiddle-base@sha256:a48b4b4877fa5bd299ef4755e2f5123b89ee907c1e1f9177390e7c22fe1ea9ce

COPY canvaskit.js /usr/local/share/jsfiddle/dist/canvaskit.js
COPY canvaskit.wasm /usr/local/share/jsfiddle/dist/canvaskit.wasm
COPY version.js /usr/local/share/jsfiddle/dist/version.js
COPY index.d.ts /usr/local/share/jsfiddle/dist/index.d.ts

CMD ["/usr/local/bin/jsfiddle", "--help"]