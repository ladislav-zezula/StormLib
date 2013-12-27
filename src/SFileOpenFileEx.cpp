/*****************************************************************************/
/* SFileOpenFileEx.cpp                    Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* Description :                                                             */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* xx.xx.99  1.00  Lad  The first version of SFileOpenFileEx.cpp             */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"

/*****************************************************************************/
/* Local functions                                                           */
/*****************************************************************************/

static const char * GetPatchFileName(TMPQArchive * ha, const char * szFileName, char * szBuffer)
{
    if(ha->cchPatchPrefix != 0)
    {
        // Copy the patch prefix
        memcpy(szBuffer, ha->szPatchPrefix, ha->cchPatchPrefix);
        
        // The patch name for "OldWorld\\XXX\\YYY" is "Base\\XXX\YYY"
        // We need to remove the "Oldworld\\" prefix
        if(!_strnicmp(szFileName, "OldWorld\\", 9))
            szFileName += 9;

        // Copy the rest of the name
        strcpy(szBuffer + ha->cchPatchPrefix, szFileName);
        szFileName = szBuffer;
    }

    return szFileName;
}

static bool OpenLocalFile(const char * szFileName, HANDLE * phFile)
{
    TFileStream * pStream;
    TMPQFile * hf = NULL;
    TCHAR szFileNameT[MAX_PATH];

    // Convert the file name to UNICODE (if needed)
    CopyFileName(szFileNameT, szFileName, strlen(szFileName));

    // Open the file and create the TMPQFile structure
    pStream = FileStream_OpenFile(szFileNameT, STREAM_FLAG_READ_ONLY);
    if(pStream != NULL)
    {
        // Allocate and initialize file handle
        hf = CreateMpqFile(NULL);
        if(hf != NULL)
        {
            hf->pStream = pStream;
            *phFile = hf;
            return true;
        }
        else
        {
            FileStream_Close(pStream);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        }
    }
    *phFile = NULL;
    return false;
}

bool OpenPatchedFile(HANDLE hMpq, const char * szFileName, DWORD dwReserved, HANDLE * phFile)
{
    TMPQArchive * haBase = NULL;
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    TFileEntry * pFileEntry;
    TMPQFile * hfPatch;                     // Pointer to patch file
    TMPQFile * hfBase = NULL;               // Pointer to base open file
    TMPQFile * hf = NULL;
    HANDLE hPatchFile;
    char szPrefixBuffer[MAX_PATH];

    // Keep this flag here for future updates
    dwReserved = dwReserved;

    // First of all, find the latest archive where the file is in base version
    // (i.e. where the original, unpatched version of the file exists)
    while(ha != NULL)
    {
        // If the file is there, then we remember the archive
        pFileEntry = GetFileEntryExact(ha, GetPatchFileName(ha, szFileName, szPrefixBuffer), 0);
        if(pFileEntry != NULL && (pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE) == 0)
            haBase = ha;

        // Move to the patch archive
        ha = ha->haPatch;
    }

    // If we couldn't find the base file in any of the patches, it doesn't exist
    if((ha = haBase) == NULL)
    {
        SetLastError(ERROR_FILE_NOT_FOUND);
        return false;
    }

    // Now open the base file
    if(SFileOpenFileEx((HANDLE)ha, GetPatchFileName(ha, szFileName, szPrefixBuffer), SFILE_OPEN_BASE_FILE, (HANDLE *)&hfBase))
    {
        // The file must be a base file, i.e. without MPQ_FILE_PATCH_FILE
        assert((hfBase->pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE) == 0);
        hf = hfBase;

        // Now open all patches and attach them on top of the base file
        for(ha = ha->haPatch; ha != NULL; ha = ha->haPatch)
        {
            // Prepare the file name with a correct prefix
            if(SFileOpenFileEx((HANDLE)ha, GetPatchFileName(ha, szFileName, szPrefixBuffer), SFILE_OPEN_BASE_FILE, &hPatchFile))
            {
                // Remember the new version
                hfPatch = (TMPQFile *)hPatchFile;

                // We should not find patch file
                assert((hfPatch->pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE) != 0);

                // Attach the patch to the base file
                hf->hfPatch = hfPatch;
                hf = hfPatch;
            }
        }
    }

    // Give the updated base MPQ
    if(phFile != NULL)
        *phFile = (HANDLE)hfBase;
    return true;
}

/*****************************************************************************/
/* Public functions                                                          */
/*****************************************************************************/

//-----------------------------------------------------------------------------
// SFileEnumLocales enums all locale versions within MPQ. 
// Functions fills all available language identifiers on a file into the buffer
// pointed by plcLocales. There must be enough entries to copy the localed,
// otherwise the function returns ERROR_INSUFFICIENT_BUFFER.

int WINAPI SFileEnumLocales(
    HANDLE hMpq,
    const char * szFileName,
    LCID * plcLocales,
    LPDWORD pdwMaxLocales,
    DWORD dwSearchScope)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    TFileEntry * pFileEntry;
    TMPQHash * pFirstHash;
    TMPQHash * pHash;
    DWORD dwFileIndex = 0;
    DWORD dwLocales = 0;

    // Test the parameters
    if(!IsValidMpqHandle(hMpq))
        return ERROR_INVALID_HANDLE;
    if(szFileName == NULL || *szFileName == 0)
        return ERROR_INVALID_PARAMETER;
    if(pdwMaxLocales == NULL)
        return ERROR_INVALID_PARAMETER;
    
    // Keep compiler happy
    dwSearchScope = dwSearchScope;

    // Parse hash table entries for all locales
    if(!IsPseudoFileName(szFileName, &dwFileIndex))
    {
        // Calculate the number of locales
        pFirstHash = pHash = GetFirstHashEntry(ha, szFileName);
        while(pHash != NULL)
        {
            dwLocales++;
            pHash = GetNextHashEntry(ha, pFirstHash, pHash);
        }

        // Test if there is enough space to copy the locales
        if(*pdwMaxLocales < dwLocales)
        {
            *pdwMaxLocales = dwLocales;
            return ERROR_INSUFFICIENT_BUFFER;
        }

        // Enum the locales
        pFirstHash = pHash = GetFirstHashEntry(ha, szFileName);
        while(pHash != NULL)
        {
            *plcLocales++ = pHash->lcLocale;
            pHash = GetNextHashEntry(ha, pFirstHash, pHash);
        }
    }
    else
    {
        // There must be space for 1 locale
        if(*pdwMaxLocales < 1)
        {
            *pdwMaxLocales = 1;
            return ERROR_INSUFFICIENT_BUFFER;
        }

        // For nameless access, always return 1 locale
        pFileEntry = GetFileEntryByIndex(ha, dwFileIndex);
        pHash = ha->pHashTable + pFileEntry->dwHashIndex;
        *plcLocales = pHash->lcLocale;
        dwLocales = 1;
    }

    // Give the caller the total number of found locales
    *pdwMaxLocales = dwLocales;
    return ERROR_SUCCESS;
}

//-----------------------------------------------------------------------------
// SFileHasFile
//
//   hMpq          - Handle of opened MPQ archive
//   szFileName    - Name of file to look for

bool WINAPI SFileHasFile(HANDLE hMpq, const char * szFileName)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    TFileEntry * pFileEntry;
    DWORD dwFlagsToCheck = MPQ_FILE_EXISTS;
    DWORD dwFileIndex = 0;
    char szPrefixBuffer[MAX_PATH];
    bool bIsPseudoName;
    int nError = ERROR_SUCCESS;

    if(!IsValidMpqHandle(hMpq))
        nError = ERROR_INVALID_HANDLE;
    if(szFileName == NULL || *szFileName == 0)
        nError = ERROR_INVALID_PARAMETER;

    // Prepare the file opening
    if(nError == ERROR_SUCCESS)
    {
        // Different processing for pseudo-names
        bIsPseudoName = IsPseudoFileName(szFileName, &dwFileIndex);

        // Walk through the MPQ and all patches
        while(ha != NULL)
        {
            // Verify presence of the file
            pFileEntry = (bIsPseudoName == false) ? GetFileEntryLocale(ha, GetPatchFileName(ha, szFileName, szPrefixBuffer), lcFileLocale)
                                                  : GetFileEntryByIndex(ha, dwFileIndex);
            // Verify the file flags
            if(pFileEntry != NULL && (pFileEntry->dwFlags & dwFlagsToCheck) == MPQ_FILE_EXISTS)
                return true;

            // If this is patched archive, go to the patch
            dwFlagsToCheck = MPQ_FILE_EXISTS | MPQ_FILE_PATCH_FILE;
            ha = ha->haPatch;
        }

        // Not found, sorry
        nError = ERROR_FILE_NOT_FOUND;
    }

    // Cleanup
    SetLastError(nError);
    return false;
}


//-----------------------------------------------------------------------------
// SFileOpenFileEx
//
//   hMpq          - Handle of opened MPQ archive
//   szFileName    - Name of file to open
//   dwSearchScope - Where to search
//   phFile        - Pointer to store opened file handle

bool WINAPI SFileOpenFileEx(HANDLE hMpq, const char * szFileName, DWORD dwSearchScope, HANDLE * phFile)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    TFileEntry  * pFileEntry = NULL;
    TMPQFile    * hf = NULL;
    DWORD dwFileIndex = 0;
    bool bOpenByIndex = false;
    int nError = ERROR_SUCCESS;

    // Don't accept NULL pointer to file handle
    if(phFile == NULL)
        nError = ERROR_INVALID_PARAMETER;

    // Prepare the file opening
    if(nError == ERROR_SUCCESS)
    {
        switch(dwSearchScope)
        {
            case SFILE_OPEN_FROM_MPQ:
            case SFILE_OPEN_BASE_FILE:
                
                if(!IsValidMpqHandle(hMpq))
                {
                    nError = ERROR_INVALID_HANDLE;
                    break;
                }

                if(szFileName == NULL || *szFileName == 0)
                {
                    nError = ERROR_INVALID_PARAMETER;
                    break;
                }

                // Check the pseudo-file name
                if(IsPseudoFileName(szFileName, &dwFileIndex))
                {
                    pFileEntry = GetFileEntryByIndex(ha, dwFileIndex);
                    bOpenByIndex = true;
                    if(pFileEntry == NULL)
                        nError = ERROR_FILE_NOT_FOUND;
                }
                else
                {
                    // If this MPQ is a patched archive, open the file as patched
                    if(ha->haPatch == NULL || dwSearchScope == SFILE_OPEN_BASE_FILE)
                    {
                        // Otherwise, open the file from *this* MPQ
                        pFileEntry = GetFileEntryLocale(ha, szFileName, lcFileLocale);
                        if(pFileEntry == NULL)
                            nError = ERROR_FILE_NOT_FOUND;
                    }
                    else
                    {
                        return OpenPatchedFile(hMpq, szFileName, 0, phFile);
                    }
                }
                break;

            case SFILE_OPEN_ANY_LOCALE:

                // This open option is reserved for opening MPQ internal listfile.
                // No argument validation. Tries to open file with neutral locale first,
                // then any other available.
                pFileEntry = GetFileEntryAny(ha, szFileName);
                if(pFileEntry == NULL)
                    nError = ERROR_FILE_NOT_FOUND;
                break;

            case SFILE_OPEN_LOCAL_FILE:

                if(szFileName == NULL || *szFileName == 0)
                {
                    nError = ERROR_INVALID_PARAMETER;
                    break;
                }

                return OpenLocalFile(szFileName, phFile); 

            default:

                // Don't accept any other value
                nError = ERROR_INVALID_PARAMETER;
                break;
        }

        // Quick return if something failed
        if(nError != ERROR_SUCCESS)
        {
            SetLastError(nError);
            *phFile = NULL;
            return false;
        }
    }

    // Test if the file was not already deleted.
    if(nError == ERROR_SUCCESS)
    {
        if((pFileEntry->dwFlags & MPQ_FILE_EXISTS) == 0)
            nError = ERROR_FILE_NOT_FOUND;
        if(pFileEntry->dwFlags & ~MPQ_FILE_VALID_FLAGS)
            nError = ERROR_NOT_SUPPORTED;
    }

    // Allocate file handle
    if(nError == ERROR_SUCCESS)
    {
        if((hf = STORM_ALLOC(TMPQFile, 1)) == NULL)
            nError = ERROR_NOT_ENOUGH_MEMORY;
    }

    // Initialize file handle
    if(nError == ERROR_SUCCESS)
    {
        memset(hf, 0, sizeof(TMPQFile));
        hf->pFileEntry = pFileEntry;
        hf->dwMagic = ID_MPQ_FILE;
        hf->ha = ha;

        hf->MpqFilePos   = pFileEntry->ByteOffset;
        hf->RawFilePos   = ha->MpqPos + hf->MpqFilePos;
        hf->dwDataSize   = pFileEntry->dwFileSize;

        // If the MPQ has sector CRC enabled, enable if for the file
        if(ha->dwFlags & MPQ_FLAG_CHECK_SECTOR_CRC)
            hf->bCheckSectorCRCs = true;

        // If we know the real file name, copy it to the file entry
        if(bOpenByIndex == false)
        {
            // If there is no file name yet, allocate it
            AllocateFileName(ha, pFileEntry, szFileName);

            // If the file is encrypted, we should detect the file key
            if(pFileEntry->dwFlags & MPQ_FILE_ENCRYPTED)
            {
                hf->dwFileKey = DecryptFileKey(szFileName,
                                               pFileEntry->ByteOffset,
                                               pFileEntry->dwFileSize,
                                               pFileEntry->dwFlags);
            }
        }
        else
        {
            // Try to auto-detect the file name
            if(!SFileGetFileName(hf, NULL))
                nError = GetLastError();
        }
    }

    // If the file is actually a patch file, we have to load the patch file header
    if(nError == ERROR_SUCCESS && pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE)
    {
        assert(hf->pPatchInfo == NULL);
        nError = AllocatePatchInfo(hf, true);
    }

    // Cleanup and exit
    if(nError != ERROR_SUCCESS)
    {
        SetLastError(nError);
        FreeMPQFile(hf);
        return false;
    }

    *phFile = hf;
    return true;
}

//-----------------------------------------------------------------------------
// bool WINAPI SFileCloseFile(HANDLE hFile);

bool WINAPI SFileCloseFile(HANDLE hFile)
{
    TMPQFile * hf = (TMPQFile *)hFile;
    
    if(!IsValidFileHandle(hFile))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return false;
    }

    // Free the structure
    FreeMPQFile(hf);
    return true;
}
