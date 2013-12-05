/*****************************************************************************/
/* SFileReadFile.cpp                      Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* Description :                                                             */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* xx.xx.99  1.00  Lad  The first version of SFileReadFile.cpp               */
/* 24.03.99  1.00  Lad  Added the SFileGetFileInfo function                  */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "StormCommon.h"

//-----------------------------------------------------------------------------
// Local functions

static DWORD GetMpqFileCount(TMPQArchive * ha)
{
    TFileEntry * pFileTableEnd;
    TFileEntry * pFileEntry;
    DWORD dwFileCount = 0;

    // Go through all open MPQs, including patches
    while(ha != NULL)
    {
        // Only count files that are not patch files
        pFileTableEnd = ha->pFileTable + ha->dwFileTableSize;
        for(pFileEntry = ha->pFileTable; pFileEntry < pFileTableEnd; pFileEntry++)
        {
            // If the file is patch file and this is not primary archive, skip it
            // BUGBUG: This errorneously counts non-patch files that are in both
            // base MPQ and in patches, and increases the number of files by cca 50%
            if((pFileEntry->dwFlags & (MPQ_FILE_EXISTS | MPQ_FILE_PATCH_FILE)) == MPQ_FILE_EXISTS)
                dwFileCount++;
        }

        // Move to the next patch archive
        ha = ha->haPatch;
    }

    return dwFileCount;
}

static TCHAR * GetFilePatchChain(TMPQFile * hf, DWORD * pcbChainLength)
{
    TMPQFile * hfTemp;
    TCHAR * szPatchChain = NULL;
    TCHAR * szPatchItem = NULL;
    TCHAR * szFileName;
    size_t cchCharsNeeded = 1;
    size_t nLength;

    // Patch chain is only supported on MPQ files.
    if(hf->pStream == NULL)
    {
        // Calculate the necessary length of the multi-string
        for(hfTemp = hf; hfTemp != NULL; hfTemp->hfPatchFile)
            cchCharsNeeded += _tcslen(FileStream_GetFileName(hfTemp->ha->pStream)) + 1;
        
        // Allocate space for the multi-string
        szPatchChain = szPatchItem = STORM_ALLOC(TCHAR, cchCharsNeeded);
        if(szPatchChain != NULL)
        {
            // Fill-in all the names
            for(hfTemp = hf; hfTemp != NULL; hfTemp = hfTemp->hfPatchFile)
            {
                szFileName = FileStream_GetFileName(hfTemp->ha->pStream);
                nLength = _tcslen(szFileName) + 1;

                memcpy(szPatchItem, szFileName, nLength * sizeof(TCHAR));
                szPatchItem += nLength;
            }

            // Terminate the multi-string
            *szPatchItem++ = 0;
        }

        // The length must match
        assert((size_t)(szPatchItem - szPatchChain) == cchCharsNeeded);
    }

    // Give the length of the patch chain, in bytes
    if(pcbChainLength != NULL)
        pcbChainLength[0] = (DWORD)(cchCharsNeeded * sizeof(TCHAR));
    return szPatchChain;
}

//  hf            - MPQ File handle.
//  pbBuffer      - Pointer to target buffer to store sectors.
//  dwByteOffset  - Position of sector in the file (relative to file begin)
//  dwBytesToRead - Number of bytes to read. Must be multiplier of sector size.
//  pdwBytesRead  - Stored number of bytes loaded
static int ReadMpqSectors(TMPQFile * hf, LPBYTE pbBuffer, DWORD dwByteOffset, DWORD dwBytesToRead, LPDWORD pdwBytesRead)
{
    ULONGLONG RawFilePos;
    TMPQArchive * ha = hf->ha;
    TFileEntry * pFileEntry = hf->pFileEntry;
    LPBYTE pbRawSector = NULL;
    LPBYTE pbOutSector = pbBuffer;
    LPBYTE pbInSector = pbBuffer;
    DWORD dwRawBytesToRead;
    DWORD dwRawSectorOffset = dwByteOffset;
    DWORD dwSectorsToRead = dwBytesToRead / ha->dwSectorSize;
    DWORD dwSectorIndex = dwByteOffset / ha->dwSectorSize;
    DWORD dwSectorsDone = 0;
    DWORD dwBytesRead = 0;
    int nError = ERROR_SUCCESS;

    // Note that dwByteOffset must be aligned to size of one sector
    // Note that dwBytesToRead must be a multiplier of one sector size
    // This is local function, so we won't check if that's true.
    // Note that files stored in single units are processed by a separate function

    // If there is not enough bytes remaining, cut dwBytesToRead
    if((dwByteOffset + dwBytesToRead) > hf->dwDataSize)
        dwBytesToRead = hf->dwDataSize - dwByteOffset;
    dwRawBytesToRead = dwBytesToRead;

    // Perform all necessary work to do with compressed files
    if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS_MASK)
    {
        // If the sector positions are not loaded yet, do it
        if(hf->SectorOffsets == NULL)
        {
            nError = AllocateSectorOffsets(hf, true);
            if(nError != ERROR_SUCCESS)
                return nError;
        }

        // If the sector checksums are not loaded yet, load them now.
        if(hf->SectorChksums == NULL && (pFileEntry->dwFlags & MPQ_FILE_SECTOR_CRC) && hf->bLoadedSectorCRCs == false)
        {
            //
            // Sector CRCs is plain crap feature. It is almost never present,
            // often it's empty, or the end offset of sector CRCs is zero.
            // We only try to load sector CRCs once, and regardless if it fails
            // or not, we won't try that again for the given file.
            //

            AllocateSectorChecksums(hf, true);
            hf->bLoadedSectorCRCs = true;
        }

        // TODO: If the raw data MD5s are not loaded yet, load them now
        // Only do it if the MPQ is of format 4.0
//      if(ha->pHeader->wFormatVersion >= MPQ_FORMAT_VERSION_4 && ha->pHeader->dwRawChunkSize != 0)
//      {
//          nError = AllocateRawMD5s(hf, true);
//          if(nError != ERROR_SUCCESS)
//              return nError;
//      }

        // If the file is compressed, also allocate secondary buffer
        pbInSector = pbRawSector = STORM_ALLOC(BYTE, dwBytesToRead);
        if(pbRawSector == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;

        // Assign the temporary buffer as target for read operation
        dwRawSectorOffset = hf->SectorOffsets[dwSectorIndex];
        dwRawBytesToRead = hf->SectorOffsets[dwSectorIndex + dwSectorsToRead] - dwRawSectorOffset;
    }

    // Calculate raw file offset where the sector(s) are stored.
    CalculateRawSectorOffset(RawFilePos, hf, dwRawSectorOffset);

    // Set file pointer and read all required sectors
    if(!FileStream_Read(ha->pStream, &RawFilePos, pbInSector, dwRawBytesToRead))
        return GetLastError();
    dwBytesRead = 0;

    // Now we have to decrypt and decompress all file sectors that have been loaded
    for(DWORD i = 0; i < dwSectorsToRead; i++)
    {
        DWORD dwRawBytesInThisSector = ha->dwSectorSize;
        DWORD dwBytesInThisSector = ha->dwSectorSize;
        DWORD dwIndex = dwSectorIndex + i;

        // If there is not enough bytes in the last sector,
        // cut the number of bytes in this sector
        if(dwRawBytesInThisSector > dwBytesToRead)
            dwRawBytesInThisSector = dwBytesToRead;
        if(dwBytesInThisSector > dwBytesToRead)
            dwBytesInThisSector = dwBytesToRead;

        // If the file is compressed, we have to adjust the raw sector size
        if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS_MASK)
            dwRawBytesInThisSector = hf->SectorOffsets[dwIndex + 1] - hf->SectorOffsets[dwIndex];

        // If the file is encrypted, we have to decrypt the sector
        if(pFileEntry->dwFlags & MPQ_FILE_ENCRYPTED)
        {
            BSWAP_ARRAY32_UNSIGNED(pbInSector, dwRawBytesInThisSector);

            // If we don't know the key, try to detect it by file content
            if(hf->dwFileKey == 0)
            {
                hf->dwFileKey = DetectFileKeyByContent(pbInSector, dwBytesInThisSector);
                if(hf->dwFileKey == 0)
                {
                    nError = ERROR_UNKNOWN_FILE_KEY;
                    break;
                }
            }

            DecryptMpqBlock(pbInSector, dwRawBytesInThisSector, hf->dwFileKey + dwIndex);
            BSWAP_ARRAY32_UNSIGNED(pbInSector, dwRawBytesInThisSector);
        }

        // If the file has sector CRC check turned on, perform it
        if(hf->bCheckSectorCRCs && hf->SectorChksums != NULL)
        {
            DWORD dwAdlerExpected = hf->SectorChksums[dwIndex];
            DWORD dwAdlerValue = 0;

            // We can only check sector CRC when it's not zero
            // Neither can we check it if it's 0xFFFFFFFF.
            if(dwAdlerExpected != 0 && dwAdlerExpected != 0xFFFFFFFF)
            {
                dwAdlerValue = adler32(0, pbInSector, dwRawBytesInThisSector);
                if(dwAdlerValue != dwAdlerExpected)
                {
                    nError = ERROR_CHECKSUM_ERROR;
                    break;
                }
            }
        }

        // If the sector is really compressed, decompress it.
        // WARNING : Some sectors may not be compressed, it can be determined only
        // by comparing uncompressed and compressed size !!!
        if(dwRawBytesInThisSector < dwBytesInThisSector)
        {
            int cbOutSector = dwBytesInThisSector;
            int cbInSector = dwRawBytesInThisSector;
            int nResult = 0;

            // Is the file compressed by Blizzard's multiple compression ?
            if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS)
            {
                if(ha->pHeader->wFormatVersion >= MPQ_FORMAT_VERSION_2)
                    nResult = SCompDecompress2(pbOutSector, &cbOutSector, pbInSector, cbInSector);
                else
                    nResult = SCompDecompress(pbOutSector, &cbOutSector, pbInSector, cbInSector);
            }

            // Is the file compressed by PKWARE Data Compression Library ?
            else if(pFileEntry->dwFlags & MPQ_FILE_IMPLODE)
            {
                nResult = SCompExplode(pbOutSector, &cbOutSector, pbInSector, cbInSector);
            }

            // Did the decompression fail ?
            if(nResult == 0)
            {
                nError = ERROR_FILE_CORRUPT;
                break;
            }
        }
        else
        {
            if(pbOutSector != pbInSector)
                memcpy(pbOutSector, pbInSector, dwBytesInThisSector);
        }

        // Move pointers
        dwBytesToRead -= dwBytesInThisSector;
        dwByteOffset += dwBytesInThisSector;
        dwBytesRead += dwBytesInThisSector;
        pbOutSector += dwBytesInThisSector;
        pbInSector += dwRawBytesInThisSector;
        dwSectorsDone++;
    }

    // Free all used buffers
    if(pbRawSector != NULL)
        STORM_FREE(pbRawSector);
    
    // Give the caller thenumber of bytes read
    *pdwBytesRead = dwBytesRead;
    return nError; 
}

static int ReadMpqFileSingleUnit(TMPQFile * hf, void * pvBuffer, DWORD dwFilePos, DWORD dwToRead, LPDWORD pdwBytesRead)
{
    ULONGLONG RawFilePos = hf->RawFilePos;
    TMPQArchive * ha = hf->ha;
    TFileEntry * pFileEntry = hf->pFileEntry;
    LPBYTE pbCompressed = NULL;
    LPBYTE pbRawData = NULL;
    int nError = ERROR_SUCCESS;

    // If the file buffer is not allocated yet, do it.
    if(hf->pbFileSector == NULL)
    {
        nError = AllocateSectorBuffer(hf);
        if(nError != ERROR_SUCCESS)
            return nError;
        pbRawData = hf->pbFileSector;
    }

    // If the file is a patch file, adjust raw data offset
    if(hf->pPatchInfo != NULL)
        RawFilePos += hf->pPatchInfo->dwLength;

    // If the file sector is not loaded yet, do it
    if(hf->dwSectorOffs != 0)
    {
        // Is the file compressed?
        if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS_MASK)
        {
            // Allocate space for compressed data
            pbCompressed = STORM_ALLOC(BYTE, pFileEntry->dwCmpSize);
            if(pbCompressed == NULL)
                return ERROR_NOT_ENOUGH_MEMORY;
            pbRawData = pbCompressed;
        }
        
        // Load the raw (compressed, encrypted) data
        if(!FileStream_Read(ha->pStream, &RawFilePos, pbRawData, pFileEntry->dwCmpSize))
        {
            STORM_FREE(pbCompressed);
            return GetLastError();
        }

        // If the file is encrypted, we have to decrypt the data first
        if(pFileEntry->dwFlags & MPQ_FILE_ENCRYPTED)
        {
            BSWAP_ARRAY32_UNSIGNED(pbRawData, pFileEntry->dwCmpSize);
            DecryptMpqBlock(pbRawData, pFileEntry->dwCmpSize, hf->dwFileKey);
            BSWAP_ARRAY32_UNSIGNED(pbRawData, pFileEntry->dwCmpSize);
        }

        // If the file is compressed, we have to decompress it now
        if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS_MASK)
        {
            int cbOutBuffer = (int)hf->dwDataSize;
            int cbInBuffer = (int)pFileEntry->dwCmpSize;
            int nResult = 0;

            //
            // If the file is an incremental patch, the size of compressed data
            // is determined as pFileEntry->dwCmpSize - sizeof(TPatchInfo)
            //
            // In "wow-update-12694.MPQ" from Wow-Cataclysm BETA:
            //
            // File                                    CmprSize   DcmpSize DataSize Compressed?
            // --------------------------------------  ---------- -------- -------- ---------------
            // esES\DBFilesClient\LightSkyBox.dbc      0xBE->0xA2  0xBC     0xBC     Yes
            // deDE\DBFilesClient\MountCapability.dbc  0x93->0x77  0x77     0x77     No
            // 

            if(pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE)
                cbInBuffer = cbInBuffer - sizeof(TPatchInfo);

            // Is the file compressed by Blizzard's multiple compression ?
            if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS)
            {
                if(ha->pHeader->wFormatVersion >= MPQ_FORMAT_VERSION_2)
                    nResult = SCompDecompress2(hf->pbFileSector, &cbOutBuffer, pbRawData, cbInBuffer);
                else
                    nResult = SCompDecompress(hf->pbFileSector, &cbOutBuffer, pbRawData, cbInBuffer);
            }

            // Is the file compressed by PKWARE Data Compression Library ?
            // Note: Single unit files compressed with IMPLODE are not supported by Blizzard
            else if(pFileEntry->dwFlags & MPQ_FILE_IMPLODE)
                nResult = SCompExplode(hf->pbFileSector, &cbOutBuffer, pbRawData, cbInBuffer);

            nError = (nResult != 0) ? ERROR_SUCCESS : ERROR_FILE_CORRUPT;
        }
        else
        {
            if(pbRawData != hf->pbFileSector)
                memcpy(hf->pbFileSector, pbRawData, hf->dwDataSize);
        }

        // Free the decompression buffer.
        if(pbCompressed != NULL)
            STORM_FREE(pbCompressed);

        // The file sector is now properly loaded
        hf->dwSectorOffs = 0;
    }

    // At this moment, we have the file loaded into the file buffer.
    // Copy as much as the caller wants
    if(nError == ERROR_SUCCESS && hf->dwSectorOffs == 0)
    {
        // File position is greater or equal to file size ?
        if(dwFilePos >= hf->dwDataSize)
        {
            *pdwBytesRead = 0;
            return ERROR_SUCCESS;
        }

        // If not enough bytes remaining in the file, cut them
        if((hf->dwDataSize - dwFilePos) < dwToRead)
            dwToRead = (hf->dwDataSize - dwFilePos);

        // Copy the bytes
        memcpy(pvBuffer, hf->pbFileSector + dwFilePos, dwToRead);

        // Give the number of bytes read
        *pdwBytesRead = dwToRead;
        return ERROR_SUCCESS;
    }

    // An error, sorry
    return ERROR_CAN_NOT_COMPLETE;
}

static int ReadMpkFileSingleUnit(TMPQFile * hf, void * pvBuffer, DWORD dwFilePos, DWORD dwToRead, LPDWORD pdwBytesRead)
{
    ULONGLONG RawFilePos = hf->RawFilePos + 0x0C;   // For some reason, MPK files start at position (hf->RawFilePos + 0x0C)
    TMPQArchive * ha = hf->ha;
    TFileEntry * pFileEntry = hf->pFileEntry;
    LPBYTE pbCompressed = NULL;
    LPBYTE pbRawData = hf->pbFileSector;
    int nError = ERROR_SUCCESS;

    // We do not support patch files in MPK archives
    assert(hf->pPatchInfo == NULL);

    // If the file buffer is not allocated yet, do it.
    if(hf->pbFileSector == NULL)
    {
        nError = AllocateSectorBuffer(hf);
        if(nError != ERROR_SUCCESS)
            return nError;

        // Is the file compressed?
        if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS_MASK)
        {
            // Allocate space for compressed data
            pbCompressed = STORM_ALLOC(BYTE, pFileEntry->dwCmpSize);
            if(pbCompressed == NULL)
                return ERROR_NOT_ENOUGH_MEMORY;
            pbRawData = pbCompressed;
        }
        
        // Load the raw (compressed, encrypted) data
        if(!FileStream_Read(ha->pStream, &RawFilePos, pbRawData, pFileEntry->dwCmpSize))
        {
            STORM_FREE(pbCompressed);
            return GetLastError();
        }

        // If the file is encrypted, we have to decrypt the data first
        if(pFileEntry->dwFlags & MPQ_FILE_ENCRYPTED)
        {
            DecryptMpkTable(pbRawData, pFileEntry->dwCmpSize);
        }

        // If the file is compressed, we have to decompress it now
        if(pFileEntry->dwFlags & MPQ_FILE_COMPRESS_MASK)
        {
            int cbOutBuffer = (int)hf->dwDataSize;

            if(!SCompDecompressMpk(hf->pbFileSector, &cbOutBuffer, pbRawData, (int)pFileEntry->dwCmpSize))
                nError = ERROR_FILE_CORRUPT;
        }
        else
        {
            if(pbRawData != hf->pbFileSector)
                memcpy(hf->pbFileSector, pbRawData, hf->dwDataSize);
        }

        // Free the decompression buffer.
        if(pbCompressed != NULL)
            STORM_FREE(pbCompressed);

        // The file sector is now properly loaded
        hf->dwSectorOffs = 0;
    }

    // At this moment, we have the file loaded into the file buffer.
    // Copy as much as the caller wants
    if(nError == ERROR_SUCCESS && hf->dwSectorOffs == 0)
    {
        // File position is greater or equal to file size ?
        if(dwFilePos >= hf->dwDataSize)
        {
            *pdwBytesRead = 0;
            return ERROR_SUCCESS;
        }

        // If not enough bytes remaining in the file, cut them
        if((hf->dwDataSize - dwFilePos) < dwToRead)
            dwToRead = (hf->dwDataSize - dwFilePos);

        // Copy the bytes
        memcpy(pvBuffer, hf->pbFileSector + dwFilePos, dwToRead);

        // Give the number of bytes read
        *pdwBytesRead = dwToRead;
        return ERROR_SUCCESS;
    }

    // An error, sorry
    return ERROR_CAN_NOT_COMPLETE;
}


static int ReadMpqFileSectorFile(TMPQFile * hf, void * pvBuffer, DWORD dwFilePos, DWORD dwBytesToRead, LPDWORD pdwBytesRead)
{
    TMPQArchive * ha = hf->ha;
    LPBYTE pbBuffer = (BYTE *)pvBuffer;
    DWORD dwTotalBytesRead = 0;                         // Total bytes read in all three parts
    DWORD dwSectorSizeMask = ha->dwSectorSize - 1;      // Mask for block size, usually 0x0FFF
    DWORD dwFileSectorPos;                              // File offset of the loaded sector
    DWORD dwBytesRead;                                  // Number of bytes read (temporary variable)
    int nError;

    // If the file position is at or beyond end of file, do nothing
    if(dwFilePos >= hf->dwDataSize)
    {
        *pdwBytesRead = 0;
        return ERROR_SUCCESS;
    }

    // If not enough bytes in the file remaining, cut them
    if(dwBytesToRead > (hf->dwDataSize - dwFilePos))
        dwBytesToRead = (hf->dwDataSize - dwFilePos);

    // Compute sector position in the file
    dwFileSectorPos = dwFilePos & ~dwSectorSizeMask;  // Position in the block

    // If the file sector buffer is not allocated yet, do it now
    if(hf->pbFileSector == NULL)
    {
        nError = AllocateSectorBuffer(hf);
        if(nError != ERROR_SUCCESS)
            return nError;
    }

    // Load the first (incomplete) file sector
    if(dwFilePos & dwSectorSizeMask)
    {
        DWORD dwBytesInSector = ha->dwSectorSize;
        DWORD dwBufferOffs = dwFilePos & dwSectorSizeMask;
        DWORD dwToCopy;                                     

        // Is the file sector already loaded ?
        if(hf->dwSectorOffs != dwFileSectorPos)
        {
            // Load one MPQ sector into archive buffer
            nError = ReadMpqSectors(hf, hf->pbFileSector, dwFileSectorPos, ha->dwSectorSize, &dwBytesInSector);
            if(nError != ERROR_SUCCESS)
                return nError;

            // Remember that the data loaded to the sector have new file offset
            hf->dwSectorOffs = dwFileSectorPos;
        }
        else
        {
            if((dwFileSectorPos + dwBytesInSector) > hf->dwDataSize)
                dwBytesInSector = hf->dwDataSize - dwFileSectorPos;
        }

        // Copy the data from the offset in the loaded sector to the end of the sector
        dwToCopy = dwBytesInSector - dwBufferOffs;
        if(dwToCopy > dwBytesToRead)
            dwToCopy = dwBytesToRead;

        // Copy data from sector buffer into target buffer
        memcpy(pbBuffer, hf->pbFileSector + dwBufferOffs, dwToCopy);

        // Update pointers and byte counts
        dwTotalBytesRead += dwToCopy;
        dwFileSectorPos  += dwBytesInSector;
        pbBuffer         += dwToCopy;
        dwBytesToRead    -= dwToCopy;
    }

    // Load the whole ("middle") sectors only if there is at least one full sector to be read
    if(dwBytesToRead >= ha->dwSectorSize)
    {
        DWORD dwBlockBytes = dwBytesToRead & ~dwSectorSizeMask;

        // Load all sectors to the output buffer
        nError = ReadMpqSectors(hf, pbBuffer, dwFileSectorPos, dwBlockBytes, &dwBytesRead);
        if(nError != ERROR_SUCCESS)
            return nError;

        // Update pointers
        dwTotalBytesRead += dwBytesRead;
        dwFileSectorPos  += dwBytesRead;
        pbBuffer         += dwBytesRead;
        dwBytesToRead    -= dwBytesRead;
    }

    // Read the terminating sector
    if(dwBytesToRead > 0)
    {
        DWORD dwToCopy = ha->dwSectorSize;

        // Is the file sector already loaded ?
        if(hf->dwSectorOffs != dwFileSectorPos)
        {
            // Load one MPQ sector into archive buffer
            nError = ReadMpqSectors(hf, hf->pbFileSector, dwFileSectorPos, ha->dwSectorSize, &dwBytesRead);
            if(nError != ERROR_SUCCESS)
                return nError;

            // Remember that the data loaded to the sector have new file offset
            hf->dwSectorOffs = dwFileSectorPos;
        }

        // Check number of bytes read
        if(dwToCopy > dwBytesToRead)
            dwToCopy = dwBytesToRead;

        // Copy the data from the cached last sector to the caller's buffer
        memcpy(pbBuffer, hf->pbFileSector, dwToCopy);
        
        // Update pointers
        dwTotalBytesRead += dwToCopy;
    }

    // Store total number of bytes read to the caller
    *pdwBytesRead = dwTotalBytesRead;
    return ERROR_SUCCESS;
}

static int ReadMpqFilePatchFile(TMPQFile * hf, void * pvBuffer, DWORD dwFilePos, DWORD dwToRead, LPDWORD pdwBytesRead)
{
    DWORD dwBytesToRead = dwToRead;
    DWORD dwBytesRead = 0;
    int nError = ERROR_SUCCESS;

    // Make sure that the patch file is loaded completely
    if(hf->pbFileData == NULL)
    {
        // Load the original file and store its content to "pbOldData"
        hf->pbFileData = STORM_ALLOC(BYTE, hf->pFileEntry->dwFileSize);
        hf->cbFileData = hf->pFileEntry->dwFileSize;
        if(hf->pbFileData == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;

        // Read the file data
        if(hf->pFileEntry->dwFlags & MPQ_FILE_SINGLE_UNIT)
            nError = ReadMpqFileSingleUnit(hf, hf->pbFileData, 0, hf->cbFileData, &dwBytesRead);
        else
            nError = ReadMpqFileSectorFile(hf, hf->pbFileData, 0, hf->cbFileData, &dwBytesRead);

        // Fix error code
        if(nError == ERROR_SUCCESS && dwBytesRead != hf->cbFileData)
            nError = ERROR_FILE_CORRUPT;

        // Patch the file data
        if(nError == ERROR_SUCCESS)
            nError = PatchFileData(hf);

        // Reset number of bytes read to zero
        dwBytesRead = 0;
    }

    // If there is something to read, do it
    if(nError == ERROR_SUCCESS)
    {
        if(dwFilePos < hf->cbFileData)
        {
            // Make sure we don't copy more than file size
            if((dwFilePos + dwToRead) > hf->cbFileData)
                dwToRead = hf->cbFileData - dwFilePos;

            // Copy the appropriate amount of the file data to the caller's buffer
            memcpy(pvBuffer, hf->pbFileData + dwFilePos, dwToRead);
            dwBytesRead = dwToRead;
        }

        // Set the proper error code
        nError = (dwBytesRead == dwBytesToRead) ? ERROR_SUCCESS : ERROR_HANDLE_EOF;
    }

    // Give the result to the caller
    if(pdwBytesRead != NULL)
        *pdwBytesRead = dwBytesRead;
    return nError;
}

static int ReadMpqFileLocalFile(TMPQFile * hf, void * pvBuffer, DWORD dwFilePos, DWORD dwToRead, LPDWORD pdwBytesRead)
{
    ULONGLONG FilePosition1 = dwFilePos;
    ULONGLONG FilePosition2;
    DWORD dwBytesRead = 0;
    int nError = ERROR_SUCCESS;

    assert(hf->pStream != NULL);

    // Because stream I/O functions are designed to read
    // "all or nothing", we compare file position before and after,
    // and if they differ, we assume that number of bytes read
    // is the difference between them

    if(!FileStream_Read(hf->pStream, &FilePosition1, pvBuffer, dwToRead))
    {
        // If not all bytes have been read, then return the number of bytes read
        if((nError = GetLastError()) == ERROR_HANDLE_EOF)
        {
            FileStream_GetPos(hf->pStream, &FilePosition2);
            dwBytesRead = (DWORD)(FilePosition2 - FilePosition1);
        }
    }
    else
    {
        dwBytesRead = dwToRead;
    }

    *pdwBytesRead = dwBytesRead;
    return nError;
}

//-----------------------------------------------------------------------------
// SFileReadFile

bool WINAPI SFileReadFile(HANDLE hFile, void * pvBuffer, DWORD dwToRead, LPDWORD pdwRead, LPOVERLAPPED lpOverlapped)
{
    TMPQFile * hf = (TMPQFile *)hFile;
    DWORD dwBytesRead = 0;                      // Number of bytes read
    int nError = ERROR_SUCCESS;

    // Keep compilers happy
    lpOverlapped = lpOverlapped;

    // Check valid parameters
    if(!IsValidFileHandle(hFile))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return false;
    }

    if(pvBuffer == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    // If the file is local file, read the data directly from the stream
    if(hf->pStream != NULL)
    {
        nError = ReadMpqFileLocalFile(hf, pvBuffer, hf->dwFilePos, dwToRead, &dwBytesRead);
    }

    // If the file is a patch file, we have to read it special way
    else if(hf->hfPatchFile != NULL && (hf->pFileEntry->dwFlags & MPQ_FILE_PATCH_FILE) == 0)
    {
        nError = ReadMpqFilePatchFile(hf, pvBuffer, hf->dwFilePos, dwToRead, &dwBytesRead);
    }

    // If the archive is a MPK archive, we need special way to read the file
    else if(hf->ha->dwSubType == MPQ_SUBTYPE_MPK)
    {
        nError = ReadMpkFileSingleUnit(hf, pvBuffer, hf->dwFilePos, dwToRead, &dwBytesRead);
    }

    // If the file is single unit file, redirect it to read file 
    else if(hf->pFileEntry->dwFlags & MPQ_FILE_SINGLE_UNIT)
    {
        nError = ReadMpqFileSingleUnit(hf, pvBuffer, hf->dwFilePos, dwToRead, &dwBytesRead);
    }

    // Otherwise read it as sector based MPQ file
    else
    {                                                                   
        nError = ReadMpqFileSectorFile(hf, pvBuffer, hf->dwFilePos, dwToRead, &dwBytesRead);
    }

    // Increment the file position
    hf->dwFilePos += dwBytesRead;

    // Give the caller the number of bytes read
    if(pdwRead != NULL)
        *pdwRead = dwBytesRead;

    // If the read operation succeeded, but not full number of bytes was read,
    // set the last error to ERROR_HANDLE_EOF
    if(nError == ERROR_SUCCESS && (dwBytesRead < dwToRead))
        nError = ERROR_HANDLE_EOF;

    // If something failed, set the last error value
    if(nError != ERROR_SUCCESS)
        SetLastError(nError);
    return (nError == ERROR_SUCCESS);
}

//-----------------------------------------------------------------------------
// SFileGetFileSize

DWORD WINAPI SFileGetFileSize(HANDLE hFile, LPDWORD pdwFileSizeHigh)
{
    ULONGLONG FileSize;
    TMPQFile * hf = (TMPQFile *)hFile;

    // Validate the file handle before we go on
    if(IsValidFileHandle(hFile))
    {
        // Make sure that the variable is initialized
        FileSize = 0;

        // If the file is patched file, we have to get the size of the last version
        if(hf->hfPatchFile != NULL)
        {
            // Walk through the entire patch chain, take the last version
            while(hf != NULL)
            {
                // Get the size of the currently pointed version
                FileSize = hf->pFileEntry->dwFileSize;

                // Move to the next patch file in the hierarchy
                hf = hf->hfPatchFile;
            }
        }
        else
        {
            // Is it a local file ?
            if(hf->pStream != NULL)
            {
                FileStream_GetSize(hf->pStream, &FileSize);
            }
            else
            {
                FileSize = hf->dwDataSize;
            }
        }

        // If opened from archive, return file size
        if(pdwFileSizeHigh != NULL)
            *pdwFileSizeHigh = (DWORD)(FileSize >> 32);
        return (DWORD)FileSize;
    }

    SetLastError(ERROR_INVALID_HANDLE);
    return SFILE_INVALID_SIZE;
}

DWORD WINAPI SFileSetFilePointer(HANDLE hFile, LONG lFilePos, LONG * plFilePosHigh, DWORD dwMoveMethod)
{
    TMPQFile * hf = (TMPQFile *)hFile;
    ULONGLONG FilePosition;
    ULONGLONG MoveOffset;
    DWORD dwFilePosHi;

    // If the hFile is not a valid file handle, return an error.
    if(!IsValidFileHandle(hFile))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return SFILE_INVALID_POS;
    }

    // Get the relative point where to move from
    switch(dwMoveMethod)
    {
        case FILE_BEGIN:
            FilePosition = 0;
            break;

        case FILE_CURRENT:
            if(hf->pStream != NULL)
            {
                FileStream_GetPos(hf->pStream, &FilePosition);
            }
            else
            {
                FilePosition = hf->dwFilePos;
            }
            break;

        case FILE_END:
            if(hf->pStream != NULL)
            {
                FileStream_GetSize(hf->pStream, &FilePosition);
            }
            else
            {
                FilePosition = SFileGetFileSize(hFile, NULL);
            }
            break;

        default:
            SetLastError(ERROR_INVALID_PARAMETER);
            return SFILE_INVALID_POS;
    }

    // Now get the move offset. Note that both values form
    // a signed 64-bit value (a file pointer can be moved backwards)
    if(plFilePosHigh != NULL)
        dwFilePosHi = *plFilePosHigh;
    else
        dwFilePosHi = (lFilePos & 0x80000000) ? 0xFFFFFFFF : 0;
    MoveOffset = MAKE_OFFSET64(dwFilePosHi, lFilePos);

    // Now calculate the new file pointer
    // Do not allow the file pointer to go before the begin of the file
    FilePosition += MoveOffset;
    if(FilePosition < 0)
        FilePosition = 0;

    // Now apply the file pointer to the file
    if(hf->pStream != NULL)
    {
        // Apply the new file position
        if(!FileStream_Read(hf->pStream, &FilePosition, NULL, 0))
            return SFILE_INVALID_POS;

        // Return the new file position
        if(plFilePosHigh != NULL)
            *plFilePosHigh = (LONG)(FilePosition >> 32);
        return (DWORD)FilePosition;
    }
    else
    {
        // Files in MPQ can't be bigger than 4 GB.
        // We don't allow to go past 4 GB
        if(FilePosition >> 32)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return SFILE_INVALID_POS;
        }

        // Change the file position
        hf->dwFilePos = (DWORD)FilePosition;

        // Return the new file position
        if(plFilePosHigh != NULL)
            *plFilePosHigh = 0;
        return (DWORD)FilePosition;
    }
}

//-----------------------------------------------------------------------------
// Tries to retrieve the file name

struct TFileHeader2Ext
{
    DWORD dwOffset00Data;               // Required data at offset 00 (32-bits)
    DWORD dwOffset00Mask;               // Mask for data at offset 00 (32 bits). 0 = data are ignored
    DWORD dwOffset04Data;               // Required data at offset 04 (32-bits)
    DWORD dwOffset04Mask;               // Mask for data at offset 04 (32 bits). 0 = data are ignored
    const char * szExt;                 // Supplied extension, if the condition is true
};

static TFileHeader2Ext data2ext[] = 
{
    {0x00005A4D, 0x0000FFFF, 0x00000000, 0x00000000, "exe"},    // EXE files
    {0x00000006, 0xFFFFFFFF, 0x00000001, 0xFFFFFFFF, "dc6"},    // EXE files
    {0x1A51504D, 0xFFFFFFFF, 0x00000000, 0x00000000, "mpq"},    // MPQ archive header ID ('MPQ\x1A')
    {0x46464952, 0xFFFFFFFF, 0x00000000, 0x00000000, "wav"},    // WAVE header 'RIFF'
    {0x324B4D53, 0xFFFFFFFF, 0x00000000, 0x00000000, "smk"},    // Old "Smacker Video" files 'SMK2'
    {0x694B4942, 0xFFFFFFFF, 0x00000000, 0x00000000, "bik"},    // Bink video files (new)
    {0x0801050A, 0xFFFFFFFF, 0x00000000, 0x00000000, "pcx"},    // PCX images used in Diablo I
    {0x544E4F46, 0xFFFFFFFF, 0x00000000, 0x00000000, "fnt"},    // Font files used in Diablo II
    {0x6D74683C, 0xFFFFFFFF, 0x00000000, 0x00000000, "html"},   // HTML '<htm'
    {0x4D54483C, 0xFFFFFFFF, 0x00000000, 0x00000000, "html"},   // HTML '<HTM
    {0x216F6F57, 0xFFFFFFFF, 0x00000000, 0x00000000, "tbl"},    // Table files
    {0x31504C42, 0xFFFFFFFF, 0x00000000, 0x00000000, "blp"},    // BLP textures
    {0x32504C42, 0xFFFFFFFF, 0x00000000, 0x00000000, "blp"},    // BLP textures (v2)
    {0x584C444D, 0xFFFFFFFF, 0x00000000, 0x00000000, "mdx"},    // MDX files
    {0x45505954, 0xFFFFFFFF, 0x00000000, 0x00000000, "pud"},    // Warcraft II maps
    {0x38464947, 0xFFFFFFFF, 0x00000000, 0x00000000, "gif"},    // GIF images 'GIF8'
    {0x3032444D, 0xFFFFFFFF, 0x00000000, 0x00000000, "m2"},     // WoW ??? .m2
    {0x43424457, 0xFFFFFFFF, 0x00000000, 0x00000000, "dbc"},    // ??? .dbc
    {0x47585053, 0xFFFFFFFF, 0x00000000, 0x00000000, "bls"},    // WoW pixel shaders
    {0xE0FFD8FF, 0xFFFFFFFF, 0x00000000, 0x00000000, "jpg"},    // JPEG image
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, "xxx"},    // Default extension
    {0, 0, 0, 0, NULL}                                          // Terminator 
};

static int CreatePseudoFileName(HANDLE hFile, TFileEntry * pFileEntry, char * szFileName)
{
    TMPQFile * hf = (TMPQFile *)hFile;  // MPQ File handle
    DWORD FirstBytes[2] = {0, 0};       // The first 4 bytes of the file
    DWORD dwBytesRead = 0;
    DWORD dwFilePos;                    // Saved file position

    // Read the first 2 DWORDs bytes from the file
    dwFilePos = SFileSetFilePointer(hFile, 0, NULL, FILE_CURRENT);   
    SFileReadFile(hFile, FirstBytes, sizeof(FirstBytes), &dwBytesRead, NULL);
    SFileSetFilePointer(hFile, dwFilePos, NULL, FILE_BEGIN);

    // If we read at least 8 bytes
    if(dwBytesRead == sizeof(FirstBytes))
    {
        // Make sure that the array is properly BSWAP-ed
        BSWAP_ARRAY32_UNSIGNED(FirstBytes, sizeof(FirstBytes));

        // Try to guess file extension from those 2 DWORDs
        for(size_t i = 0; data2ext[i].szExt != NULL; i++)
        {
            if((FirstBytes[0] & data2ext[i].dwOffset00Mask) == data2ext[i].dwOffset00Data &&
               (FirstBytes[1] & data2ext[i].dwOffset04Mask) == data2ext[i].dwOffset04Data)
            {
                char szPseudoName[20] = "";    

                // Format the pseudo-name
                sprintf(szPseudoName, "File%08u.%s", (unsigned int)(pFileEntry - hf->ha->pFileTable), data2ext[i].szExt);

                // Save the pseudo-name in the file entry as well
                AllocateFileName(hf->ha, pFileEntry, szPseudoName);

                // If the caller wants to copy the file name, do it
                if(szFileName != NULL)
                    strcpy(szFileName, szPseudoName);
                return ERROR_SUCCESS;
            }
        }
    }

    return ERROR_NOT_SUPPORTED;
}

bool WINAPI SFileGetFileName(HANDLE hFile, char * szFileName)
{
    TMPQFile * hf = (TMPQFile *)hFile;  // MPQ File handle
    TCHAR * szFileNameT;
    int nError = ERROR_INVALID_HANDLE;

    // Pre-zero the output buffer
    if(szFileName != NULL)
        *szFileName = 0;

    // Check valid parameters
    if(IsValidFileHandle(hFile))
    {
        TFileEntry * pFileEntry = hf->pFileEntry;

        // For MPQ files, retrieve the file name from the file entry
        if(hf->pStream == NULL)
        {
            if(pFileEntry != NULL)
            {
                // If the file name is not there yet, create a pseudo name
                if(pFileEntry->szFileName == NULL)
                {
                    nError = CreatePseudoFileName(hFile, pFileEntry, szFileName);
                }
                else
                {
                    if(szFileName != NULL)
                        strcpy(szFileName, pFileEntry->szFileName);
                    nError = ERROR_SUCCESS;
                }
            }
        }

        // For local files, copy the file name from the stream
        else
        {
            if(szFileName != NULL)
            {
                szFileNameT = FileStream_GetFileName(hf->pStream);
                CopyFileName(szFileName, szFileNameT, _tcslen(szFileNameT));
            }
            nError = ERROR_SUCCESS;
        }
    }

    if(nError != ERROR_SUCCESS)
        SetLastError(nError);
    return (nError == ERROR_SUCCESS);
}

//-----------------------------------------------------------------------------
// Retrieves an information about an archive or about a file within the archive
//
//  hMpqOrFile - Handle to an MPQ archive or to a file
//  dwInfoType - Information to obtain

bool WINAPI SFileGetFileInfo(
    HANDLE hMpqOrFile,
    DWORD dwInfoType,
    void * pvFileInfo,
    DWORD cbFileInfo,
    LPDWORD pcbLengthNeeded)
{
    TMPQArchive * ha = NULL;
    TMPQBlock * pBlockTable = NULL;
    ULONGLONG Int64Value = 0;
    TMPQFile * hf = NULL;
    TCHAR * szPatchChain = NULL;
    void * pvSrcFileInfo = NULL;
    DWORD cbSrcFileInfo = 0;
    DWORD dwInt32Value = 0;
    int nError = ERROR_INVALID_PARAMETER;

    switch(dwInfoType)
    {
        case SFILE_INFO_ARCHIVE_NAME:
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = FileStream_GetFileName(ha->pStream);
                cbSrcFileInfo = (DWORD)(_tcslen((TCHAR *)pvSrcFileInfo) + 1) * sizeof(TCHAR);
            }
            break;

        case SFILE_INFO_ARCHIVE_SIZE:       // Size of the archive
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = &ha->pHeader->dwArchiveSize;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_MAX_FILE_COUNT:     // Max. number of files in the MPQ
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = &ha->dwMaxFileCount;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_HASH_TABLE_SIZE:    // Size of the hash table
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = &ha->pHeader->dwHashTableSize;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_BLOCK_TABLE_SIZE:   // Size of the block table
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = &ha->pHeader->dwBlockTableSize;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_SECTOR_SIZE:
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = &ha->dwSectorSize;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_HASH_TABLE:
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = ha->pHashTable;
                cbSrcFileInfo = ha->pHeader->dwHashTableSize * sizeof(TMPQHash);
            }
            break;

        case SFILE_INFO_BLOCK_TABLE:
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = pBlockTable = TranslateBlockTable(ha, &Int64Value, NULL);
                cbSrcFileInfo = (DWORD)(Int64Value / sizeof(TMPQBlock));
            }
            break;

        case SFILE_INFO_NUM_FILES:
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = &dwInt32Value;
                cbSrcFileInfo = sizeof(DWORD);
                dwInt32Value = GetMpqFileCount(ha);
            }
            break;

        case SFILE_INFO_STREAM_FLAGS:
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                FileStream_GetFlags(ha->pStream, &dwInt32Value);
                pvSrcFileInfo = &dwInt32Value;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_IS_READ_ONLY:
            ha = IsValidMpqHandle(hMpqOrFile);
            if(ha != NULL)
            {
                pvSrcFileInfo = &dwInt32Value;
                cbSrcFileInfo = sizeof(DWORD);
                dwInt32Value = (FileStream_IsReadOnly(ha->pStream) || (ha->dwFlags & MPQ_FLAG_READ_ONLY));
            }
            break;

        case SFILE_INFO_HASH_INDEX:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                pvSrcFileInfo = &hf->pFileEntry->dwHashIndex;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_CODENAME1:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL && hf->ha != NULL && hf->ha->pHashTable != NULL)
            {
                pvSrcFileInfo = &ha->pHashTable[hf->pFileEntry->dwHashIndex].dwName1;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_CODENAME2:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL && hf->ha != NULL && hf->ha->pHashTable != NULL)
            {
                pvSrcFileInfo = &ha->pHashTable[hf->pFileEntry->dwHashIndex].dwName2;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_LOCALEID:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                pvSrcFileInfo = &dwInt32Value;
                cbSrcFileInfo = sizeof(DWORD);
                dwInt32Value = hf->pFileEntry->lcLocale;
            }
            break;

        case SFILE_INFO_BLOCKINDEX:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL && hf->ha != NULL)
            {
                pvSrcFileInfo = &dwInt32Value;
                cbSrcFileInfo = sizeof(DWORD);
                dwInt32Value = (DWORD)(hf->pFileEntry - hf->ha->pFileTable);
            }
            break;

        case SFILE_INFO_FILE_SIZE:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                pvSrcFileInfo = &hf->pFileEntry->dwFileSize;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_COMPRESSED_SIZE:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                pvSrcFileInfo = &hf->pFileEntry->dwCmpSize;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_FLAGS:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                pvSrcFileInfo = &hf->pFileEntry->dwFlags;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_POSITION:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                pvSrcFileInfo = &hf->pFileEntry->ByteOffset;
                cbSrcFileInfo = sizeof(ULONGLONG);
            }
            break;

        case SFILE_INFO_KEY:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                pvSrcFileInfo = &hf->dwFileKey;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_KEY_UNFIXED:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                dwInt32Value = hf->dwFileKey;
                if(hf->pFileEntry->dwFlags & MPQ_FILE_FIX_KEY)
                    dwInt32Value = (dwInt32Value ^ hf->pFileEntry->dwFileSize) - (DWORD)hf->MpqFilePos;
                pvSrcFileInfo = &dwInt32Value;
                cbSrcFileInfo = sizeof(DWORD);
            }
            break;

        case SFILE_INFO_FILETIME:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
            {
                pvSrcFileInfo = &hf->pFileEntry->FileTime;
                cbSrcFileInfo = sizeof(ULONGLONG);
            }
            break;

        case SFILE_INFO_PATCH_CHAIN:
            hf = IsValidFileHandle(hMpqOrFile);
            if(hf != NULL)
                pvSrcFileInfo = szPatchChain = GetFilePatchChain(hf, &cbSrcFileInfo);
            break;
    }

    // Check if one of the cases case yielded a result
    if(pvSrcFileInfo != NULL && pvFileInfo != NULL)
    {
        // Give the length needed
        if(pcbLengthNeeded != NULL)
            pcbLengthNeeded[0] = cbSrcFileInfo;

        // Verify if we have enough space in the output buffer
        if(cbSrcFileInfo <= cbFileInfo)
        {
            memcpy(pvFileInfo, pvSrcFileInfo, cbSrcFileInfo);
            nError = ERROR_SUCCESS;
        }
        else
        {
            nError = ERROR_INSUFFICIENT_BUFFER;
        }
    }

    // Free the allocated buffers, if any
    if(szPatchChain != NULL)
        STORM_FREE(szPatchChain);
    if(pBlockTable != NULL)
        STORM_FREE(pBlockTable);

    // Set the last error value, if needed
    if(nError != ERROR_SUCCESS)
        SetLastError(nError);
    return (nError == ERROR_SUCCESS);
}
