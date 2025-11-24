// Shared WASM wrapper used by both Node and browser builds.
(function (root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = { makeCreateStormLib: factory };
  } else {
    root.makeCreateStormLib = factory;
  }
})(typeof self !== 'undefined' ? self : this, function makeCreateStormLib(loadModule) {
  return async function createStormLib({ workDir = '/work', ...moduleOpts } = {}) {
    const Module = await loadModule(moduleOpts);
    const baseWorkDir = workDir || '/work';
    try {
      Module.FS.mkdirTree(baseWorkDir);
    } catch (_err) {
      /* ignore if already exists */
    }

    const DEFAULT_ARCHIVE_FLAGS = 0x03000000 | 0x00100000 | 0x00200000;
    const DEFAULT_FILE_FLAGS = 0x00000200 | 0x80000000;
    const DEFAULT_COMPRESSION = 0x02;
    const MPQ_FORMAT_VERSION_3 = 2;
    const MPQ_FORMAT_VERSION_4 = 3;
    const DEFAULT_MPQ_VERSION = MPQ_FORMAT_VERSION_4;
    const MPQ_FILE_DEFAULT_INTERNAL = 0xFFFFFFFF;
    const MPQ_ATTRIBUTE_CRC32 = 0x00000001;
    const MPQ_ATTRIBUTE_FILETIME = 0x00000002;
    const MPQ_ATTRIBUTE_MD5 = 0x00000004;
    const MPQ_ATTRIBUTE_PATCH_BIT = 0x00000008;

    const openArchive = Module.cwrap('storm_open_archive', 'number', ['string', 'number', 'number']);
    const closeArchive = Module.cwrap('storm_close_archive', 'number', ['number']);
    const hasFile = Module.cwrap('storm_has_file', 'number', ['number', 'string']);
    const openFile = Module.cwrap('storm_open_file', 'number', ['number', 'string']);
    const closeFile = Module.cwrap('storm_close_file', 'number', ['number']);
    const getFileSize = Module.cwrap('storm_get_file_size', 'bigint', ['number']);
    const readFileChunk = Module.cwrap('storm_read_file', 'number', ['number', 'number', 'number']);
    const createArchive = Module.cwrap('storm_create_archive', 'number', ['string', 'number', 'number']);
    const createArchive2 = Module.cwrap('storm_create_archive2', 'number',
      ['string', 'number', 'number', 'number', 'number', 'number', 'number']);
    const addFileFromMemory = Module.cwrap('storm_add_file_from_memory', 'number', ['number', 'string', 'number', 'number', 'number', 'number']);
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

    const toUint8 = (buffer) => {
      if (buffer instanceof Uint8Array) return buffer;
      if (ArrayBuffer.isView(buffer)) {
        return new Uint8Array(buffer.buffer, buffer.byteOffset, buffer.byteLength);
      }
      return new Uint8Array(buffer);
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
      if (bytesRead < 0) {
        Module._free(ptr);
        const err = lastError();
        throw new Error(`read failed (err=${err})`);
      }
      const out = Module.HEAPU8.slice(ptr, ptr + bytesRead);
      Module._free(ptr);
      return out;
    };

    return {
      fs: Module.FS,
      constants: { DEFAULT_ARCHIVE_FLAGS, DEFAULT_FILE_FLAGS, DEFAULT_COMPRESSION },
      openArchive: (path, { priority = 0, flags = 0 } = {}) =>
        ensureHandle(openArchive(path, priority, flags), 'open archive'),
      openArchiveFromBuffer: (buffer, { path = `${baseWorkDir}/archive.mpq`, priority = 0, flags = 0 } = {}) => {
        const data = toUint8(buffer);
        Module.FS.writeFile(path, data);
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
      createArchive: (path = `${baseWorkDir}/out.mpq`, { flags = DEFAULT_ARCHIVE_FLAGS, maxFiles = 16 } = {}) =>
        ensureHandle(createArchive(path, flags, maxFiles), 'create archive'),
      createArchiveCustom: (path = `${baseWorkDir}/out.mpq`, params = {}) => {
        const {
          mpqVersion = DEFAULT_MPQ_VERSION,
          sectorSize,
          listfileFlags,
          attrFileFlags,
          attrFlags,
          maxFiles = 16,
        } = params;
        const resolvedSectorSize =
          (typeof sectorSize === 'number' && sectorSize > 0)
            ? sectorSize
            : (mpqVersion >= MPQ_FORMAT_VERSION_3 ? 0x4000 : 0x1000);
        const resolvedListfileFlags =
          listfileFlags !== undefined ? listfileFlags : MPQ_FILE_DEFAULT_INTERNAL;
        const resolvedAttrFileFlags =
          attrFileFlags !== undefined ? attrFileFlags : MPQ_FILE_DEFAULT_INTERNAL;
        const defaultAttrFlags = MPQ_ATTRIBUTE_CRC32 | MPQ_ATTRIBUTE_FILETIME | MPQ_ATTRIBUTE_MD5;
        const resolvedAttrFlags = attrFlags !== undefined
          ? attrFlags
          : (resolvedAttrFileFlags
              ? defaultAttrFlags | (mpqVersion >= MPQ_FORMAT_VERSION_3 ? MPQ_ATTRIBUTE_PATCH_BIT : 0)
              : 0);
        return ensureHandle(
          createArchive2(
            path,
            mpqVersion,
            resolvedSectorSize,
            resolvedListfileFlags,
            resolvedAttrFileFlags,
            resolvedAttrFlags,
            maxFiles
          ),
          'create archive (custom)'
        );
      },
      addFileFromBuffer: (
        archiveHandle,
        archivedName,
        buffer,
        { fileFlags = DEFAULT_FILE_FLAGS, compression = DEFAULT_COMPRESSION } = {}
      ) => {
        const data = toUint8(buffer);
        const ptr = Module._malloc(Math.max(data.length, 1));
        Module.HEAPU8.set(data, ptr);
        const ok = addFileFromMemory(archiveHandle, archivedName, ptr, data.length, fileFlags, compression);
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
  };
});
