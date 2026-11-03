FROM gcr.io/skia-public/debugger-app-base@sha256:396232912f82ef5327eff72ec20787b2c8223d4595f36624557d68aa3a286c0b

COPY canvaskit.js /usr/local/share/debugger-app/canvaskit.js
COPY canvaskit.wasm /usr/local/share/debugger-app/canvaskit.wasm
COPY version.js /usr/local/share/debugger-app/version.js
COPY index.d.ts /usr/local/share/debugger-app/index.d.ts

CMD ["/usr/local/bin/debugger", "--help"]