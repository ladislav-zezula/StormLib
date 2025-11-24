#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../src/StormLib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Minimal C wrappers
// todo: incomplete, export everything?

uintptr_t storm_open_archive(const char * path, uint32_t priority, uint32_t flags)
{
    HANDLE hArchive = NULL;

    if(!SFileOpenArchive(path, priority, flags, &hArchive))
        return 0;
    return (uintptr_t)hArchive;
}

bool storm_close_archive(uintptr_t hArchive)
{
    return SFileCloseArchive((HANDLE)hArchive);
}

bool storm_has_file(uintptr_t hArchive, const char * archived_name)
{
    return SFileHasFile((HANDLE)hArchive, archived_name);
}

uintptr_t storm_open_file(uintptr_t hArchive, const char * archived_name)
{
    HANDLE hFile = NULL;

    if(!SFileOpenFileEx((HANDLE)hArchive, archived_name, SFILE_OPEN_FROM_MPQ, &hFile))
        return 0;
    return (uintptr_t)hFile;
}

bool storm_close_file(uintptr_t hFile)
{
    return SFileCloseFile((HANDLE)hFile);
}

int64_t storm_get_file_size(uintptr_t hFile)
{
    DWORD sizeHigh = 0;
    DWORD sizeLow = SFileGetFileSize((HANDLE)hFile, &sizeHigh);

    if(sizeLow == SFILE_INVALID_SIZE)
        return -1;
    return ((int64_t)(((uint64_t)sizeHigh << 32) | sizeLow));
}

// Returns number of bytes read, or -1 on error.
int storm_read_file(uintptr_t hFile, void * buffer, uint32_t bytes_to_read)
{
    DWORD bytes_read = 0;

    if(!SFileReadFile((HANDLE)hFile, buffer, bytes_to_read, &bytes_read, NULL))
        return -1;
    return (int)bytes_read;
}

uintptr_t storm_create_archive(const char * path, uint32_t create_flags, uint32_t max_file_count)
{
    HANDLE hArchive = NULL;

    if(!SFileCreateArchive(path, create_flags, max_file_count, &hArchive))
        return 0;
    return (uintptr_t)hArchive;
}

// Adds a single file from memory. Returns true on success.
bool storm_add_file_from_memory(uintptr_t hArchive,
                                const char * archived_name,
                                const uint8_t * data,
                                uint32_t data_size,
                                uint32_t file_flags,
                                uint32_t compression)
{
    HANDLE hFile = NULL;

    if(!SFileCreateFile((HANDLE)hArchive, archived_name, 0, data_size, 0, file_flags, &hFile))
        return false;

    if(!SFileWriteFile(hFile, data, data_size, compression))
    {
        SFileCloseFile(hFile);
        return false;
    }

    if(!SFileFinishFile(hFile))
        return false;

    return true;
}

uint32_t storm_last_error(void)
{
    return SErrGetLastError();
}

// -------- File enumeration helpers --------

typedef struct _WFindCtx
{
    HANDLE hFind;
    SFILE_FIND_DATA data;
} WFindCtx;

uintptr_t storm_find_first(uintptr_t hArchive, const char * mask)
{
    WFindCtx * ctx = (WFindCtx *)malloc(sizeof(WFindCtx));
    if(ctx == NULL)
        return 0;

    ctx->hFind = SFileFindFirstFile((HANDLE)hArchive, (mask && mask[0]) ? mask : "*", &ctx->data, NULL);
    if(ctx->hFind == NULL)
    {
        free(ctx);
        return 0;
    }

    return (uintptr_t)ctx;
}

bool storm_find_next(uintptr_t ctxPtr)
{
    WFindCtx * ctx = (WFindCtx *)ctxPtr;
    if(ctx == NULL || ctx->hFind == NULL)
        return false;
    return SFileFindNextFile(ctx->hFind, &ctx->data);
}

const char * storm_find_name(uintptr_t ctxPtr)
{
    WFindCtx * ctx = (WFindCtx *)ctxPtr;
    if(ctx == NULL)
        return NULL;
    return ctx->data.cFileName;
}

uint32_t storm_find_size(uintptr_t ctxPtr)
{
    WFindCtx * ctx = (WFindCtx *)ctxPtr;
    if(ctx == NULL)
        return 0;
    return ctx->data.dwFileSize;
}

bool storm_find_close(uintptr_t ctxPtr)
{
    WFindCtx * ctx = (WFindCtx *)ctxPtr;
    bool ok = false;
    if(ctx != NULL)
    {
        if(ctx->hFind != NULL)
            ok = SFileFindClose(ctx->hFind);
        free(ctx);
    }
    return ok;
}

#ifdef __cplusplus
}
#endif
