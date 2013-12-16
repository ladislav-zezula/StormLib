/*****************************************************************************/
/* SListFile.cpp                          Copyright (c) Ladislav Zezula 2004 */
/*---------------------------------------------------------------------------*/
/* Description:                                                              */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 12.06.04  1.00  Lad  The first version of SListFile.cpp                   */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"
#include <assert.h>

//-----------------------------------------------------------------------------
// Listfile entry structure

#define CACHE_BUFFER_SIZE  0x1000       // Size of the cache buffer

struct TListFileCache
{
    HANDLE  hFile;                      // Stormlib file handle
    char  * szMask;                     // Self-relative pointer to file mask
    DWORD   dwFileSize;                 // Total size of the cached file
    DWORD   dwFilePos;                  // Position of the cache in the file
    BYTE  * pBegin;                     // The begin of the listfile cache
    BYTE  * pPos;
    BYTE  * pEnd;                       // The last character in the file cache

    BYTE Buffer[CACHE_BUFFER_SIZE];
//  char MaskBuff[1]                    // Followed by the name mask (if any)
};

//-----------------------------------------------------------------------------
// Local functions (cache)

static char * CopyListLine(char * szListLine, const char * szFileName)
{
    // Copy the string
    while(szFileName[0] != 0)
        *szListLine++ = *szFileName++;

    // Append the end-of-line
    *szListLine++ = 0x0D;
    *szListLine++ = 0x0A;
    return szListLine;
}

static bool FreeListFileCache(TListFileCache * pCache)
{
    // Valid parameter check
    if(pCache != NULL)
        STORM_FREE(pCache);
    return true;
}

static TListFileCache * CreateListFileCache(HANDLE hListFile, const char * szMask)
{
    TListFileCache * pCache = NULL;
    size_t nMaskLength = 0;
    DWORD dwBytesRead = 0;
    DWORD dwFileSize;

    // Get the amount of bytes that need to be allocated
    dwFileSize = SFileGetFileSize(hListFile, NULL);
    if(dwFileSize == 0)
        return NULL;

    // Append buffer for name mask, if any
    if(szMask != NULL)
        nMaskLength = strlen(szMask) + 1;

    // Allocate cache for one file block
    pCache = (TListFileCache *)STORM_ALLOC(BYTE, sizeof(TListFileCache) + nMaskLength);
    if(pCache != NULL)
    {
        // Clear the entire structure
        memset(pCache, 0, sizeof(TListFileCache) + nMaskLength);

        // Shall we copy the mask?
        if(szMask != NULL)
        {
            pCache->szMask = (char *)(pCache + 1);
            memcpy(pCache->szMask, szMask, nMaskLength);
        }

        // Load the file cache from the file
        SFileReadFile(hListFile, pCache->Buffer, CACHE_BUFFER_SIZE, &dwBytesRead, NULL);
        if(dwBytesRead != 0)
        {
            // Allocate pointers
            pCache->pBegin = pCache->pPos = &pCache->Buffer[0];
            pCache->pEnd   = pCache->pBegin + dwBytesRead;
            pCache->dwFileSize = dwFileSize;
            pCache->hFile  = hListFile;
        }
        else
        {
            FreeListFileCache(pCache);
            pCache = NULL;
        }
    }

    // Return the cache
    return pCache;
}

// Reloads the cache. Returns number of characters
// that has been loaded into the cache.
static DWORD ReloadListFileCache(TListFileCache * pCache)
{
    DWORD dwBytesToRead;
    DWORD dwBytesRead = 0;

    // Only do something if the cache is empty
    if(pCache->pPos >= pCache->pEnd)
    {
        // Move the file position forward
        pCache->dwFilePos += CACHE_BUFFER_SIZE;
        if(pCache->dwFilePos >= pCache->dwFileSize)
            return 0;

        // Get the number of bytes remaining
        dwBytesToRead = pCache->dwFileSize - pCache->dwFilePos;
        if(dwBytesToRead > CACHE_BUFFER_SIZE)
            dwBytesToRead = CACHE_BUFFER_SIZE;

        // Load the next data chunk to the cache
        SFileSetFilePointer(pCache->hFile, pCache->dwFilePos, NULL, FILE_BEGIN);
        SFileReadFile(pCache->hFile, pCache->Buffer, dwBytesToRead, &dwBytesRead, NULL);

        // If we didn't read anything, it might mean that the block
        // of the file is not available (in case of partial MPQs).
        // We stop reading the file at this point, because the rest
        // of the listfile is unreliable
        if(dwBytesRead == 0)
            return 0;

        // Set the buffer pointers
        pCache->pBegin =
        pCache->pPos = &pCache->Buffer[0];
        pCache->pEnd = pCache->pBegin + dwBytesRead;
    }

    return dwBytesRead;
}

static size_t ReadListFileLine(TListFileCache * pCache, char * szLine, int nMaxChars)
{
    char * szLineBegin = szLine;
    char * szLineEnd = szLine + nMaxChars - 1;
    char * szExtraString = NULL;
    
    // Skip newlines, spaces, tabs and another non-printable stuff
    for(;;)
    {
        // If we need to reload the cache, do it
        if(pCache->pPos == pCache->pEnd)
        {
            if(ReloadListFileCache(pCache) == 0)
                break;
        }

        // If we found a non-whitespace character, stop
        if(*pCache->pPos > 0x20)
            break;

        // Skip the character
        pCache->pPos++;
    }

    // Copy the remaining characters
    while(szLine < szLineEnd)
    {
        // If we need to reload the cache, do it now and resume copying
        if(pCache->pPos == pCache->pEnd)
        {
            if(ReloadListFileCache(pCache) == 0)
                break;
        }

        // If we have found a newline, stop loading
        if(*pCache->pPos == 0x0D || *pCache->pPos == 0x0A)
            break;

        // Blizzard listfiles can also contain information about patch:
        // Pass1\Files\MacOS\unconditional\user\Background Downloader.app\Contents\Info.plist~Patch(Data#frFR#base-frFR,1326)
        if(*pCache->pPos == '~')
            szExtraString = szLine;

        // Copy the character
        *szLine++ = *pCache->pPos++;
    }

    // Terminate line with zero
    *szLine = 0;

    // If there was extra string after the file name, clear it
    if(szExtraString != NULL)
    {
        if(szExtraString[0] == '~' && szExtraString[1] == 'P')
        {
            szLine = szExtraString;
            *szExtraString = 0;
        }
    }

    // Return the length of the line
    return (szLine - szLineBegin);
}

static int CompareFileNodes(const void * p1, const void * p2) 
{
    char * szFileName1 = *(char **)p1;
    char * szFileName2 = *(char **)p2;

    return _stricmp(szFileName1, szFileName2);
}

static LPBYTE CreateListFile(TMPQArchive * ha, DWORD * pcbListFile)
{
    TFileEntry * pFileTableEnd = ha->pFileTable + ha->dwFileTableSize;
    TFileEntry * pFileEntry;
    char ** SortTable = NULL;
    char * szListFile = NULL;
    char * szListLine;
    size_t nFileNodes = 0;
    size_t cbListFile = 0;
    size_t nIndex0;
    size_t nIndex1;

    // Allocate the table for sorting listfile
    SortTable = STORM_ALLOC(char*, ha->dwFileTableSize);
    if(SortTable == NULL)
        return NULL;

    // Construct the sort table
    // Note: in MPQs with multiple locale versions of the same file,
    // this code causes adding multiple listfile entries.
    // They will get removed after the listfile sorting
    for(pFileEntry = ha->pFileTable; pFileEntry < pFileTableEnd; pFileEntry++)
    {
        // Only take existing items
        if((pFileEntry->dwFlags & MPQ_FILE_EXISTS) && pFileEntry->szFileName != NULL)
        {
            // Ignore pseudo-names and internal names
            if(!IsPseudoFileName(pFileEntry->szFileName, NULL) && !IsInternalMpqFileName(pFileEntry->szFileName))
            {
                SortTable[nFileNodes++] = pFileEntry->szFileName;
            }
        }
    }

    // Remove duplicities
    if(nFileNodes > 0)
    {
        // Sort the table
        qsort(SortTable, nFileNodes, sizeof(char *), CompareFileNodes);

        // Count the 0-th item
        cbListFile += strlen(SortTable[0]) + 2;

        // Walk through the items and only use the ones that are not duplicated
        for(nIndex0 = 0, nIndex1 = 1; nIndex1 < nFileNodes; nIndex1++)
        {
            // If the next file node is different, we will include it to the result listfile
            if(_stricmp(SortTable[nIndex1], SortTable[nIndex0]) != 0)
            {
                cbListFile += strlen(SortTable[nIndex1]) + 2;
                nIndex0 = nIndex1;
            }
        }

        // Now allocate buffer for the entire listfile
        szListFile = szListLine = STORM_ALLOC(char, cbListFile + 1);
        if(szListFile != NULL)
        {
            // Copy the 0-th item
            szListLine = CopyListLine(szListLine, SortTable[0]);

            // Walk through the items and only use the ones that are not duplicated
            for(nIndex0 = 0, nIndex1 = 1; nIndex1 < nFileNodes; nIndex1++)
            {
                // If the next file node is different, we will include it to the result listfile
                if(_stricmp(SortTable[nIndex1], SortTable[nIndex0]) != 0)
                {
                    // Copy the listfile line
                    szListLine = CopyListLine(szListLine, SortTable[nIndex1]);
                    nIndex0 = nIndex1;
                }
            }

            // Sanity check - does the size match?
            assert((size_t)(szListLine - szListFile) == cbListFile);
        }
    }

    // Free the sort table
    STORM_FREE(SortTable);

    // Give away the listfile
    if(pcbListFile != NULL)
        *pcbListFile = (DWORD)cbListFile;
    return (LPBYTE)szListFile;
}

//-----------------------------------------------------------------------------
// Local functions (listfile nodes)

// Adds a name into the list of all names. For each locale in the MPQ,
// one entry will be created
// If the file name is already there, does nothing.
static int SListFileCreateNodeForAllLocales(TMPQArchive * ha, const char * szFileName)
{
    TMPQHeader * pHeader = ha->pHeader;
    TFileEntry * pFileEntry;
    TMPQHash * pFirstHash;
    TMPQHash * pHash;

    // If we have HET table, use that one
    if(ha->pHetTable != NULL)
    {
        pFileEntry = GetFileEntryAny(ha, szFileName);
        if(pFileEntry != NULL)
        {
            // Allocate file name for the file entry
            AllocateFileName(ha, pFileEntry, szFileName);
        }

        return ERROR_SUCCESS;
    }

    // If we have hash table, we use it
    if(ha->pHashTable != NULL)
    {
        // Look for the first hash table entry for the file
        pFirstHash = pHash = GetFirstHashEntry(ha, szFileName);

        // Go while we found something
        while(pHash != NULL)
        {
            // Is it a valid file table index ?
            if(pHash->dwBlockIndex < pHeader->dwBlockTableSize)
            {
                // Allocate file name for the file entry
                AllocateFileName(ha, ha->pFileTable + pHash->dwBlockIndex, szFileName);
            }

            // Now find the next language version of the file
            pHash = GetNextHashEntry(ha, pFirstHash, pHash);
        }
    }

    return ERROR_CAN_NOT_COMPLETE;
}

// Saves the whole listfile to the MPQ
int SListFileSaveToMpq(TMPQArchive * ha)
{
    TMPQFile * hf = NULL;
    LPBYTE pbListFile;
    DWORD cbListFile = 0;
    int nError = ERROR_SUCCESS;

    // Only save (listfile) if we should do so
    if(ha->dwFileFlags1 == 0 || ha->dwMaxFileCount == 0)
        return ERROR_SUCCESS;

    // At this point, we expect to have at least one reserved entry in the file table
    assert(ha->dwReservedFiles >= 1);
    ha->dwReservedFiles--;

    // Create the raw data that is to be written to (listfile)
    // Note: Creating the raw data before the (listfile) has been created in the MPQ
    // causes that the name of the listfile will not be included in the listfile itself.
    // That is OK, because (listfile) in Blizzard MPQs does not contain it either.
    pbListFile = CreateListFile(ha, &cbListFile);
    if(pbListFile != NULL)
    {
        // We expect it to be nonzero size
        assert(cbListFile != 0);

        // Determine the real flags for (listfile)
        if(ha->dwFileFlags1 == MPQ_FILE_EXISTS)
            ha->dwFileFlags1 = GetDefaultSpecialFileFlags(cbListFile, ha->pHeader->wFormatVersion);

        // Create the listfile in the MPQ
        nError = SFileAddFile_Init(ha, LISTFILE_NAME,
                                       0,
                                       cbListFile,
                                       LANG_NEUTRAL,
                                       ha->dwFileFlags1 | MPQ_FILE_REPLACEEXISTING,
                                      &hf);

        // Write the listfile raw data to it
        if(nError == ERROR_SUCCESS)
        {
            // Write the content of the listfile to the MPQ
            nError = SFileAddFile_Write(hf, pbListFile, cbListFile, MPQ_COMPRESSION_ZLIB);
            SFileAddFile_Finish(hf);

            // Clear the invalidate flag
            ha->dwFlags &= ~MPQ_FLAG_LISTFILE_INVALID;
        }

        // Free the listfile buffer
        STORM_FREE(pbListFile);
    }

    return nError;
}

static int SFileAddArbitraryListFile(
    TMPQArchive * ha,
    HANDLE hListFile)
{
    TListFileCache * pCache = NULL;
    size_t nLength;
    char szFileName[MAX_PATH];

    // Create the listfile cache for that file
    pCache = CreateListFileCache(hListFile, NULL);
    if(pCache != NULL)
    {
        // Load the node list. Add the node for every locale in the archive
        while((nLength = ReadListFileLine(pCache, szFileName, sizeof(szFileName))) > 0)
            SListFileCreateNodeForAllLocales(ha, szFileName);

        // Delete the cache
        FreeListFileCache(pCache);
    }
    
    return (pCache != NULL) ? ERROR_SUCCESS : ERROR_FILE_CORRUPT;
}

static int SFileAddExternalListFile(
    TMPQArchive * ha,
    HANDLE hMpq,
    const char * szListFile)
{
    HANDLE hListFile;
    int nError = ERROR_SUCCESS;

    // Open the external list file
    if(SFileOpenFileEx(hMpq, szListFile, SFILE_OPEN_LOCAL_FILE, &hListFile))
    {
        // Add the data from the listfile to MPQ
        nError = SFileAddArbitraryListFile(ha, hListFile);
        SFileCloseFile(hListFile);
    }
    return nError;
}

static int SFileAddInternalListFile(
    TMPQArchive * ha,
    HANDLE hMpq)
{
    TMPQArchive * haMpq = (TMPQArchive *)hMpq;
    TMPQHash * pFirstHash;
    TMPQHash * pHash;
    HANDLE hListFile;
    LCID lcSaveLocale = lcFileLocale;
    int nError = ERROR_SUCCESS;

    // If there is hash table, we need to support multiple listfiles
    // with different locales (BrooDat.mpq)
    if(haMpq->pHashTable != NULL)
    {
        pFirstHash = pHash = GetFirstHashEntry(haMpq, LISTFILE_NAME);
        while(nError == ERROR_SUCCESS && pHash != NULL)
        {
            // Set the prefered locale to that from list file
            SFileSetLocale(pHash->lcLocale);
            if(SFileOpenFileEx(hMpq, LISTFILE_NAME, 0, &hListFile))
            {
                // Add the data from the listfile to MPQ
                nError = SFileAddArbitraryListFile(ha, hListFile);
                SFileCloseFile(hListFile);
            }
            
            // Restore the original locale
            SFileSetLocale(lcSaveLocale);

            // Move to the next hash
            pHash = GetNextHashEntry(haMpq, pFirstHash, pHash);
        }
    }
    else
    {
        // Open the external list file
        if(SFileOpenFileEx(hMpq, LISTFILE_NAME, 0, &hListFile))
        {
            // Add the data from the listfile to MPQ
            // The function also closes the listfile handle
            nError = SFileAddArbitraryListFile(ha, hListFile);
            SFileCloseFile(hListFile);
        }
    }

    // Return the result of the operation
    return nError;
}

//-----------------------------------------------------------------------------
// File functions

// Adds a listfile into the MPQ archive.
int WINAPI SFileAddListFile(HANDLE hMpq, const char * szListFile)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    int nError = ERROR_SUCCESS;

    // Add the listfile for each MPQ in the patch chain
    while(ha != NULL)
    {
        if(szListFile != NULL)
            SFileAddExternalListFile(ha, hMpq, szListFile);
        else
            SFileAddInternalListFile(ha, hMpq);

        // Also, add three special files to the listfile:
        // (listfile) itself, (attributes) and (signature)
        SListFileCreateNodeForAllLocales(ha, LISTFILE_NAME);
        SListFileCreateNodeForAllLocales(ha, SIGNATURE_NAME);
        SListFileCreateNodeForAllLocales(ha, ATTRIBUTES_NAME);

        // Move to the next archive in the chain
        ha = ha->haPatch;
    }

    return nError;
}

//-----------------------------------------------------------------------------
// Enumerating files in listfile

HANDLE WINAPI SListFileFindFirstFile(HANDLE hMpq, const char * szListFile, const char * szMask, SFILE_FIND_DATA * lpFindFileData)
{
    TListFileCache * pCache = NULL;
    HANDLE hListFile = NULL;
    size_t nLength = 0;
    DWORD dwSearchScope = SFILE_OPEN_LOCAL_FILE;
    int nError = ERROR_SUCCESS;

    // Initialize the structure with zeros
    memset(lpFindFileData, 0, sizeof(SFILE_FIND_DATA));

    // If the szListFile is NULL, it means we have to open internal listfile
    if(szListFile == NULL)
    {
        // Use SFILE_OPEN_ANY_LOCALE for listfile. This will allow us to load
        // the listfile even if there is only non-neutral version of the listfile in the MPQ
        dwSearchScope = SFILE_OPEN_ANY_LOCALE;
        szListFile = LISTFILE_NAME;
    }

    // Open the local/internal listfile
    if(!SFileOpenFileEx(hMpq, szListFile, dwSearchScope, &hListFile))
        nError = GetLastError();

    // Load the listfile to cache
    if(nError == ERROR_SUCCESS)
    {
        pCache = CreateListFileCache(hListFile, szMask);
        if(pCache == NULL)
            nError = ERROR_FILE_CORRUPT;
    }

    // Perform file search
    if(nError == ERROR_SUCCESS)
    {
        // The listfile handle is in the cache now
        hListFile = NULL;

        // Iterate through the listfile
        for(;;)
        {
            // Read the (next) line
            nLength = ReadListFileLine(pCache, lpFindFileData->cFileName, sizeof(lpFindFileData->cFileName));
            if(nLength == 0)
            {
                nError = ERROR_NO_MORE_FILES;
                break;
            }

            // If some mask entered, check it
            if(CheckWildCard(lpFindFileData->cFileName, pCache->szMask))
                break;                
        }
    }

    // Cleanup & exit
    if(nError != ERROR_SUCCESS)
    {
        if(pCache != NULL)
            FreeListFileCache(pCache);
        pCache = NULL;

        memset(lpFindFileData, 0, sizeof(SFILE_FIND_DATA));
        SetLastError(nError);
    }

    // Close remaining unowned listfile handle
    if(hListFile != NULL)
        SFileCloseFile(hListFile);
    return (HANDLE)pCache;
}

bool WINAPI SListFileFindNextFile(HANDLE hFind, SFILE_FIND_DATA * lpFindFileData)
{
    TListFileCache * pCache = (TListFileCache *)hFind;
    size_t nLength;
    int nError = ERROR_INVALID_PARAMETER;

    // Check for parameters
    if(pCache != NULL)
    {
        for(;;)
        {
            // Read the (next) line
            nLength = ReadListFileLine(pCache, lpFindFileData->cFileName, sizeof(lpFindFileData->cFileName));
            if(nLength == 0)
            {
                nError = ERROR_NO_MORE_FILES;
                break;
            }

            // If some mask entered, check it
            if(CheckWildCard(lpFindFileData->cFileName, pCache->szMask))
            {
                nError = ERROR_SUCCESS;
                break;
            }
        }
    }

    if(nError != ERROR_SUCCESS)
        SetLastError(nError);
    return (nError == ERROR_SUCCESS);
}

bool WINAPI SListFileFindClose(HANDLE hFind)
{
    TListFileCache * pCache = (TListFileCache *)hFind;

    if(pCache == NULL)
        return false;

    if(pCache->hFile != NULL)
        SFileCloseFile(pCache->hFile);
    return FreeListFileCache(pCache);
}

