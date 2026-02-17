## WASM build

This folder holds all WASM-specific glue to keep changes away from the main sources.

### Build
```
bash ./wasm/build.sh
```
Outputs `wasm/dist/storm.js` (ESM-friendly, `MODULARIZE=1`, Node target). Requires Emscripten (`emcc`/`emcmake`) on PATH.

### Browser demo
- Open `wasm/demo.html` via one of two options:
  1) Serve over HTTP (recommended): `python -m http.server 8000` from repo root, then visit `http://localhost:8000/wasm/demo.html`.
  2) Local file with CORS disabled: `chromium --disable-web-security --user-data-dir=/tmp/chrome-storm wasm/demo.html`.
- Use the UI


### JS usage (Node)
```js
const createStormLib = require('./wasm/index.js');

(async () => {
  const storm = await createStormLib();

  // Open an existing MPQ from a Uint8Array
  const archive = storm.openArchiveFromBuffer(myUint8Array, { path: '/work/input.mpq' });
  const data = storm.readFile(archive.handle, 'path/inside.mpq');

  // Create a new archive and add a file from a buffer
  const outHandle = storm.createArchive('/work/out.mpq');
  storm.addFileFromBuffer(outHandle, 'foo.txt', Buffer.from('hello'), {});
  storm.closeArchive(outHandle);
  const outBytes = storm.readArchiveFile('/work/out.mpq'); // Uint8Array
})();
```
(Or check out demo.html)