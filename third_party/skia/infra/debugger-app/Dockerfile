FROM gcr.io/skia-public/debugger-app-base@sha256:bb04143c038b593ed5955a14ae790ca56fd5cc6489913ceb5e1ec8eb4e9eaa9b

COPY canvaskit.js /usr/local/share/debugger-app/canvaskit.js
COPY canvaskit.wasm /usr/local/share/debugger-app/canvaskit.wasm
COPY version.js /usr/local/share/debugger-app/version.js
COPY index.d.ts /usr/local/share/debugger-app/index.d.ts

CMD ["/usr/local/bin/debugger", "--help"]