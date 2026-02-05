const path = require('path');
const fs = require('fs');
const createModule = require('./dist/storm.js');
const { makeCreateStormLib } = require('./client.js');

const createStormLib = makeCreateStormLib((opts = {}) =>
  createModule({
    locateFile: (p) => path.join(__dirname, 'dist', p),
    instantiateWasm: (info, receiveInstance) => {
      const wasmPath = path.join(__dirname, 'dist', 'storm.wasm');
      const bytes = fs.readFileSync(wasmPath);
      return WebAssembly.instantiate(bytes, info).then((result) => {
        receiveInstance(result.instance);
        return result.exports;
      });
    },
    ...opts,
  })
);

module.exports = createStormLib;
