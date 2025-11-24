// Thin JS wrapper around the Emscripten build to work with Uint8Array buffers.
const createModule = require('./dist/storm.js');
const path = require('path');
const fs = require('fs');

const DEFAULT_ARCHIVE_FLAGS = 0x03000000 /* MPQ_CREATE_ARCHIVE_V4 */ |
                              0x00100000 /* MPQ_CREATE_LISTFILE */ |
                              0x00200000 /* MPQ_CREATE_ATTRIBUTES */;
const DEFAULT_FILE_FLAGS = 0x00000200 /* MPQ_FILE_COMPRESS */ |
                           0x80000000 /* MPQ_FILE_REPLACEEXISTING */;
const DEFAULT_COMPRESSION = 0x02; /* MPQ_COMPRESSION_ZLIB */

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
  Module.FS.mkdirTree('/work');

  const openArchive = Module.cwrap('storm_open_archive', 'number', ['string', 'number', 'number']);
  const closeArchive = Module.cwrap('storm_close_archive', 'number', ['number']);
  const hasFile = Module.cwrap('storm_has_file', 'number', ['number', 'string']);
  const openFile = Module.cwrap('storm_open_file', 'number', ['number', 'string']);
  const closeFile = Module.cwrap('storm_close_file', 'number', ['number']);
  const getFileSize = Module.cwrap('storm_get_file_size', 'bigint', ['number']);
  const readFileChunk = Module.cwrap('storm_read_file', 'number', ['number', 'number', 'number']);
  const createArchive = Module.cwrap('storm_create_archive', 'number', ['string', 'number', 'number']);
  const addFileFromMemory = Module.cwrap('storm_add_file_from_memory', 'number',
    ['number', 'string', 'number', 'number', 'number', 'number']);
  const lastError = Module.cwrap('storm_last_error', 'number', []);
  const findFirst = Module.cwrap('storm_find_first', 'number', ['number', 'string']);
  const findNext = Module.cwrap('storm_find_next', 'number', ['number']);
  const findName = Module.cwrap('storm_find_name', 'number', ['number']);
  const findSize = Module.cwrap('storm_find_size', 'number', ['number']);
  const findClose = Module.cwrap('storm_find_close', 'number', ['number']);

  const ensureHandle = (value, action) => {
    if (!value) {
      const err = lastError();
      throw new Error(`${action} failed (err=${err})`);
    }
    return value;
  };

  const readWholeFile = (fileHandle) => {
    const sizeBig = getFileSize(fileHandle);
    if (sizeBig < 0n) {
      const err = lastError();
      throw new Error(`get_file_size failed (err=${err})`);
    }

    if (sizeBig > BigInt(Number.MAX_SAFE_INTEGER)) {
      throw new Error('file too large for JS heap');
    }

    const size = Number(sizeBig);
    const ptr = Module._malloc(Math.max(size, 1));
    const bytesRead = readFileChunk(fileHandle, ptr, size);
    const out = bytesRead > 0 ? Module.HEAPU8.slice(ptr, ptr + bytesRead) : new Uint8Array(0);
    Module._free(ptr);

    if (bytesRead < 0) {
      const err = lastError();
      throw new Error(`read failed (err=${err})`);
    }

    return out;
  };

  return {
    fs: Module.FS,
    constants: {
      DEFAULT_ARCHIVE_FLAGS,
      DEFAULT_FILE_FLAGS,
      DEFAULT_COMPRESSION,
    },
    openArchive: (path, { priority = 0, flags = 0 } = {}) =>
      ensureHandle(openArchive(path, priority, flags), 'open archive'),
    openArchiveFromBuffer: (buffer, { path = '/work/archive.mpq', priority = 0, flags = 0 } = {}) => {
      Module.FS.writeFile(path, buffer);
      const handle = ensureHandle(openArchive(path, priority, flags), 'open archive');
      return { handle, path };
    },
    closeArchive: (handle) => !!closeArchive(handle),
    hasFile: (archiveHandle, archivedName) => !!hasFile(archiveHandle, archivedName),
    readFile: (archiveHandle, archivedName) => {
      const fileHandle = ensureHandle(openFile(archiveHandle, archivedName), 'open file');
      try {
        return readWholeFile(fileHandle);
      } finally {
        closeFile(fileHandle);
      }
    },
    createArchive: (path = '/work/out.mpq', { flags = DEFAULT_ARCHIVE_FLAGS, maxFiles = 16 } = {}) =>
      ensureHandle(createArchive(path, flags, maxFiles), 'create archive'),
    addFileFromBuffer: (
      archiveHandle,
      archivedName,
      buffer,
      { fileFlags = DEFAULT_FILE_FLAGS, compression = DEFAULT_COMPRESSION } = {}
    ) => {
      const ptr = Module._malloc(Math.max(buffer.length, 1));
      Module.HEAPU8.set(buffer, ptr);
      const ok = addFileFromMemory(archiveHandle, archivedName, ptr, buffer.length, fileFlags, compression);
      Module._free(ptr);
      if (!ok) {
        const err = lastError();
        throw new Error(`add file failed (err=${err})`);
      }
      return true;
    },
    lastError: () => lastError(),
    readArchiveFile: (path) => Module.FS.readFile(path),
    listFiles: (archiveHandle, mask = '*') => {
      const ctx = findFirst(archiveHandle, mask);
      if (!ctx) return [];
      const results = [];
      try {
        let keepGoing = true;
        while (keepGoing) {
          const ptr = findName(ctx);
          if (ptr) {
            const name = Module.UTF8ToString(ptr);
            results.push({ name, size: findSize(ctx) });
          }
          keepGoing = !!findNext(ctx);
        }
      } finally {
        findClose(ctx);
      }
      return results;
    },
  };
}

module.exports = createStormLib;
