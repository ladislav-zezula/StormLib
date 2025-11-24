#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/wasm/build"
DIST_DIR="$ROOT/wasm/dist"

mkdir -p "$BUILD_DIR" "$DIST_DIR"

emcmake cmake -S "$ROOT" -B "$BUILD_DIR" \
  -D BUILD_SHARED_LIBS=OFF \
  -D STORM_USE_BUNDLED_LIBRARIES=ON \
  -D STORM_SKIP_INSTALL=ON \
  -D CMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" -j

emcc "$ROOT/wasm/shim.c" "$BUILD_DIR/libstorm.a" -I"$ROOT/src" \
  -sWASM_BIGINT=1 \
  -sMODULARIZE=1 \
  -sEXPORT_NAME=createModule \
  -sENVIRONMENT=web,worker,node \
  -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_RUNTIME_METHODS=['cwrap','ccall','FS','UTF8ToString','stringToUTF8','lengthBytesUTF8','getValue','setValue','HEAPU8'] \
  -sEXPORTED_FUNCTIONS=['_malloc','_free','_storm_open_archive','_storm_close_archive','_storm_has_file','_storm_open_file','_storm_close_file','_storm_get_file_size','_storm_read_file','_storm_create_archive','_storm_add_file_from_memory','_storm_last_error','_storm_find_first','_storm_find_next','_storm_find_name','_storm_find_size','_storm_find_close'] \
  -O3 \
  -o "$DIST_DIR/storm.js"
