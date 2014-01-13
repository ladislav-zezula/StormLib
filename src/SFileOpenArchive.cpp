/*****************************************************************************/
/* SFileOpenArchive.cpp                       Copyright Ladislav Zezula 1999 */
/*                                                                           */
/* Author : Ladislav Zezula                                                  */
/* E-mail : ladik@zezula.net                                                 */
/* WWW    : www.zezula.net                                                   */
/*---------------------------------------------------------------------------*/
/*                       Archive functions of Storm.dll                      */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* xx.xx.xx  1.00  Lad  The first version of SFileOpenArchive.cpp            */
/* 19.11.03  1.01  Dan  Big endian handling                                  */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"

/*****************************************************************************/
/* Local functions                                                           */
/*****************************************************************************/

static bool IsAviFile(DWORD * HeaderData)
{
    DWORD DwordValue0 = BSWAP_INT32_UNSIGNED(HeaderData[0]);
    DWORD DwordValue2 = BSWAP_INT32_UNSIGNED(HeaderData[2]);
    DWORD DwordValue3 = BSWAP_INT32_UNSIGNED(HeaderData[3]);

    // Test for 'RIFF', 'AVI ' or 'LIST'
    return (DwordValue0 == 0x46464952 && DwordValue2 == 0x20495641 && DwordValue3 == 0x5453494C);
}

static TMPQUserData * IsValidMpqUserData(ULONGLONG ByteOffset, ULONGLONG FileSize, void * pvUserData)
{
    TMPQUserData * pUserData;

    // BSWAP the source data and copy them to our buffer
    BSWAP_ARRAY32_UNSIGNED(&pvUserData, sizeof(TMPQUserData));
    pUserData = (TMPQUserData *)pvUserData;

    // Check the sizes
    if(pUserData->cbUserDataHeader <= pUserData->cbUserDataSize && pUserData->cbUserDataSize <= pUserData->dwHeaderOffs)
    {
        // Move to the position given by the userdata
        ByteOffset += pUserData->dwHeaderOffs;

        // The MPQ header should be within range of the file size
        if((ByteOffset + MPQ_HEADER_SIZE_V1) < FileSize)
        {
            // Note: We should verify if there is the MPQ header.
            // However, the header could be at any position below that
            // that is multiplier of 0x200
            return (TMPQUserData *)pvUserData;
        }
    }

    return NULL;
}

// This function gets the right positions of the hash table and the block table.
static int VerifyMpqTablePositions(TMPQArchive * ha, ULONGLONG FileSize)
{
    TMPQHeader * pHeader = ha->pHeader;
    ULONGLONG ByteOffset;

    // Check the begin of HET table
    if(pHeader->HetTablePos64)
    {
        ByteOffset = ha->MpqPos + pHeader->HetTablePos64;
        if(ByteOffset > FileSize)
            return ERROR_BAD_FORMAT;
    }

    // Check the begin of BET table
    if(pHeader->BetTablePos64)
    {
        ByteOffset = ha->MpqPos + pHeader->BetTablePos64;
        if(ByteOffset > FileSize)
            return ERROR_BAD_FORMAT;
    }

    // Check the begin of hash table
    if(pHeader->wHashTablePosHi || pHeader->dwHashTablePos)
    {
        ByteOffset = ha->MpqPos + MAKE_OFFSET64(pHeader->wHashTablePosHi, pHeader->dwHashTablePos);
        if(ByteOffset > FileSize)
            return ERROR_BAD_FORMAT;
    }

    // Check the begin of block table
    if(pHeader->wBlockTablePosHi || pHeader->dwBlockTablePos)
    {
        if((pHeader->wFormatVersion == MPQ_FORMAT_VERSION_1) && (ha->dwFlags & MPQ_FLAG_MALFORMED))
        {
            ByteOffset = (DWORD)ha->MpqPos + pHeader->dwBlockTablePos;
            if(ByteOffset > FileSize)
                return ERROR_BAD_FORMAT;
        }
        else
        {
            ByteOffset = ha->MpqPos + MAKE_OFFSET64(pHeader->wBlockTablePosHi, pHeader->dwBlockTablePos);
            if(ByteOffset > FileSize)
                return ERROR_BAD_FORMAT;
        }
    }

    // Check the begin of hi-block table
    if(pHeader->HiBlockTablePos64 != 0)
    {
        ByteOffset = ha->MpqPos + pHeader->HiBlockTablePos64;
        if(ByteOffset > FileSize)
            return ERROR_BAD_FORMAT;
    }

    // All OK.
    return ERROR_SUCCESS;
}


/*****************************************************************************/
/* Public functions                                                          */
/*****************************************************************************/

//-----------------------------------------------------------------------------
// SFileGetLocale and SFileSetLocale
// Set the locale for all newly opened files

LCID WINAPI SFileGetLocale()
{
    return lcFileLocale;
}

LCID WINAPI SFileSetLocale(LCID lcNewLocale)
{
    lcFileLocale = lcNewLocale;
    return lcFileLocale;
}

//-----------------------------------------------------------------------------
// SFileOpenArchive
//
//   szFileName - MPQ archive file name to open
//   dwPriority - When SFileOpenFileEx called, this contains the search priority for searched archives
//   dwFlags    - See MPQ_OPEN_XXX in StormLib.h
//   phMpq      - Pointer to store open archive handle

bool WINAPI SFileOpenArchive(
    const TCHAR * szMpqName,
    DWORD dwPriority,
    DWORD dwFlags,
    HANDLE * phMpq)
{
    TMPQUserData * pUserData;
    TFileStream * pStream = NULL;       // Open file stream
    TMPQArchive * ha = NULL;            // Archive handle
    TFileEntry * pFileEntry;
    ULONGLONG FileSize = 0;             // Size of the file
    int nError = ERROR_SUCCESS;   

    // Verify the parameters
    if(szMpqName == NULL || *szMpqName == 0 || phMpq == NULL)
        nError = ERROR_INVALID_PARAMETER;

    // One time initialization of MPQ cryptography
    InitializeMpqCryptography();
    dwPriority = dwPriority;

    // Open the MPQ archive file
    if(nError == ERROR_SUCCESS)
    {
        DWORD dwStreamFlags = (dwFlags & STREAM_FLAGS_MASK);

        // If not forcing MPQ v 1.0, also use file bitmap
        dwStreamFlags |= (dwFlags & MPQ_OPEN_FORCE_MPQ_V1) ? 0 : STREAM_FLAG_USE_BITMAP;

        // Initialize the stream
        pStream = FileStream_OpenFile(szMpqName, dwStreamFlags);
        if(pStream == NULL)
            nError = GetLastError();
    }

    // Check the file size. There must be at least 0x20 bytes
    if(nError == ERROR_SUCCESS)
    {
        FileStream_GetSize(pStream, &FileSize);
        if(FileSize < MPQ_HEADER_SIZE_V1)
            nError = ERROR_BAD_FORMAT;
    }

    // Allocate the MPQhandle
    if(nError == ERROR_SUCCESS)
    {
        if((ha = STORM_ALLOC(TMPQArchive, 1)) == NULL)
            nError = ERROR_NOT_ENOUGH_MEMORY;
    }

    // Initialize handle structure and allocate structure for MPQ header
    if(nError == ERROR_SUCCESS)
    {
        ULONGLONG SearchOffset = 0;
        DWORD dwStreamFlags = 0;
        DWORD dwHeaderSize;
        DWORD dwHeaderID;

        memset(ha, 0, sizeof(TMPQArchive));
        ha->pfnHashString = HashString;
        ha->pStream = pStream;
        pStream = NULL;

        // Set the archive read only if the stream is read-only
        FileStream_GetFlags(ha->pStream, &dwStreamFlags);
        ha->dwFlags |= (dwStreamFlags & STREAM_FLAG_READ_ONLY) ? MPQ_FLAG_READ_ONLY : 0;

        // Also remember if we shall check sector CRCs when reading file
        if(dwFlags & MPQ_OPEN_CHECK_SECTOR_CRC)
            ha->dwFlags |= MPQ_FLAG_CHECK_SECTOR_CRC;

        // Find the offset of MPQ header within the file
        while(SearchOffset < FileSize)
        {
            DWORD dwBytesAvailable = MPQ_HEADER_SIZE_V4;

            // Cut the bytes available, if needed
            if((FileSize - SearchOffset) < MPQ_HEADER_SIZE_V4)
                dwBytesAvailable = (DWORD)(FileSize - SearchOffset);

            // Read the eventual MPQ header
            if(!FileStream_Read(ha->pStream, &SearchOffset, ha->HeaderData, dwBytesAvailable))
            {
                nError = GetLastError();
                break;
            }

            // There are AVI files from Warcraft III with 'MPQ' extension.
            if(SearchOffset == 0 && IsAviFile(ha->HeaderData))
            {
                nError = ERROR_AVI_FILE;
                break;
            }

            // If there is the MPQ user data signature, process it
            dwHeaderID = BSWAP_INT32_UNSIGNED(ha->HeaderData[0]);
            if(dwHeaderID == ID_MPQ_USERDATA && ha->pUserData == NULL && (dwFlags & MPQ_OPEN_FORCE_MPQ_V1) == 0)
            {
                // Verify if this looks like a valid user data
                pUserData = IsValidMpqUserData(SearchOffset, FileSize, ha->HeaderData);
                if(pUserData != NULL)
                {
                    // Fill the user data header
                    ha->UserDataPos = SearchOffset;
                    ha->pUserData = &ha->UserData;
                    memcpy(ha->pUserData, pUserData, sizeof(TMPQUserData));

                    // Continue searching from that position
                    SearchOffset += ha->pUserData->dwHeaderOffs;
                    continue;
                }
            }

            // There must be MPQ header signature. Note that STORM.dll from Warcraft III actually
            // tests the MPQ header size. It must be at least 0x20 bytes in order to load it
            // Abused by Spazzler Map protector. Note that the size check is not present
            // in Storm.dll v 1.00, so Diablo I code would load the MPQ anyway.
            dwHeaderSize = BSWAP_INT32_UNSIGNED(ha->HeaderData[1]);
            if(dwHeaderID == ID_MPQ && dwHeaderSize >= MPQ_HEADER_SIZE_V1)
            {
                // Now convert the header to version 4
                nError = ConvertMpqHeaderToFormat4(ha, SearchOffset, FileSize, dwFlags);
                break;
            }

            // Check for MPK archives (Longwu Online - MPQ fork)
            if(dwHeaderID == ID_MPK)
            {
                // Now convert the MPK header to MPQ Header version 4
                nError = ConvertMpkHeaderToFormat4(ha, FileSize, dwFlags);
                break;
            }

            // If searching for the MPQ header is disabled, return an error
            if(dwFlags & MPQ_OPEN_NO_HEADER_SEARCH)
            {
                nError = ERROR_NOT_SUPPORTED;
                break;
            }

            // Move to the next possible offset
            SearchOffset += 0x200;
        }

        // Did we identify one of the supported headers?
        if(nError == ERROR_SUCCESS)
        {
            // Set the user data position to the MPQ header, if none
            if(ha->pUserData == NULL)
                ha->UserDataPos = SearchOffset;

            // Set the position of the MPQ header
            ha->pHeader = (TMPQHeader *)ha->HeaderData;
            ha->MpqPos = SearchOffset;

            // Sector size must be nonzero.
            if(SearchOffset >= FileSize || ha->pHeader->wSectorSize == 0)
                nError = ERROR_BAD_FORMAT;
        }
    }

    // Fix table positions according to format
    if(nError == ERROR_SUCCESS)
    {
        // Dump the header
//      DumpMpqHeader(ha->pHeader);

        // W3x Map Protectors use the fact that War3's Storm.dll ignores the MPQ user data,
        // and ignores the MPQ format version as well. The trick is to
        // fake MPQ format 2, with an improper hi-word position of hash table and block table
        // We can overcome such protectors by forcing opening the archive as MPQ v 1.0
        if(dwFlags & MPQ_OPEN_FORCE_MPQ_V1)
        {
            ha->pHeader->wFormatVersion = MPQ_FORMAT_VERSION_1;
            ha->pHeader->dwHeaderSize = MPQ_HEADER_SIZE_V1;
            ha->dwFlags |= MPQ_FLAG_READ_ONLY;
            ha->pUserData = NULL;
        }

        // Both MPQ_OPEN_NO_LISTFILE or MPQ_OPEN_NO_ATTRIBUTES trigger read only mode
        if(dwFlags & (MPQ_OPEN_NO_LISTFILE | MPQ_OPEN_NO_ATTRIBUTES))
            ha->dwFlags |= MPQ_FLAG_READ_ONLY;

        // Set the size of file sector
        ha->dwSectorSize = (0x200 << ha->pHeader->wSectorSize);

        // Verify if any of the tables doesn't start beyond the end of the file
        nError = VerifyMpqTablePositions(ha, FileSize);
    }

    // Read the hash table. Ignore the result, as hash table is no longer required
    // Read HET table. Ignore the result, as HET table is no longer required
    if(nError == ERROR_SUCCESS)
    {
        nError = LoadAnyHashTable(ha);
    }

    // Now, build the file table. It will be built by combining
    // the block table, BET table, hi-block table, (attributes) and (listfile).
    if(nError == ERROR_SUCCESS)
    {
        nError = BuildFileTable(ha);
    }

    // Verify the file table, if no kind of malformation was detected
    if(nError == ERROR_SUCCESS && (ha->dwFlags & MPQ_FLAG_MALFORMED) == 0)
    {
        TFileEntry * pFileTableEnd = ha->pFileTable + ha->dwFileTableSize;
        ULONGLONG RawFilePos;

        // Parse all file entries
        for(pFileEntry = ha->pFileTable; pFileEntry < pFileTableEnd; pFileEntry++)
        {
            // If that file entry is valid, check the file position
            if(pFileEntry->dwFlags & MPQ_FILE_EXISTS)
            {
                // Get the 64-bit file position,
                // relative to the begin of the file
                RawFilePos = ha->MpqPos + pFileEntry->ByteOffset;

                // Begin of the file must be within range
                if(RawFilePos > FileSize)
                {
                    nError = ERROR_FILE_CORRUPT;
                    break;
                }

                // End of the file must be within range
                RawFilePos += pFileEntry->dwCmpSize;
                if(RawFilePos > FileSize)
                {
                    nError = ERROR_FILE_CORRUPT;
                    break;
                }
            }
        }
    }

    // Load the internal listfile and include it to the file table
    if(nError == ERROR_SUCCESS && (dwFlags & MPQ_OPEN_NO_LISTFILE) == 0)
    {
        // Save the flags for (listfile)
        pFileEntry = GetFileEntryLocale(ha, LISTFILE_NAME, LANG_NEUTRAL);
        if(pFileEntry != NULL)
        {
            // Ignore result of the operation. (listfile) is optional.
            SFileAddListFile((HANDLE)ha, NULL);
            ha->dwFileFlags1 = pFileEntry->dwFlags;
        }
    }

    // Load the "(attributes)" file and merge it to the file table
    if(nError == ERROR_SUCCESS && (dwFlags & MPQ_OPEN_NO_ATTRIBUTES) == 0)
    {
        // Save the flags for (attributes)
        pFileEntry = GetFileEntryLocale(ha, ATTRIBUTES_NAME, LANG_NEUTRAL);
        if(pFileEntry != NULL)
        {
            // Ignore result of the operation. (attributes) is optional.
            SAttrLoadAttributes(ha);
            ha->dwFileFlags2 = pFileEntry->dwFlags;
        }
    }

    // Cleanup and exit
    if(nError != ERROR_SUCCESS)
    {
        FileStream_Close(pStream);
        FreeMPQArchive(ha);
        SetLastError(nError);
        ha = NULL;
    }

    *phMpq = ha;
    return (nError == ERROR_SUCCESS);
}

//-----------------------------------------------------------------------------
// bool WINAPI SFileSetDownloadCallback(HANDLE, SFILE_DOWNLOAD_CALLBACK, void *);
//
// Sets a callback that is called when content is downloaded from the master MPQ
//

bool WINAPI SFileSetDownloadCallback(HANDLE hMpq, SFILE_DOWNLOAD_CALLBACK DownloadCB, void * pvUserData)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;

    // Do nothing if 'hMpq' is bad parameter
    if(!IsValidMpqHandle(hMpq))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return false;
    }

    return FileStream_SetCallback(ha->pStream, DownloadCB, pvUserData);
}

//-----------------------------------------------------------------------------
// bool SFileFlushArchive(HANDLE hMpq)
//
// Saves all dirty data into MPQ archive.
// Has similar effect like SFileCloseArchive, but the archive is not closed.
// Use on clients who keep MPQ archive open even for write operations,
// and terminating without calling SFileCloseArchive might corrupt the archive.
//

bool WINAPI SFileFlushArchive(HANDLE hMpq)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    int nResultError = ERROR_SUCCESS;
    int nError;

    // Do nothing if 'hMpq' is bad parameter
    if(!IsValidMpqHandle(hMpq))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return false;
    }

    // Indicate that we are saving MPQ internal structures
    ha->dwFlags |= MPQ_FLAG_SAVING_TABLES;

    // If the (listfile) has been invalidated, save it
    if(ha->dwFlags & MPQ_FLAG_LISTFILE_INVALID)
    {
        nError = SListFileSaveToMpq(ha);
        if(nError != ERROR_SUCCESS)
            nResultError = nError;
    }

    // If the (attributes) has been invalidated, save it
    if(ha->dwFlags & MPQ_FLAG_ATTRIBUTES_INVALID)
    {
        nError = SAttrFileSaveToMpq(ha);
        if(nError != ERROR_SUCCESS)
            nResultError = nError;
    }

    // Save HET table, BET table, hash table, block table, hi-block table
    if(ha->dwFlags & MPQ_FLAG_CHANGED)
    {
        nError = SaveMPQTables(ha);
        if(nError != ERROR_SUCCESS)
            nResultError = nError;
    }

    // We are no longer saving internal MPQ structures
    ha->dwFlags &= ~MPQ_FLAG_SAVING_TABLES;

    // Return the error
    if(nResultError != ERROR_SUCCESS)
        SetLastError(nResultError);
    return (nResultError == ERROR_SUCCESS);
}

//-----------------------------------------------------------------------------
// bool SFileCloseArchive(HANDLE hMpq);
//

bool WINAPI SFileCloseArchive(HANDLE hMpq)
{
    TMPQArchive * ha = (TMPQArchive *)hMpq;
    bool bResult;

    // Flush all unsaved data to the storage
    bResult = SFileFlushArchive(hMpq);

    // Free all memory used by MPQ archive
    FreeMPQArchive(ha);
    return bResult;
}

