// Thin JS wrapper around the Emscripten build to work with Uint8Array buffers.
const createModule = require('./dist/storm.js');
const createStormClient = require('./client');
const path = require('path');
const fs = require('fs');

async function createStormLib() {
  const Module = await createModule({
    locateFile: (p) => path.join(__dirname, 'dist', p),
    instantiateWasm: (info, receiveInstance) => {
      const wasmPath = path.join(__dirname, 'dist', 'storm.wasm');
      const bytes = fs.readFileSync(wasmPath);
      return WebAssembly.instantiate(bytes, info).then((result) => {
        receiveInstance(result.instance);
        return result.exports;
      });
    },
  });
  return createStormClient(Module, { workDir: '/work' });
}

module.exports = createStormLib;
